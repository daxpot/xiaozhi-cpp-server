#pragma once
#include <memory>
#include <xz-cpp-server/config/setting.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace websocket = boost::beast::websocket;

namespace xiaozhi {
    class Server {    
        private:
            net::io_context ioc;
            std::shared_ptr<Setting> setting;
            net::awaitable<void> listen(net::ip::tcp::endpoint endpoint);
            net::awaitable<void> run_session(websocket::stream<beast::tcp_stream> ws);
            net::awaitable<bool> authenticate(websocket::stream<beast::tcp_stream> &ws, http::request<http::string_body> &req);
            net::awaitable<std::string> send_welcome(websocket::stream<beast::tcp_stream> &ws);
        public:
            Server(std::shared_ptr<Setting> setting);
            void run();
    };
}