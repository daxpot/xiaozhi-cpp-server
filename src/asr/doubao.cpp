#include <boost/asio/awaitable.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/log/trivial.hpp>
#include <memory>
#include <optional>
#include <string>
#include <xz-cpp-server/asr/doubao.h>
#include <nlohmann/json.hpp>
#include <xz-cpp-server/common/tools.h>
#include <xz-cpp-server/config/setting.h>
using tcp = net::ip::tcp;
namespace ssl = net::ssl;
namespace websocket = beast::websocket;
namespace http = beast::http;

const std::string host{"openspeech.bytedance.com"};
const std::string port{"443"};
const std::string path{"/api/v2/asr"};

// Helper to construct the header as per protocol (big-endian)
std::vector<uint8_t> make_header(uint8_t msg_type, uint8_t flags, bool json_serialization, bool gzip_compression) {
    std::vector<uint8_t> header(4);
    header[0] = (0x1 << 4) | 0x1; // Protocol version 1, Header size 4 bytes
    header[1] = (msg_type << 4) | flags; // Message type and flags
    header[2] = (json_serialization ? 0x1 : 0x0) << 4 | (gzip_compression ? 0x1 : 0x0); // Serialization and compression
    header[3] = 0x00; // Reserved
    return header;
}

void parse_reponse(std::string payload, bool gzip_compression) {

}

namespace xiaozhi {
    net::awaitable<std::shared_ptr<DoubaoASR>> DoubaoASR::createInstance() {
        auto executor = co_await net::this_coro::executor;
        co_return std::make_shared<DoubaoASR>(executor);
    }
    DoubaoASR::DoubaoASR(net::any_io_executor &executor) {
        auto setting = xiaozhi::Setting::getSetting();
        impl_ = std::make_unique<ASRImpl>(setting, executor);
    }
    
    class DoubaoASR::ASRImpl {
        public:
            net::any_io_executor executor_; //需要比resolver和ws先初始化，所以申明在前面
        private:
            bool is_connecting_ = false;
            bool is_connected_ = false;
            bool is_sending_ = false;
            const std::string appid_;
            const std::string access_token_;
            const std::string cluster_;
            tcp::resolver resolver_;
            ssl::context ctx_;
            websocket::stream<ssl::stream<beast::tcp_stream>> ws_;
        public:
            ASRImpl(std::shared_ptr<Setting> setting, net::any_io_executor &executor):
                executor_(executor),
                appid_(setting->config["ASR"]["DoubaoASR"]["appid"].as<std::string>()),
                access_token_(setting->config["ASR"]["DoubaoASR"]["access_token"].as<std::string>()),
                cluster_(setting->config["ASR"]["DoubaoASR"]["cluster"].as<std::string>()),
                resolver_(executor_),
                ctx_(ssl::context::sslv23_client),
                ws_(executor_, ctx_) {
                    ctx_.set_verify_mode(ssl::verify_peer);
                    ctx_.set_default_verify_paths();
            }
            net::awaitable<void> connect() {
                if(is_connecting_) {
                    co_return;
                }
                is_connecting_ = true;
                co_await start();
            }
            void close() {

            }
            void send_opus(std::optional<beast::flat_buffer> &buf) {
        
            }
        private:
            void clear(std::string title, beast::error_code ec) {
                is_connected_ = false;
                is_connecting_ = false;
                is_sending_ = false;
                if(ec) {
                    BOOST_LOG_TRIVIAL(error) << title << ec.message();
                }
            }
            net::awaitable<void> start() {
                auto results = co_await resolver_.async_resolve(host, port, net::use_awaitable);
                BOOST_LOG_TRIVIAL(debug) << "resolve ok:";

                auto ep = co_await beast::get_lowest_layer(ws_).async_connect(results, net::use_awaitable);
                BOOST_LOG_TRIVIAL(debug) << "connect ok";
                // Set a timeout on the operation
                beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
                if(!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host.c_str())) {
                    auto ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                        net::error::get_ssl_category());
                    clear("DoubaoASR ssl:", ec);
                    co_return;
                }
                BOOST_LOG_TRIVIAL(debug) << "tlsext host name ok";
                co_await ws_.next_layer().async_handshake(ssl::stream_base::client, net::use_awaitable);
                BOOST_LOG_TRIVIAL(debug) << "ssl handshake ok";
                beast::get_lowest_layer(ws_).expires_never();
                ws_.set_option(
                    websocket::stream_base::timeout::suggested(
                        beast::role_type::client));
                ws_.set_option(websocket::stream_base::decorator(
                    [&token = access_token_](websocket::request_type& req) {
                        // req.set(http::field::user_agent, "beast/1.0.0");
                        req.set(http::field::accept, "*/*");
                        req.set(http::field::authorization, std::string("Bearer; ") + token);
                    }));
                co_await ws_.async_handshake(host + ':' + port, path, net::use_awaitable);
                co_await send_full_client();
            }
            net::awaitable<void> send_full_client() {
                auto uuid = tools::generate_uuid();
                nlohmann::json obj = {
                    {"app", {
                            {"appid", appid_},
                            {"token", access_token_},
                            {"cluster", cluster_}
                        }
                    }, {
                        "user", {
                            {"uid", uuid}
                        }
                    }, {
                        "audio", {
                            {"format", "wav"},
                            {"rate", 16000},
                            {"bits", 16},
                            {"channel", 1},
                            {"codec", "raw"},
                            {"language", "zh-CN"}
                        }
                    }, {
                        "request", {
                            {"reqid", uuid},
                            {"show_utterances", false},
                            {"sequence", 1}
                        }
                    }
                };
                std::string payload_str = obj.dump();
                payload_str = tools::gzip_compress(payload_str);
                std::vector<uint8_t> payload(payload_str.begin(), payload_str.end());
                
                auto data = make_header(0x1, 0x0, true, true); // Full client request, JSON, Gzip
                uint32_t payload_size = payload.size();
                data.insert(data.end(), {(uint8_t)(payload_size >> 24), (uint8_t)(payload_size >> 16),
                                        (uint8_t)(payload_size >> 8), (uint8_t)payload_size}); // Big-endian size
                data.insert(data.end(), payload.begin(), payload.end());
                BOOST_LOG_TRIVIAL(debug) << "data:" << data.size();
                // ws_.binary(true);
                auto [ec, bytes_transferred] = co_await ws_.async_write(net::buffer(data), net::as_tuple(net::use_awaitable));
                if(ec) {
                    clear("DoubaoASR send full client:", ec);
                    co_return;
                }
                beast::flat_buffer buffer;
                std::tie(ec, bytes_transferred) = co_await ws_.async_read(buffer, net::as_tuple(net::use_awaitable));
                if(ec) {
                    clear("DoubaoASR recv full server:", ec);
                    co_return;
                }
                BOOST_LOG_TRIVIAL(debug) << "read full server:" << buffer.data().size();
                is_connecting_ = false;
                is_connected_ = true;
                // payload_str = beast::buffers_to_string(buffer);
            }
    };
    
    void DoubaoASR::connect() {
        net::co_spawn(impl_->executor_, [self = shared_from_this()]() {return self->impl_->connect();}, 
        [](std::exception_ptr e) {
            if(e) {
                try {
                    std::rethrow_exception(e);
                } catch(std::exception& e) {
                    BOOST_LOG_TRIVIAL(error) << "DoubaoASR error:" << e.what();
                }
            }
        });
    }
    void DoubaoASR::close() {
        return impl_->close();
    }
    void DoubaoASR::send_opus(std::optional<beast::flat_buffer> &buf) {
        return impl_->send_opus(buf);
    }

}
//     void DoubaoASR::send_opus(std::optional<beast::flat_buffer> buf) {
//         if(!is_connected_ || is_sending_) {
//             caches.push(buf);
//             return;
//         }
//         is_sending_ = true;
//         std::string data = "\x11";
//         if(buf == std::nullopt) {
//             data.push_back(0b00100010);
//         } else {
//             data.push_back(0b00100000);
//         }
//         data.push_back(0b00000000); //raw : no compress
//         data.push_back(0b00000000); //reserved
//         std::string payload = "";
//         if(buf != std::nullopt) {
//             payload = beast::buffers_to_string(buf->data());
//         }
//         auto payload_len_big = boost::endian::native_to_big<uint32_t>(payload.size());
//         data.append((const char *) &payload_len_big, sizeof(uint32_t));
//         data.append(payload.data(), payload.size());
//         BOOST_LOG_TRIVIAL(info) << "DoubaoASR send_opus:" << data.size();
//         // ws.binary(true);
//         // if(buf == std::nullopt) {
//         //     ws.async_write(net::buffer(data), beast::bind_front_handler(&DoubaoASR::on_full_server, shared_from_this()));
//         // } else {
//         //     ws.async_write(net::buffer(data), beast::bind_front_handler(&DoubaoASR::on_send_opus, shared_from_this()));
//         // }
//     }
