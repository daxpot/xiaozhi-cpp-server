#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/log/trivial.hpp>
#include <optional>
#include <string>
#include <xz-cpp-server/asr/doubao.h>
#include <xz-cpp-server/common/root_certificates.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <nlohmann/json.hpp>
#include <xz-cpp-server/common/tools.h>
namespace http = boost::beast::http;

std::string gzip_compress(const std::string &data) {
    std::stringstream compressed;
    std::stringstream origin(data);
    boost::iostreams::filtering_streambuf <boost::iostreams::input> out;
    out.push(boost::iostreams::gzip_compressor());
    out.push(origin);
    boost::iostreams::copy(out, compressed);
    return compressed.str();
}

std::string gzip_decompress(const std::string &data) {
    std::stringstream compressed(data);
    std::stringstream decompressed;

    boost::iostreams::filtering_streambuf <boost::iostreams::input> out;
    out.push(boost::iostreams::gzip_decompressor());
    out.push(compressed);
    boost::iostreams::copy(out, decompressed);

    return decompressed.str();
}

// Helper to construct the header as per protocol (big-endian)
std::vector<uint8_t> make_header(uint8_t msg_type, uint8_t flags, bool json_serialization, bool gzip_compression) {
    std::vector<uint8_t> header(4);
    header[0] = (0x1 << 4) | 0x1; // Protocol version 1, Header size 4 bytes
    header[1] = (msg_type << 4) | flags; // Message type and flags
    header[2] = (json_serialization ? 0x1 : 0x0) << 4 | (gzip_compression ? 0x1 : 0x0); // Serialization and compression
    header[3] = 0x00; // Reserved
    return header;
}

namespace xiaozhi {
    net::awaitable<std::shared_ptr<DoubaoASR>> DoubaoASR::createInstance() {
        auto setting = xiaozhi::Setting::getSetting();
        auto executor = co_await net::this_coro::executor;
        
        // The SSL context is required, and holds certificates
        ssl::context ctx{ssl::context::tlsv12_client};
    
        // This holds the root certificate used for verification
        // load_root_certificates(ctx);
        ctx.set_verify_mode(ssl::verify_peer);
        ctx.set_default_verify_paths();
    
        co_return std::make_shared<xiaozhi::DoubaoASR>(setting, executor, ctx);
    }

    DoubaoASR::DoubaoASR(std::shared_ptr<Setting> setting, net::any_io_executor &executor, ssl::context &ctx): 
        appid(setting->config["ASR"]["DoubaoASR"]["appid"].as<std::string>()),
        access_token(setting->config["ASR"]["DoubaoASR"]["access_token"].as<std::string>()),
        cluster(setting->config["ASR"]["DoubaoASR"]["cluster"].as<std::string>()),
        resolver(net::make_strand(executor)),
        ws(net::make_strand(executor), ctx) {

    }

    void DoubaoASR::connect() {
        if(is_connecting) {
            return;
        }
        is_connecting = true;
        resolver.async_resolve(host,
            port,
            beast::bind_front_handler(&DoubaoASR::on_resolve, shared_from_this())
        );
    }

    void DoubaoASR::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
        if(ec) {
            clear("DoubaoASR resolve:", ec);
            return;
        }
        beast::get_lowest_layer(ws).async_connect(results, beast::bind_front_handler(&DoubaoASR::on_connect, shared_from_this()));
    }

    void DoubaoASR::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
        if(ec) {
            clear("DoubaoASR connect:", ec);
            return;
        }
        // Set a timeout on the operation
        beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(30));
        if(!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
            ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                net::error::get_ssl_category());
            clear("DoubaoASR ssl:", ec);
            return;
        }
        ws.next_layer().async_handshake(ssl::stream_base::client,
            beast::bind_front_handler(&DoubaoASR::on_ssl_handshake, shared_from_this())
        );
    }

    void DoubaoASR::on_ssl_handshake(beast::error_code ec) {
        if(ec) {
            clear("DoubaoASR ssl handshake:", ec);
            return;
        }
        beast::get_lowest_layer(ws).expires_never();
        ws.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::client));
        ws.set_option(websocket::stream_base::decorator(
            [&token = access_token](websocket::request_type& req) {
                // req.set(http::field::user_agent, "beast/1.0.0");
                req.set(http::field::accept, "*/*");
                req.set(http::field::authorization, std::string("Bearer; ") + token);
            }));
        ws.async_handshake(host + ':' + port, path,
            beast::bind_front_handler(
                &DoubaoASR::on_handshake,
                shared_from_this()));
    }
    void DoubaoASR::on_handshake(beast::error_code ec) {
        if(ec) {
            clear("DoubaoASR handshake:", ec);
            return;
        }
        auto uuid = tools::generate_uuid();
        nlohmann::json obj = {
            {"app", {
                    {"appid", appid},
                    {"token", access_token},
                    {"cluster", cluster}
                }
            }, {
                "user", {
                    {"uid", "388808088185088"}
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
                    {"reqid", "a3273f8ee3db11e7bf2ff3223ff33638"},
                    {"show_utterances", false},
                    {"sequence", 1}
                }
            }
        };
        std::string payload_str = obj.dump();
        std::vector<uint8_t> payload(payload_str.begin(), payload_str.end());
        // payload = gzip_compress(payload);
        
        auto data = make_header(0x1, 0x0, true, false); // Full client request, JSON, Gzip
        uint32_t payload_size = payload.size();
        data.insert(data.end(), {(uint8_t)(payload_size >> 24), (uint8_t)(payload_size >> 16),
                               (uint8_t)(payload_size >> 8), (uint8_t)payload_size}); // Big-endian size
        data.insert(data.end(), payload.begin(), payload.end());
        BOOST_LOG_TRIVIAL(info) << "data:" << data.size();
        // ws.binary(true);
        ws.async_write(net::buffer(data), beast::bind_front_handler(&DoubaoASR::on_full_client, shared_from_this()));
    }

    void DoubaoASR::on_full_client(beast::error_code ec, std::size_t bytes_transferred) {
        if(ec) {
            clear("DoubaoASR send full client:", ec);
            return;
        }
        is_connecting = false;
        is_connected = true;
        // send_cache();
        // buffer.consume(buffer.size());

        ws.async_read(buffer, beast::bind_front_handler(&DoubaoASR::on_read, shared_from_this()));
        BOOST_LOG_TRIVIAL(info) << "write:" << bytes_transferred << ":" << ws.is_open();
    }

    void DoubaoASR::send_opus(std::optional<beast::flat_buffer> buf) {
        if(!is_connected || is_sending) {
            caches.push(buf);
            return;
        }
        is_sending = true;
        std::string data = "\x11";
        if(buf == std::nullopt) {
            data.push_back(0b00100010);
        } else {
            data.push_back(0b00100000);
        }
        data.push_back(0b00000000); //raw : no compress
        data.push_back(0b00000000); //reserved
        std::string payload = "";
        if(buf != std::nullopt) {
            payload = beast::buffers_to_string(buf->data());
        }
        auto payload_len_big = boost::endian::native_to_big<uint32_t>(payload.size());
        data.append((const char *) &payload_len_big, sizeof(uint32_t));
        data.append(payload.data(), payload.size());
        BOOST_LOG_TRIVIAL(info) << "DoubaoASR send_opus:" << data.size();
        ws.binary(true);
        if(buf == std::nullopt) {
            ws.async_write(net::buffer(data), beast::bind_front_handler(&DoubaoASR::on_full_server, shared_from_this()));
        } else {
            ws.async_write(net::buffer(data), beast::bind_front_handler(&DoubaoASR::on_send_opus, shared_from_this()));
        }
    }

    void DoubaoASR::on_send_opus(beast::error_code ec, std::size_t bytes_transferred) {
        if(ec) {
            clear("DoubaoASR on send opus:", ec);
            return;
        }
        is_sending = false;
        send_cache();
    }

    void DoubaoASR::send_cache() {
        if(!caches.empty()) {
            auto buf = caches.front();
            caches.pop();
            send_opus(buf);
        }
    }

    void DoubaoASR::on_full_server(beast::error_code ec, std::size_t bytes_transferred) {
        if(ec) {
            clear("DoubaoASR on send opus end:", ec);
            return;
        }
        buffer.consume(buffer.size());
        ws.async_read(buffer, beast::bind_front_handler(&DoubaoASR::on_read, shared_from_this()));
    }

    void DoubaoASR::on_read(beast::error_code ec, std::size_t bytes_transferred) {
        if(ec) {
            clear("DoubaoASR on read:", ec);
            return;
        }
        BOOST_LOG_TRIVIAL(info) << "read:" << beast::make_printable(buffer.data());
    }

    void DoubaoASR::close() {
        ws.async_close(websocket::close_code::normal, beast::bind_front_handler(
            &DoubaoASR::on_close,
            shared_from_this()));
    }

    void DoubaoASR::on_close(beast::error_code ec) {
        if(ec) {
            clear("DoubaoASR on close:", ec);
            return;
        }
    }

    void DoubaoASR::clear(std::string title, beast::error_code ec) {
        is_connected = false;
        is_connecting = false;
        is_sending = false;
        while(!caches.empty()) {
            caches.pop();
        }
        if(ec) {
            BOOST_LOG_TRIVIAL(error) << title << ec.message();
        }
    }
}