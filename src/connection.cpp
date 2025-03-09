#include <boost/beast/core/multi_buffer.hpp>
#include <boost/log/trivial.hpp>
#include <string>
#include <xz-cpp-server/connection.h>
#include <xz-cpp-server/silero_vad/vad.h>

namespace xiaozhi {
    Connection::Connection(std::shared_ptr<Setting> setting, std::string session_id):setting(setting), session_id(session_id) {

    }
    
    net::awaitable<void> Connection::handle_text(websocket::stream<beast::tcp_stream> &ws, beast::flat_buffer &buffer) {
        BOOST_LOG_TRIVIAL(info) << "收到文本消息:" << boost::beast::buffers_to_string(buffer.data());
        co_return;
    }

    net::awaitable<void> Connection::handle_binary(websocket::stream<beast::tcp_stream> &ws, beast::flat_buffer &buffer) {
        auto pcm = vad.check_vad(buffer);
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
                co_await handle_binary(ws, buffer);
            }
        }
    }
}