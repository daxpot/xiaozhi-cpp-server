#include <boost/asio/awaitable.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/make_printable.hpp>
#include <boost/beast/http/field.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <xz-cpp-server/config/setting.h>
#include <xz-cpp-server/tts/bytedancev3.h>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/log/trivial.hpp>
#include <xz-cpp-server/common/tools.h>
#include <nlohmann/json.hpp>
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace ssl = net::ssl;
namespace websocket = beast::websocket;

const std::string host{"openspeech.bytedance.com"};
const std::string port{"443"};
const std::string path{"/api/v3/tts/bidirection"};
const bool is_gzip = false; //设置为true返回的也是未压缩的，索性直接不压缩了

// Helper to construct the header as per protocol (big-endian)
std::vector<uint8_t> make_header(bool json_serialization, bool gzip_compression) {
    std::vector<uint8_t> header(4);
    header[0] = (0x1 << 4) | 0x1; // Protocol version 1, Header size 4 bytes
    header[1] = (0x1 << 4) | 0x4; // Message type and flags
    header[2] = (json_serialization ? 0x1 : 0x0) << 4 | (gzip_compression ? 0x1 : 0x0); // Serialization and compression
    header[3] = 0x00; // Reserved
    return header;
}

void payload_insert_big_num(std::vector<uint8_t> &data, uint32_t num) {
    data.insert(data.end(), {(uint8_t)(num >> 24), (uint8_t)(num >> 16),
        (uint8_t)(num >> 8), (uint8_t)num});
}

std::vector<uint8_t> build_payload(int32_t event_code, std::string payload_str, std::string session_id = "") {
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

int32_t parser_response_code(const std::string& payload) {
    int header_len = 4;
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
    class BytedanceV3TTS::TTSImpl {
        public:
            net::any_io_executor executor_; //需要比resolver和ws先初始化，所以申明在前面
        private:
            std::string appid_;
            std::string access_token_;
            std::string voice_;
            std::string uuid_;

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
                    clear("DoubaoASR resolve:", ec);
                    co_return false;
                }
                tcp::endpoint ep;
                std::tie(ec, ep) = co_await beast::get_lowest_layer(ws_).async_connect(results, net::as_tuple(net::use_awaitable));
                if(ec) {
                    clear("DoubaoASR connect:", ec);
                    co_return false;
                }
                // Set a timeout on the operation
                beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
                if(!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host.c_str())) {
                    auto ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                        net::error::get_ssl_category());
                    clear("DoubaoASR ssl:", ec);
                    co_return false;
                }
                std::tie(ec) = co_await ws_.next_layer().async_handshake(ssl::stream_base::client, net::as_tuple(net::use_awaitable));
                if(ec) {
                    clear("DoubaoASR ssl handshake:", ec);
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
                    clear("DoubaoASR handshake:", ec);
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
                nlohmann::json obj = {
                    {"event", EventCodes::StartSession},
                    {"req_params", {
                        {"speaker", voice_},
                        {"audio_params", {
                            {"format", "ogg_opus"},
                            {"sample_rate", 16000}
                        }}
                    }}
                };
                auto data = build_payload(EventCodes::StartSession, obj.dump(), uuid_);
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
                nlohmann::json obj = {
                    {"event", EventCodes::TaskRequest},
                    {"req_params", {
                        {"text", text}
                    }}
                };
                auto data = build_payload(EventCodes::TaskRequest, obj.dump(), uuid_);
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

        public:
            TTSImpl(std::shared_ptr<Setting> setting, net::any_io_executor& executor):
                executor_(executor),
                appid_(setting->config["TTS"]["BytedanceTTS"]["appid"].as<std::string>()),
                access_token_(setting->config["TTS"]["BytedanceTTS"]["access_token"].as<std::string>()),
                voice_(setting->config["TTS"]["BytedanceTTS"]["voice"].as<std::string>()),
                resolver_(executor_),
                ctx_(ssl::context::sslv23_client),
                ws_(executor_, ctx_),
                uuid_(tools::generate_uuid()) {
                    ctx_.set_verify_mode(ssl::verify_peer);
                    ctx_.set_default_verify_paths();
            }

            net::awaitable<std::vector<std::string>> text_to_speak(const std::string& text) {
                std::vector<std::string> audio;
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
                        audio.push_back(data.substr(28));
                    }
                }
                co_await finish_connection();
                co_return audio;
            }
    };
    
    BytedanceV3TTS::BytedanceV3TTS(net::any_io_executor& executor) {
        auto setting = Setting::getSetting();
        impl_ = std::make_unique<TTSImpl>(setting, executor);
    }

    BytedanceV3TTS::~BytedanceV3TTS() = default;

    net::awaitable<std::vector<std::string>> BytedanceV3TTS::text_to_speak(const std::string& text) {
        co_return co_await impl_->text_to_speak(text);
    }
}