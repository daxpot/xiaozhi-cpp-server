#pragma once
#include <boost/asio/awaitable.hpp>
#include <xz-cpp-server/config/setting.h>
#include <xz-cpp-server/silero_vad/vad.h>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <xz-cpp-server/asr/doubao.h>
namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;

namespace xiaozhi {
    class Connection: public std::enable_shared_from_this<Connection> {
        private:
            int min_silence_tms = 700;
            std::shared_ptr<Setting> setting_ = nullptr;
            std::string session_id_;
            Vad vad_;
            boost::asio::steady_timer silence_timer_;
            std::shared_ptr<DoubaoASR> asr_ = nullptr;
            websocket::stream<beast::tcp_stream> ws_;

            void audio_silence_end(const boost::system::error_code& ec);
            net::awaitable<void> handle_text(beast::flat_buffer &buffer);
            net::awaitable<void> handle_binary(beast::flat_buffer &buffer);
            net::awaitable<void> send_welcome();
        public:
            Connection(std::shared_ptr<Setting> setting, websocket::stream<beast::tcp_stream> ws, net::any_io_executor executor);
            net::awaitable<void> handle();
    };
}