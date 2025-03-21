#include <boost/asio/awaitable.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/make_printable.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <opus/opus_defines.h>
#include <string>
#include <vector>
#include <xz-cpp-server/config/setting.h>
#include <xz-cpp-server/tts/bytedancev3.h>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/log/trivial.hpp>
#include <xz-cpp-server/common/tools.h>
#include <xz-cpp-server/common/request.h>
#include <boost/json.hpp>
#include <ogg/ogg.h>
#include <opus/opus.h>
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace ssl = net::ssl;
namespace websocket = beast::websocket;
using ws_stream = websocket::stream<ssl::stream<beast::tcp_stream>>;

const std::string host{"openspeech.bytedance.com"};
const std::string port{"443"};
const std::string path{"/api/v3/tts/bidirection"};
const bool is_gzip = false; //设置为true返回的也是未压缩的，索性直接不压缩了

// Helper to construct the header as per protocol (big-endian)
static std::vector<uint8_t> make_header(bool json_serialization, bool gzip_compression) {
    std::vector<uint8_t> header(4);
    header[0] = (0x1 << 4) | 0x1; // Protocol version 1, Header size 4 bytes
    header[1] = (0x1 << 4) | 0x4; // Message type and flags
    header[2] = (json_serialization ? 0x1 : 0x0) << 4 | (gzip_compression ? 0x1 : 0x0); // Serialization and compression
    header[3] = 0x00; // Reserved
    return header;
}

static void payload_insert_big_num(std::vector<uint8_t> &data, uint32_t num) {
    data.insert(data.end(), {(uint8_t)(num >> 24), (uint8_t)(num >> 16),
        (uint8_t)(num >> 8), (uint8_t)num});
}

static std::vector<uint8_t> build_payload(int32_t event_code, std::string payload_str, std::string session_id = "") {
    if(is_gzip) {
        payload_str = tools::gzip_compress(payload_str);
    }
    
    auto data = make_header(true, is_gzip); // Full client request, JSON, Gzip
    payload_insert_big_num(data, event_code);

    if(session_id.size() > 0) {
        uint32_t session_size = session_id.size();
        payload_insert_big_num(data, session_size);
        data.insert(data.end(), session_id.begin(), session_id.end());
    }
    
    std::vector<uint8_t> payload(payload_str.begin(), payload_str.end());
    uint32_t payload_size = payload.size();
    payload_insert_big_num(data, payload_size);
    data.insert(data.end(), payload.begin(), payload.end());
    return data;
}

static int32_t parser_response_code(const std::string& payload, int header_len=4) {
    uint32_t event_code = *(unsigned int *) (payload.data() + header_len);
    return boost::endian::big_to_native(event_code);
}

static int32_t parser_response_code(const unsigned char* data, int header_len=4) {
    uint32_t event_code = *(unsigned int *) (data + header_len);
    return boost::endian::big_to_native(event_code);
}

enum EventCodes: int32_t {
    StartConnection = 1,    //1
    FinishConnection = 2,
    ConnectionStarted = 50,
    ConnectionFailed = 51,
    ConnectionFinished = 52,
    StartSession = 100,     //up
    FinishSession = 102,    //up
    SessionStarted = 150,
    SessionCanceled = 151,
    SessionFinished = 152,  //up | down
    SessionFailed = 153,
    TaskRequest = 200,      //up
    TTSSentenceStart = 350,
    TTSSentenceEnd = 351,
    TTSResponse = 352
};

// Ogg页面头部结构（简化版）
struct OggPageHeader {
    uint8_t capture_pattern[4]; // "OggS"
    uint8_t version;
    uint8_t header_type;
    uint64_t granule_position;
    uint32_t serial_number;
    uint32_t page_sequence;
    uint32_t checksum;
    uint8_t segment_count;
};

// 从数据中提取Opus帧
std::vector<std::pair<size_t, size_t>> extractOpusFrames(const unsigned char* data, size_t data_size) {
    std::vector<std::pair<size_t, size_t>> opus_frames_pos; // 存储提取的Opus帧
    size_t offset = 0;

    while (offset < data_size) {
        // 检查是否还有足够的数据读取Ogg页面头部
        if (offset + 27 > data_size) break;

        // 读取Ogg页面头部
        OggPageHeader header;
        memcpy(&header.capture_pattern, data + offset, 4);
        offset += 4;
        header.version = data[offset++];
        header.header_type = data[offset++];
        header.granule_position = *(uint64_t*)(data + offset); // 小端序
        offset += 8;
        header.serial_number = *(uint32_t*)(data + offset); // 小端序
        offset += 4;
        header.page_sequence = *(uint32_t*)(data + offset); // 小端序
        offset += 4;
        header.checksum = *(uint32_t*)(data + offset); // 小端序
        offset += 4;
        header.segment_count = data[offset++];

        // 验证OggS标记
        if (memcmp(header.capture_pattern, "OggS", 4) != 0) {
            BOOST_LOG_TRIVIAL(error) << "BytedanceTTSV3 Invalid OggS pattern at offset " << offset - 27 << std::endl;
            break;
        }

        // 读取段表
        if (offset + header.segment_count > data_size) break;
        std::vector<uint8_t> segment_table(header.segment_count);
        memcpy(segment_table.data(), data + offset, header.segment_count);
        offset += header.segment_count;

        // 计算页面数据总长度
        size_t payload_size = 0;
        for (uint8_t len : segment_table) {
            payload_size += len;
        }

        // 检查数据是否足够
        if (offset + payload_size > data_size) break;

        // 跳过元数据页面（序列号0和1）
        if (header.page_sequence == 0 || header.page_sequence == 1) {
            offset += payload_size; // 跳过OpusHead或OpusTags
            continue;
        }

        // 提取Opus帧（每个段可能是一个完整的Opus帧）
        size_t segment_offset = offset;
        for (uint8_t len : segment_table) {
            if (len > 0) {
                opus_frames_pos.push_back({segment_offset, len});
                segment_offset += len;
            }
        }
        offset += payload_size;
    }

    return opus_frames_pos;
}

namespace xiaozhi {
    namespace tts {
        class BytedanceV3::Impl {
            private:
                int sample_rate_;
                std::string appid_;
                std::string access_token_;
                std::string voice_;
                std::string uuid_;

                net::any_io_executor executor_; //需要比resolver和ws先初始化，所以申明在前面
                std::unique_ptr<ws_stream> ws_;

                void clear(const char* title, beast::error_code ec) {
                    if(ec) {
                        BOOST_LOG_TRIVIAL(error) << title << ec.message();
                    }
                }

                net::awaitable<bool> connect() {
                    try {
                        auto stream = co_await request::connect({true, host, port, path});
                        ws_ = std::make_unique<ws_stream>(std::move(stream));
                    } catch(const std::exception& e) {
                        BOOST_LOG_TRIVIAL(error) << "BytedanceTTSV3 connect error:" << e.what();
                        co_return false;
                    }
                    beast::get_lowest_layer(*ws_).expires_never();
                    ws_->set_option(
                        websocket::stream_base::timeout::suggested(
                            beast::role_type::client));
                    ws_->set_option(websocket::stream_base::decorator(
                        [this](websocket::request_type& req) {
                            req.set("X-Api-App-Key", appid_);
                            req.set("X-Api-Access-Key", access_token_);
                            req.set("X-Api-Resource-Id", "volc.service_type.10029");
                            req.set("X-Api-Connect-Id", uuid_);
                        }));
                    auto [ec] = co_await ws_->async_handshake(host + ':' + port, path, net::as_tuple(net::use_awaitable));
                    if(ec) {
                        clear("BytedanceTTSV3 handshake:", ec);
                        co_return false;
                    }
                    co_return true;
                }

                net::awaitable<bool> start_connection() {
                    auto data = build_payload(EventCodes::StartConnection, "{}");
                    co_await ws_->async_write(net::buffer(data), net::use_awaitable);

                    beast::flat_buffer buffer;
                    co_await ws_->async_read(buffer, net::use_awaitable);
                    auto event_code = parser_response_code(beast::buffers_to_string(buffer.data()));
                    if(event_code != EventCodes::ConnectionStarted) {
                        BOOST_LOG_TRIVIAL(error) << "BytedanceTTS start connection fail with code:" << event_code << ",data:" << beast::make_printable(buffer.data());;
                        co_return false;
                    }
                    co_return true;
                }

                net::awaitable<bool> start_session() {
                    boost::json::object obj = {
                        {"event", EventCodes::StartSession},
                        {"req_params", {
                            {"speaker", voice_},
                            {"audio_params", {
                                {"format", "ogg_opus"},
                                {"sample_rate", sample_rate_}
                            }}
                        }}
                    };
                    auto data = build_payload(EventCodes::StartSession, boost::json::serialize(obj), uuid_);
                    co_await ws_->async_write(net::buffer(data), net::use_awaitable);

                    beast::flat_buffer buffer;
                    co_await ws_->async_read(buffer, net::use_awaitable);
                    auto event_code = parser_response_code(beast::buffers_to_string(buffer.data()));
                    if(event_code != EventCodes::SessionStarted) {
                        BOOST_LOG_TRIVIAL(error) << "BytedanceTTS start session fail with code:" << event_code << ",data:" << beast::make_printable(buffer.data());
                        co_return false;
                    }
                    co_return true;
                }

                net::awaitable<void> task_request(const std::string& text) {
                    boost::json::object obj = {
                        {"event", EventCodes::TaskRequest},
                        {"req_params", {
                            {"text", text}
                        }}
                    };
                    auto data = build_payload(EventCodes::TaskRequest, boost::json::serialize(obj), uuid_);
                    co_await ws_->async_write(net::buffer(data), net::use_awaitable);

                    data = build_payload(EventCodes::FinishSession, "{}", uuid_);
                    co_await ws_->async_write(net::buffer(data), net::use_awaitable);
                }

                net::awaitable<void> finish_connection() {
                    
                    auto data = build_payload(EventCodes::FinishConnection, "{}");
                    co_await ws_->async_write(net::buffer(data), net::use_awaitable);

                    beast::flat_buffer buffer;
                    co_await ws_->async_read(buffer, net::use_awaitable);
                    auto event_code = parser_response_code(beast::buffers_to_string(buffer.data()));
                    if(event_code != EventCodes::ConnectionFinished ) {
                        BOOST_LOG_TRIVIAL(error) << "BytedanceTTS finish connection fail with code:" << event_code << ",data:" << beast::make_printable(buffer.data());
                    }
                }

                void encode_to_audio(OpusEncoder* encoder, std::vector<int16_t>& pcm, int frame_size, std::vector<std::vector<uint8_t>>& audio) {
                    std::vector<uint8_t> opus_packet_target(frame_size*2); // 最大缓冲区大小
                    int bytes_written = opus_encode(encoder, pcm.data(), frame_size, 
                                                opus_packet_target.data(), opus_packet_target.size());
                    if (bytes_written < 0) {
                        BOOST_LOG_TRIVIAL(error) << "BytedanceTTSV3 opus encode failed:" << opus_strerror(bytes_written);
                    } else {
                        opus_packet_target.resize(bytes_written);
                        audio.push_back(std::move(opus_packet_target));
                    }
                }
            public:
                Impl(const net::any_io_executor& executor, const YAML::Node& config, int sample_rate):
                    executor_(executor),
                    appid_(config["appid"].as<std::string>()),
                    access_token_(config["access_token"].as<std::string>()),
                    voice_(config["voice"].as<std::string>()),
                    sample_rate_(sample_rate),
                    uuid_(tools::generate_uuid()) {
                }

                net::awaitable<std::vector<std::vector<uint8_t>>> text_to_speak(const std::string& text) {
                    std::vector<std::vector<uint8_t>> audio;
                    if(co_await connect() == false) {
                        co_return audio;
                    }

                    if(co_await start_connection() == false) {
                        co_return audio;
                    }

                    if(co_await start_session() == false) {
                        co_return audio;
                    }
                    co_await task_request(text);

                    auto [encoder, decoder] = tools::create_opus_coders(sample_rate_);
                    if(encoder == nullptr || decoder == nullptr) {
                        co_return audio;
                    }
                    auto frame_size = sample_rate_ / 1000 * 60;

                    std::vector<int16_t> pcm(frame_size);
                    int samples_decoded = 0;

                    while(true) {
                        beast::flat_buffer buffer;
                        co_await ws_->async_read(buffer, net::use_awaitable);
                        auto data = static_cast<const unsigned char*>(buffer.data().data());
                        uint8_t message_type = data[2];
                        if(message_type == 0xf0) {
                            BOOST_LOG_TRIVIAL(error) << "BytedanceTTSV3 message type error";
                            break;
                        }
                        auto event_code = parser_response_code(data);
                        if(event_code == EventCodes::SessionFinished) {
                            break;
                        } else if(event_code == EventCodes::TTSResponse) {
                            auto session_id_len = parser_response_code(data, 8);
                            auto packet = data + 16 + session_id_len;
                            auto frames_pos = extractOpusFrames(packet, buffer.size() - 16 - session_id_len);
                            for(auto& pos : frames_pos) {
                                int origin_frame_size = opus_packet_get_samples_per_frame(packet + pos.first, sample_rate_);
                                int decoded_frame_size = opus_decode(decoder, packet + pos.first, pos.second, pcm.data() + samples_decoded, origin_frame_size, 0);
                                if (decoded_frame_size < 0) { 
                                    BOOST_LOG_TRIVIAL(error) << "BytedanceTTSV3 opus decode failed:" << origin_frame_size << " len:" << pos.second << " error:" << opus_strerror(decoded_frame_size);
                                } else {
                                    samples_decoded += decoded_frame_size;
                                    if(samples_decoded >= frame_size) {
                                        samples_decoded -= frame_size;
                                        encode_to_audio(encoder, pcm, frame_size, audio);
                                    }
                                }
                            }
                        }
                    }
                    if(samples_decoded > 0) {
                        pcm.resize(samples_decoded);
                        if(samples_decoded < frame_size) {
                            pcm.insert(pcm.end(), frame_size - samples_decoded, 0); //补足60ms
                        }
                        encode_to_audio(encoder, pcm, frame_size, audio);
                    }
                    opus_decoder_destroy(decoder);
                    opus_encoder_destroy(encoder);
                    co_await finish_connection();
                    co_return audio;
                }
        };
        
        BytedanceV3::BytedanceV3(const net::any_io_executor& executor, const YAML::Node& config, int sample_rate) {
            impl_ = std::make_unique<Impl>(executor, config, sample_rate);
        }

        BytedanceV3::~BytedanceV3() = default;

        net::awaitable<std::vector<std::vector<uint8_t>>> BytedanceV3::text_to_speak(const std::string& text) {
            co_return co_await impl_->text_to_speak(text);
        }
    }
}