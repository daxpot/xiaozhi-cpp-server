#include <iostream>
#include <string>
#include <xz-cpp-server/connection.h>

namespace xiaozhi {
    Connection::Connection(std::shared_ptr<xiaozhi::Setting> setting, std::string session_id):setting(setting), session_id(session_id) {

    }
    
    net::awaitable<void> Connection::handle_text(websocket::stream<beast::tcp_stream> &ws, beast::flat_buffer &buffer) {
        std::cout << boost::beast::buffers_to_string(buffer.data()) << std::endl;
        co_return;
    }

    net::awaitable<void> Connection::handle(websocket::stream<beast::tcp_stream> &ws) {
        while(true) {
            beast::flat_buffer buffer;
            auto [ec, _] = co_await ws.async_read(buffer, net::as_tuple(net::use_awaitable));
            if(ec == websocket::error::closed) {
                co_return;
            } else if(ec) {
                throw boost::system::system_error(ec);
            }
            if(ws.got_text()) {
                co_await handle_text(ws, buffer);
            } else if(ws.got_binary()) {
                // std::cout << "got binary:" << buffer.size() << std::endl;
            }
        }
    }
}