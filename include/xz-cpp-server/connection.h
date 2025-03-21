#pragma once
#include <atomic>
#include <boost/asio/awaitable.hpp>
#include <vector>
#include <xz-cpp-server/config/setting.h>
#include <xz-cpp-server/silero_vad/vad.h>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <xz-cpp-server/asr/base.h>
#include <xz-cpp-server/llm/base.h>
#include <xz-cpp-server/common/threadsafe_queue.hpp>
namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;

namespace xiaozhi {
    class Connection: public std::enable_shared_from_this<Connection> {
        private:
            std::atomic<bool> is_released_ = false;
            int min_silence_tms_ = 700;
            int close_connection_no_voice_time_ = 120;
            std::shared_ptr<Setting> setting_ = nullptr;
            std::string session_id_;
            Vad vad_;
            net::any_io_executor executor_;
            std::vector<llm::Dialogue> dialogue_;
            ThreadSafeQueue<std::string> llm_response_;
            std::vector<std::string> cmd_exit_;
            
            boost::asio::steady_timer silence_timer_;
            std::unique_ptr<asr::Base> asr_ = nullptr;
            std::unique_ptr<llm::Base> llm_ = nullptr;
            websocket::stream<beast::tcp_stream> ws_;

            void audio_silence_end(const boost::system::error_code& ec);
            net::awaitable<void> on_asr_detect(std::string);
            net::awaitable<void> handle_text(beast::flat_buffer &buffer);
            net::awaitable<void> handle_binary(beast::flat_buffer &buffer);
            net::awaitable<void> send_welcome();
            
            void push_llm_response(std::string str);
            net::awaitable<void> tts_loop();
        public:
            Connection(std::shared_ptr<Setting> setting, websocket::stream<beast::tcp_stream> ws, net::any_io_executor executor);
            ~Connection();
            net::awaitable<void> handle();
    };
}