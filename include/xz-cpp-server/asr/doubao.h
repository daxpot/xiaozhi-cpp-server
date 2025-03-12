#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <memory>
#include <queue>
#include <xz-cpp-server/config/setting.h>
using tcp = boost::asio::ip::tcp;
namespace net = boost::asio;
namespace websocket = boost::beast::websocket;
namespace beast = boost::beast;
namespace ssl = net::ssl;

const std::string host{"openspeech.bytedance.com"};
const std::string port{"443"};
const std::string path{"/api/v2/asr"};

namespace xiaozhi {
    class DoubaoASR: public std::enable_shared_from_this<DoubaoASR> {
        private:
            bool is_connecting = false;
            bool is_connected = false;
            bool is_sending = false;
            websocket::stream<ssl::stream<beast::tcp_stream>> ws;
            const std::string appid;
            const std::string access_token;
            const std::string cluster;
            beast::flat_buffer buffer;
            std::queue<std::optional<beast::flat_buffer>> caches;
            tcp::resolver resolver;
            void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
            void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
            void on_ssl_handshake(beast::error_code ec);
            void on_handshake(beast::error_code ec);
            void on_full_client(beast::error_code ec, std::size_t bytes_transferred);
            void on_read(beast::error_code ec, std::size_t bytes_transferred);
            void on_close(beast::error_code ec);
            void on_send_opus(beast::error_code ec, std::size_t bytes_transferred);
            void clear(std::string title, beast::error_code ec);
            void send_cache();
            void on_full_server(beast::error_code ec, std::size_t bytes_transferred);
        public:
            DoubaoASR(std::shared_ptr<Setting> setting, net::any_io_executor &executor, ssl::context &ctx);
            void connect();
            void close();
            void send_opus(std::optional<beast::flat_buffer> buf);
            static net::awaitable<std::shared_ptr<DoubaoASR>> createInstance();
    };
}