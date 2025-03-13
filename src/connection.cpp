#include <boost/beast/core/bind_handler.hpp>
#include <boost/log/trivial.hpp>
#include <chrono>
#include <string>
#include <xz-cpp-server/connection.h>
#include <xz-cpp-server/common/tools.h>
#include <nlohmann/json.hpp>

namespace xiaozhi {
    Connection::Connection(std::shared_ptr<Setting> setting, websocket::stream<beast::tcp_stream> ws, net::any_io_executor executor):
        setting_(setting), 
        vad_(setting),
        ws_(std::move(ws)),
        silence_timer_(executor) {
            min_silence_tms_ = setting->config["VAD"]["SileroVAD"]["min_silence_duration_ms"].as<int>();
            asr_ = DoubaoASR::createInstance(executor);
            asr_->on_detect(beast::bind_front_handler(&Connection::on_asr_detect, this));
    }

    void Connection::on_asr_detect(std::string text) {
        BOOST_LOG_TRIVIAL(info) << "Connection recv asr text:" << text;
    }

    net::awaitable<void> Connection::send_welcome() {
        session_id_ = tools::generate_uuid();
        std::string welcome_msg_str("{\"type\":\"hello\",\"transport\":\"websocket\",\"audio_params\":{\"sample_rate\":16000}}");
        BOOST_LOG_TRIVIAL(info) << "发送welcome_msg:" << welcome_msg_str;
        ws_.text(true);
        co_await ws_.async_write(net::buffer(std::move(welcome_msg_str)), net::use_awaitable);
    }
    
    net::awaitable<void> Connection::handle_text(beast::flat_buffer &buffer) {
        auto data_str = boost::beast::buffers_to_string(buffer.data());
        BOOST_LOG_TRIVIAL(info) << "收到文本消息(" << &ws_ << "):" << data_str;
        auto data = nlohmann::json::parse(data_str);
        if(data["type"] == "hello") {
            co_await send_welcome();
        } else if(data["type"] == "listen" && data["state"] == "detect") {
        }
        co_return;
    }

    void Connection::audio_silence_end(const boost::system::error_code& ec) {
        if(ec == net::error::operation_aborted) {
            // BOOST_LOG_TRIVIAL(debug) << "定时器被取消!" << std::endl;
            return;;
        }
        asr_->send_opus(std::nullopt);
    }

    net::awaitable<void> Connection::handle_binary(beast::flat_buffer &buffer) {
        if(vad_.is_vad(buffer)) {
            BOOST_LOG_TRIVIAL(debug) << "收到声音(" << &ws_ << "):" << buffer.size();
            asr_->connect();
            asr_->send_opus(std::move(buffer));
            silence_timer_.cancel();
            silence_timer_.expires_after(std::chrono::milliseconds(min_silence_tms_));
            silence_timer_.async_wait(beast::bind_front_handler(&Connection::audio_silence_end, this));
        } else {
            BOOST_LOG_TRIVIAL(debug) << "收到音频(" << &ws_ << "):" << buffer.size();
        }
        co_return;
    }


    net::awaitable<void> Connection::handle() {
        while(true) {
            beast::flat_buffer buffer;
            auto [ec, _] = co_await ws_.async_read(buffer, net::as_tuple(net::use_awaitable));
            if(ec == websocket::error::closed) {
                asr_->close();
                co_return;
            } else if(ec) {
                asr_->close();
                throw boost::system::system_error(ec);
            }
            if(ws_.got_text()) {
                co_await handle_text(buffer);
            } else if(ws_.got_binary()) {
                co_await handle_binary(buffer);
            }
        }
    }
}