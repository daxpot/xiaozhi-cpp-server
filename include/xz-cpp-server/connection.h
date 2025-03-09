#pragma once
#include "xz-cpp-server/config/setting.h"
#include "xz-cpp-server/silero_vad/vad.h"
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace websocket = boost::beast::websocket;

namespace xiaozhi {
    class Connection {
        private:
            std::shared_ptr<Setting> setting = nullptr;
            std::string session_id;
            Vad vad;
            net::awaitable<void> handle_text(websocket::stream<beast::tcp_stream> &ws, beast::flat_buffer &buffer);
            net::awaitable<void> handle_binary(websocket::stream<beast::tcp_stream> &ws, beast::flat_buffer &buffer);
        public:
            Connection(std::shared_ptr<Setting> setting, std::string session_id);
            net::awaitable<void> handle(websocket::stream<beast::tcp_stream> &ws);
    };
}