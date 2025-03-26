#include "xz-cpp-server/common/request.h"
#include "xz-cpp-server/common/tools.h"
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <opus/opus.h>
#include <xz-cpp-server/tts/edge.h>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/log/trivial.hpp>
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace ssl = net::ssl;
namespace websocket = beast::websocket;
using ws_stream = websocket::stream<ssl::stream<beast::tcp_stream>>;

const std::string host{"speech.platform.bing.com"};
const std::string port{"443"};
const std::string TRUSTED_CLIENT_TOKEN{"6A5AA1D4EAFF4E9FB37E23D68491D6F4"};
const constexpr char * path_format = "/consumer/speech/synthesize/readaloud/edge/v1?TrustedClientToken={}&Sec-MS-GEC={}&Sec-MS-GEC-Version=1-130.0.2849.68&ConnectionId={}";


namespace DRM {
    // 常量定义
    const int64_t WIN_EPOCH = 11644473600LL; // Windows 时间纪元偏移 (1601-01-01 到 1970-01-01 的秒数)
    const double S_TO_NS = 1e9;             // 秒到纳秒的转换因子

    class DRM {
    public:
        static double clock_skew_seconds; // 静态成员，用于时钟偏差校正

        // 调整时钟偏差
        static void adj_clock_skew_seconds(double skew_seconds) {
            clock_skew_seconds += skew_seconds;
        }

        // 获取当前 Unix 时间戳（带时钟偏差校正）
        static double get_unix_timestamp() {
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            double seconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1e6;
            return seconds + clock_skew_seconds;
        }

        // 生成 Sec-MS-GEC 令牌
        static std::string generate_sec_ms_gec() {
            // 获取当前时间戳（带时钟偏差校正）
            double ticks = get_unix_timestamp();

            // 转换为 Windows 文件时间纪元
            ticks += WIN_EPOCH;

            // 向下取整到最近的 5 分钟（300 秒）
            ticks -= std::fmod(ticks, 300.0);

            // 转换为 100 纳秒间隔（Windows 文件时间格式）
            int64_t ticks_ns = static_cast<int64_t>(ticks * S_TO_NS / 100);

            // 拼接时间戳和 TRUSTED_CLIENT_TOKEN
            std::ostringstream str_to_hash;
            str_to_hash << ticks_ns << TRUSTED_CLIENT_TOKEN;

            // 计算 SHA256 哈希
            unsigned char hash[SHA256_DIGEST_LENGTH];
            SHA256(reinterpret_cast<const unsigned char*>(str_to_hash.str().c_str()), 
                   str_to_hash.str().length(), hash);

            // 转换为大写十六进制字符串
            std::ostringstream hex_digest;
            hex_digest << std::hex << std::uppercase << std::setfill('0');
            for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
                hex_digest << std::setw(2) << static_cast<int>(hash[i]);
            }

            return hex_digest.str();
        }
    };

    // 初始化静态成员
    double DRM::clock_skew_seconds = 5.0;
}
static std::string date_to_string() {
    // 获取当前 UTC 时间
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm* utc_tm = std::gmtime(&tt); // 转换为 UTC 时间结构体

    // 格式化时间戳
    std::ostringstream oss;
    oss << std::put_time(utc_tm, "%a %b %d %Y %H:%M:%S"); // Thu Mar 20 2025 09:35:43
    oss << " GMT+0000 (Coordinated Universal Time)";       // 添加固定后缀

    return oss.str();
}
namespace xiaozhi {
    namespace tts {
        class Edge::Impl {
            private:
                const int sample_rate_ = 24000; //edge只支持24000khz采样
                
                std::string voice_;
                std::string uuid_;

                net::any_io_executor executor_; //需要比resolver和ws先初始化，所以申明在前面
                std::unique_ptr<ws_stream> ws_;

                net::awaitable<bool> connect() {
                    auto sec_ms_gec = DRM::DRM::generate_sec_ms_gec();
                    std::string path = std::format(path_format, TRUSTED_CLIENT_TOKEN, sec_ms_gec, uuid_);
                    try {
                        auto stream = co_await request::connect({true, host, port, path});
                        ws_ = std::make_unique<ws_stream>(std::move(stream));
                    } catch(const std::exception& e) {
                        BOOST_LOG_TRIVIAL(info) << "Edge tts connect error:" << e.what();
                        co_return false;
                    }
                    beast::get_lowest_layer(*ws_).expires_never();
                    ws_->set_option(
                        websocket::stream_base::timeout::suggested(
                            beast::role_type::client));
                    ws_->set_option(websocket::stream_base::decorator(
                        [this](websocket::request_type& req) {
                            req.set("Origin", "chrome-extension://jdiccldimpahaajbacbfkddppajiklmg");
                        }));
                    auto [ec] = co_await ws_->async_handshake(host + ':' + port, path, net::as_tuple(net::use_awaitable));
                    if(ec) {
                        BOOST_LOG_TRIVIAL(info) << "Edge tts handshake:" << ec.message();
                        co_return false;
                    }
                    co_return true;
                }

                net::awaitable<void> send_command_request() {
                    std::string timestamp = date_to_string();
                    std::ostringstream oss;
                    oss << "X-Timestamp:" << timestamp << "\r\n"
                        << "Content-Type:application/json; charset=utf-8\r\n"
                        << "Path:speech.config\r\n"
                        << "\r\n"
                        << R"({"context":{"synthesis":{"audio":{"metadataoptions":{"sentenceBoundaryEnabled":"false","wordBoundaryEnabled":"false"},"outputFormat":"webm-24khz-16bit-mono-opus"}}}})" << "\r\n";
                    co_await ws_->async_write(net::buffer(oss.str()), net::use_awaitable);
                }
                
                net::awaitable<void> send_ssml_request(const std::string& text) {
                    std::string timestamp = date_to_string();
                    std::ostringstream oss;
                    oss << "X-RequestId:" << uuid_ << "\r\n"
                        << "Content-Type:application/ssml+xml\r\n"
                        << "X-Timestamp:" << timestamp << "\r\n"
                        << "Path:ssml\r\n"
                        << "\r\n"
                        << "<speak version='1.0' xml:lang='en-US'>"
                        << "<voice name='" << voice_ << "'>"
                        << "<prosody pitch='+0Hz' volume='+0%' rate='+0%'>"
                        << text
                        << "</prosody></voice></speak>"
                        << "\r\n";
                    co_await ws_->async_write(net::buffer(oss.str()), net::use_awaitable);
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
                    voice_(config["voice"].as<std::string>()),
                    uuid_(tools::generate_uuid()) {

                }

                net::awaitable<std::vector<std::vector<uint8_t>>> text_to_speak(const std::string& text) {
                    std::vector<std::vector<uint8_t>> audio;
                    bool is_connected = false;
                    int retry = 0;
                    while(!is_connected && retry++ < 3) { //edge偶尔会连接不上，重试3次
                        is_connected = co_await connect();
                    }
                    if(!is_connected) {
                        BOOST_LOG_TRIVIAL(info) << "Edge tts connect max retry";
                        co_return audio;
                    }
                    co_await send_command_request();
                    co_await send_ssml_request(text);
                    
                    auto [encoder, decoder] = tools::create_opus_coders(sample_rate_);
                    if(encoder == nullptr || decoder == nullptr) {
                        co_return audio;
                    }
                    auto frame_size = sample_rate_ / 1000 * 60;

                    std::vector<int16_t> pcm(frame_size);
                    int samples_decoded = 0;
                    bool is_opus = false;
                    while(true) {
                        beast::flat_buffer buffer;
                        co_await ws_->async_read(buffer, net::use_awaitable);
                        if(ws_->got_binary()) {
                            auto packet = static_cast<const unsigned char*>(buffer.data().data());
                            uint32_t header_len = (static_cast<uint32_t>(packet[0]) << 8) | static_cast<uint32_t>(packet[1]);
                            header_len += 2;
                            packet += header_len;
                            auto packet_len = buffer.size() - header_len;
                            if(packet_len < 3) {
                                continue;
                            }
                            uint32_t bin_type = (static_cast<uint32_t>(packet[0]) << 16) | (static_cast<uint32_t>(packet[1]) << 8) | static_cast<uint32_t>(packet[2]);
                            if(bin_type == 0xa3fc81) {
                                is_opus = true;
                                continue;
                            } else if(bin_type == 0xab820c) {
                                is_opus = false;
                                continue;
                            }
                            if(!is_opus) {
                                continue;
                            }
                            packet_len -= 6;
                            int origin_frame_size = opus_packet_get_samples_per_frame(packet, sample_rate_);
                            int decoded_frame_size = opus_decode(decoder, packet, packet_len < 120 ? packet_len : 120, pcm.data() + samples_decoded, origin_frame_size, 0);
                            if (decoded_frame_size < 0) { 
                                BOOST_LOG_TRIVIAL(error) << "Edge tts opus decode failed:" << origin_frame_size << " len:" << buffer.size() - header_len << " error:" << opus_strerror(decoded_frame_size);
                            } else {
                                samples_decoded += decoded_frame_size;
                                if(samples_decoded >= frame_size) {
                                    samples_decoded -= frame_size;
                                    encode_to_audio(encoder, pcm, frame_size, audio);
                                }
                            }
                        } else {
                            std::string data = beast::buffers_to_string(buffer.data());
                            auto p = data.find("Path:turn.end");
                            if(p != data.npos) {
                                break;
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
                    co_return audio;
                }
        };

        
        Edge::Edge(const net::any_io_executor& executor, const YAML::Node& config, int sample_rate) {
            impl_ = std::make_unique<Impl>(executor, config, sample_rate);
        }

        Edge::~Edge() = default;

        net::awaitable<std::vector<std::vector<uint8_t>>> Edge::text_to_speak(const std::string& text) {
            co_return co_await impl_->text_to_speak(text);
        }
    }
}