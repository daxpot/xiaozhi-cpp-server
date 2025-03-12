#pragma once
#include "xz-cpp-server/config/setting.h"
#include "xz-cpp-server/silero_vad/vad.h"
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <vector>
namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;

namespace xiaozhi {
    class Connection {
        private:
            std::shared_ptr<Setting> setting = nullptr;
            std::string session_id;
            Vad vad;
            net::awaitable<void> handle_text(websocket::stream<beast::tcp_stream> &ws, beast::flat_buffer &buffer);
            net::awaitable<void> handle_binary(websocket::stream<beast::tcp_stream> &ws, beast::flat_buffer &buffer);
            net::awaitable<void> send_welcome(websocket::stream<beast::tcp_stream> &ws);
            std::vector<beast::flat_buffer> bufs;
            boost::asio::steady_timer timer;
        public:
            Connection(std::shared_ptr<Setting> setting, net::any_io_executor executor);
            net::awaitable<void> handle(websocket::stream<beast::tcp_stream> &ws);
    };
}