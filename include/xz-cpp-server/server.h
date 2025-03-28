#pragma once
#include <xz-cpp-server/common/setting.h>

namespace xiaozhi {
    class Server {    
        private:
            net::io_context ioc;
            std::shared_ptr<Setting> setting;
            net::awaitable<void> listen(net::ip::tcp::endpoint endpoint);
            net::awaitable<void> run_session(websocket::stream<beast::tcp_stream> ws);
            net::awaitable<bool> authenticate(websocket::stream<beast::tcp_stream> &ws, http::request<http::string_body> &req);
        public:
            Server(std::shared_ptr<Setting> setting);
            void run();
    };
}