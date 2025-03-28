#pragma once
#include <xz-cpp-server/common/setting.h>
#include <xz-cpp-server/silero_vad/vad.h>
#include <xz-cpp-server/asr/base.h>
#include <xz-cpp-server/llm/base.h>
#include <xz-cpp-server/tts/base.h>
#include <xz-cpp-server/common/threadsafe_queue.hpp>

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
            boost::json::array dialogue_;
            std::vector<std::string> cmd_exit_;

            ThreadSafeQueue<std::string> llm_response_;
            ThreadSafeQueue<std::optional<beast::flat_buffer>> asr_audio_;
            
            boost::asio::steady_timer silence_timer_;
            std::unique_ptr<asr::Base> asr_ = nullptr;
            std::unique_ptr<llm::Base> llm_ = nullptr;
            std::unique_ptr<tts::Base> tts_ = nullptr;
            websocket::stream<beast::tcp_stream> ws_;
            net::strand<net::any_io_executor> strand_;

            net::awaitable<void> handle_asr_text(std::string);
            net::awaitable<void> handle_text(beast::flat_buffer &buffer);
            net::awaitable<void> handle_binary(beast::flat_buffer &buffer);
            net::awaitable<void> send_welcome();
            
            void push_llm_response(std::string str);
            net::awaitable<void> asr_loop();
            net::awaitable<void> tts_loop();
            net::awaitable<void> handle();
        public:
            Connection(std::shared_ptr<Setting> setting, websocket::stream<beast::tcp_stream> ws, net::any_io_executor executor);
            ~Connection();
            void start();
    };
}