#include <boost/asio/awaitable.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/make_printable.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
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
#include <boost/json.hpp>
#include <ogg/ogg.h>
#include <opus/opus.h>
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace ssl = net::ssl;
namespace websocket = beast::websocket;

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

namespace xiaozhi {
    namespace tts {
        class BytedanceV3::Impl {
            private:
                std::string appid_;
                std::string access_token_;
                std::string voice_;
                std::string uuid_;

                net::any_io_executor executor_; //需要比resolver和ws先初始化，所以申明在前面
                tcp::resolver resolver_;
                ssl::context ctx_;
                websocket::stream<ssl::stream<beast::tcp_stream>> ws_;

                void clear() {
                }
                void clear(std::string title) {
                    clear();
                    BOOST_LOG_TRIVIAL(error) << title;
                }
                void clear(std::string title, beast::error_code ec) {
                    clear();
                    if(ec) {
                        BOOST_LOG_TRIVIAL(error) << title << ec.message();
                    }
                }

                net::awaitable<bool> connect() {
                    auto [ec, results] = co_await resolver_.async_resolve(host, port, net::as_tuple(net::use_awaitable));
                    if(ec) {
                        clear("BytedanceTTSV3 resolve:", ec);
                        co_return false;
                    }
                    tcp::endpoint ep;
                    std::tie(ec, ep) = co_await beast::get_lowest_layer(ws_).async_connect(results, net::as_tuple(net::use_awaitable));
                    if(ec) {
                        clear("BytedanceTTSV3 connect:", ec);
                        co_return false;
                    }
                    // Set a timeout on the operation
                    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
                    if(!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host.c_str())) {
                        auto ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                            net::error::get_ssl_category());
                        clear("BytedanceTTSV3 ssl:", ec);
                        co_return false;
                    }
                    std::tie(ec) = co_await ws_.next_layer().async_handshake(ssl::stream_base::client, net::as_tuple(net::use_awaitable));
                    if(ec) {
                        clear("BytedanceTTSV3 ssl handshake:", ec);
                        co_return false;
                    }
                    beast::get_lowest_layer(ws_).expires_never();
                    ws_.set_option(
                        websocket::stream_base::timeout::suggested(
                            beast::role_type::client));
                    ws_.set_option(websocket::stream_base::decorator(
                        [this](websocket::request_type& req) {
                            req.set("X-Api-App-Key", appid_);
                            req.set("X-Api-Access-Key", access_token_);
                            req.set("X-Api-Resource-Id", "volc.service_type.10029");
                            req.set("X-Api-Connect-Id", uuid_);
                        }));
                    std::tie(ec) = co_await ws_.async_handshake(host + ':' + port, path, net::as_tuple(net::use_awaitable));
                    if(ec) {
                        clear("BytedanceTTSV3 handshake:", ec);
                        co_return false;
                    }
                    co_return true;
                }

                net::awaitable<bool> start_connection() {
                    auto data = build_payload(EventCodes::StartConnection, "{}");
                    co_await ws_.async_write(net::buffer(data), net::use_awaitable);

                    beast::flat_buffer buffer;
                    co_await ws_.async_read(buffer, net::use_awaitable);
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
                                {"sample_rate", 16000}
                            }}
                        }}
                    };
                    auto data = build_payload(EventCodes::StartSession, boost::json::serialize(obj), uuid_);
                    co_await ws_.async_write(net::buffer(data), net::use_awaitable);

                    beast::flat_buffer buffer;
                    co_await ws_.async_read(buffer, net::use_awaitable);
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
                    co_await ws_.async_write(net::buffer(data), net::use_awaitable);

                    data = build_payload(EventCodes::FinishSession, "{}", uuid_);
                    co_await ws_.async_write(net::buffer(data), net::use_awaitable);
                }

                net::awaitable<void> finish_connection() {
                    
                    auto data = build_payload(EventCodes::FinishConnection, "{}");
                    co_await ws_.async_write(net::buffer(data), net::use_awaitable);

                    beast::flat_buffer buffer;
                    co_await ws_.async_read(buffer, net::use_awaitable);
                    auto event_code = parser_response_code(beast::buffers_to_string(buffer.data()));
                    if(event_code != EventCodes::ConnectionFinished ) {
                        BOOST_LOG_TRIVIAL(error) << "BytedanceTTS finish connection fail with code:" << event_code << ",data:" << beast::make_printable(buffer.data());
                    }
                }

                // 从 Ogg 数据中提取 Opus 数据包
                std::vector<std::vector<uint8_t>> extract_opus_packets(const std::string& ogg_data) {
                    std::vector<std::vector<uint8_t>> opus_packets;

                    // 初始化 Ogg 流状态
                    ogg_sync_state oy;
                    ogg_sync_init(&oy);

                    // 将 std::string 数据写入 Ogg 同步缓冲区
                    char* buffer = ogg_sync_buffer(&oy, ogg_data.size());
                    memcpy(buffer, ogg_data.data(), ogg_data.size());
                    ogg_sync_wrote(&oy, ogg_data.size());

                    // 分割 Ogg 页面
                    ogg_page og;
                    ogg_stream_state os;
                    bool stream_initialized = false;

                    while (ogg_sync_pageout(&oy, &og) == 1) {
                        if (!stream_initialized) {
                            ogg_stream_init(&os, ogg_page_serialno(&og));
                            stream_initialized = true;
                        }

                        // 将页面提交到流状态
                        if (ogg_stream_pagein(&os, &og) != 0) {
                            throw std::runtime_error("Failed to process Ogg page");
                        }

                        // 检查是否是头部页面（跳过 OpusHead 和 OpusTags）
                        if (ogg_page_bos(&og)) { // Beginning of Stream
                            continue; // 跳过 "OpusHead"
                        }
                        if (strncmp((char*)og.body, "OpusTags", 8) == 0) {
                            continue; // 跳过 "OpusTags"
                        }

                        // 提取数据包
                        ogg_packet op;
                        while (ogg_stream_packetout(&os, &op) == 1) {
                            std::vector<uint8_t> packet(op.bytes);
                            memcpy(packet.data(), op.packet, op.bytes);
                            opus_packets.push_back(std::move(packet));
                        }
                    }

                    // 清理
                    if (stream_initialized) {
                        ogg_stream_clear(&os);
                    }
                    ogg_sync_clear(&oy);

                    return opus_packets;
                }

                std::vector<std::vector<uint8_t>> convert_to_960_frames(const std::vector<std::vector<uint8_t>>& opus_packets_320) {
                    std::vector<std::vector<uint8_t>> opus_packets_960;
                
                    // 初始化 Opus 解码器 (16kHz, 单声道)
                    int error;
                    OpusDecoder* decoder = opus_decoder_create(16000, 1, &error);
                    if (error != OPUS_OK) {
                        BOOST_LOG_TRIVIAL(error) << "BytedanceTTSV3 failed to create opus decoder:" << opus_strerror(error);
                        return opus_packets_960;
                    }
                
                    // 初始化 Opus 编码器 (16kHz, 单声道)
                    OpusEncoder* encoder = opus_encoder_create(16000, 1, OPUS_APPLICATION_AUDIO, &error);
                    if (error != OPUS_OK) {
                        BOOST_LOG_TRIVIAL(error) << "BytedanceTTSV3 failed to create opus encoder:" << opus_strerror(error);
                        opus_decoder_destroy(decoder);
                        return opus_packets_960;
                    }
                
                    // 每 3 个 320 样本帧合并为一个 960 样本帧
                    for (size_t i = 0; i + 2 < opus_packets_320.size(); i += 3) {
                        // 解码 3 个 320 样本帧到 PCM
                        std::vector<int16_t> pcm_960(960); // 目标缓冲区
                        int samples_decoded = 0;
                
                        for (int j = 0; j < 3 && i + j < opus_packets_320.size(); ++j) {
                            const auto& packet = opus_packets_320[i + j];
                            int frame_size = opus_decode(decoder, packet.data(), packet.size(), 
                                                        pcm_960.data() + samples_decoded, 320, 0);
                            if (frame_size < 0) { 
                                BOOST_LOG_TRIVIAL(error) << "BytedanceTTSV3 opus decode failed:" << opus_strerror(frame_size);
                                break;
                            }
                            samples_decoded += frame_size; // 通常是 320
                        }
                
                        if (samples_decoded != 960) {
                            BOOST_LOG_TRIVIAL(error) << "BytedanceTTSV3 not enough samples decoded:" << samples_decoded;
                            continue;
                        }
                
                        // 编码为 960 样本的 Opus 数据包
                        std::vector<uint8_t> opus_packet_960(1275); // 最大缓冲区大小
                        int bytes_written = opus_encode(encoder, pcm_960.data(), 960, 
                                                    opus_packet_960.data(), opus_packet_960.size());
                        if (bytes_written < 0) {
                            BOOST_LOG_TRIVIAL(error) << "BytedanceTTSV3 opus encode failed:" << opus_strerror(bytes_written);
                            continue;
                        }
                
                        opus_packet_960.resize(bytes_written);
                        opus_packets_960.push_back(std::move(opus_packet_960));
                    }
                
                    // 清理
                    opus_decoder_destroy(decoder);
                    opus_encoder_destroy(encoder);
                    return opus_packets_960;
                }
            public:
                Impl(const net::any_io_executor& executor, const YAML::Node& config):
                    executor_(executor),
                    appid_(config["appid"].as<std::string>()),
                    access_token_(config["access_token"].as<std::string>()),
                    voice_(config["voice"].as<std::string>()),
                    resolver_(executor_),
                    ctx_(ssl::context::sslv23_client),
                    ws_(executor_, ctx_),
                    uuid_(tools::generate_uuid()) {
                        ctx_.set_verify_mode(ssl::verify_peer);
                        ctx_.set_default_verify_paths();
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

                    while(true) {
                        beast::flat_buffer buffer;
                        co_await ws_.async_read(buffer, net::use_awaitable);
                        std::string data = beast::buffers_to_string(buffer.data());
                        uint8_t message_type = *(unsigned int *) (data.data() + 2);
                        if(message_type == 0xf0) {
                            BOOST_LOG_TRIVIAL(error) << "BytedanceTTSV3 message type error";
                            break;
                        }
                        auto event_code = parser_response_code(data);
                        if(event_code == EventCodes::SessionFinished) {
                            break;
                        } else if(event_code == EventCodes::TTSResponse) {
                            auto session_id_len = parser_response_code(data, 8);
                            // auto audio_len = parser_response_code(data, 12+session_id_len);
                            // auto session_id = data.substr(12, session_id_len);
                            // BOOST_LOG_TRIVIAL(debug) << "header:" << data.substr(0, 16+session_id_len);
                            // BOOST_LOG_TRIVIAL(debug) << "response:" << data.substr(16+session_id_len);
                            // audio.push_back(data.substr(16+session_id_len));
                            auto opus_packets_320 = extract_opus_packets(data.substr(16+session_id_len));
                            auto opus_packets_960 = convert_to_960_frames(opus_packets_320);
                            audio.insert(audio.end(), opus_packets_960.begin(), opus_packets_960.end());
                        }
                    }
                    co_await finish_connection();
                    co_return audio;
                }
        };
        
        BytedanceV3::BytedanceV3(const net::any_io_executor& executor, const YAML::Node& config) {
            impl_ = std::make_unique<Impl>(executor, config);
        }

        BytedanceV3::~BytedanceV3() = default;

        net::awaitable<std::vector<std::vector<uint8_t>>> BytedanceV3::text_to_speak(const std::string& text) {
            co_return co_await impl_->text_to_speak(text);
        }
    }
}