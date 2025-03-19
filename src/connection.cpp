#include <boost/asio/co_spawn.hpp>
#include <boost/asio/registered_buffer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/log/trivial.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <xz-cpp-server/connection.h>
#include <xz-cpp-server/common/tools.h>
#include <boost/json.hpp>
#include <xz-cpp-server/tts/base.h>
#include <xz-cpp-server/llm/base.h>
#include <xz-cpp-server/asr/base.h>

namespace xiaozhi {
    Connection::Connection(std::shared_ptr<Setting> setting, websocket::stream<beast::tcp_stream> ws, net::any_io_executor executor):
        setting_(setting), 
        vad_(setting),
        executor_(executor),
        ws_(std::move(ws)),
        silence_timer_(executor_) {
            min_silence_tms_ = setting->config["VAD"]["SileroVAD"]["min_silence_duration_ms"].as<int>();
            asr_ = asr::createASR(executor_);
            // asr_->on_detect(beast::bind_front_handler(&Connection::on_asr_detect, this));
            asr_->on_detect([this](std::string text) {
                net::co_spawn(executor_, this->on_asr_detect(std::move(text)), [](std::exception_ptr e) {
                    if(e) {
                        try {
                            std::rethrow_exception(e);
                        } catch(std::exception& e) {
                            BOOST_LOG_TRIVIAL(error) << "Connection asr detect spawn error:" << e.what();
                        }
                    }
                });
            });
            llm_ = llm::createLLM(executor_);
            net::co_spawn(executor_, tts_loop(), [](std::exception_ptr e) {
                if(e) {
                    try {
                        std::rethrow_exception(e);
                    } catch(std::exception& e) {
                        BOOST_LOG_TRIVIAL(error) << "Connection tts loop error:" << e.what();
                    }
                }
            });
    }

    net::awaitable<void> Connection::tts_loop() {
        while(!is_released_) {
            std::string text;
            if(!llm_response_.try_pop(text)) {
                net::steady_timer timer(executor_, std::chrono::milliseconds(60));
                co_await timer.async_wait(net::use_awaitable);
                continue;
            }
            BOOST_LOG_TRIVIAL(debug) << "获取大模型输出:" << text;
            auto tts = tts::createTTS(executor_);
            ws_.text(true);
            co_await ws_.async_write(net::buffer(R"({"type":"tts","state":"start"})"), net::use_awaitable);
            auto audio = co_await tts->text_to_speak(text);
            ws_.text(true);
            boost::json::object obj = {
                {"type", "tts"},
                {"state", "sentence_start"},
                {"text", text}
            };
            co_await ws_.async_write(net::buffer(boost::json::serialize(obj)), net::use_awaitable);
            for(auto& data: audio) {
                ws_.binary(true);
                co_await ws_.async_write(net::buffer(data), net::use_awaitable);
            }
            ws_.text(true);
            // co_await ws_.async_write(net::buffer(R"({"type":"tts","state":"stop"}")"), net::use_awaitable);
        }
    }

    net::awaitable<void> Connection::on_asr_detect(std::string text) {
        BOOST_LOG_TRIVIAL(info) << "Connection recv asr text:" << text;
        dialogue_.push_back({"user", text});
        std::string message;
        size_t pos = 0;
        co_await llm_->response(dialogue_, [this, &message, &pos](std::string_view res) {
            message.append(res.data(), res.size());
            auto p = tools::find_last_segment(message);
            if(p != message.npos && p - pos + 1 > 6) {
                llm_response_.push(message.substr(pos, p-pos+1));
                pos = p+1;
            }
        });
        if(pos < message.size()) {
            llm_response_.push(message.substr(pos));
        }
        dialogue_.push_back({"assistant", message});
    }

    net::awaitable<void> Connection::send_welcome() {
        session_id_ = tools::generate_uuid();
        std::string welcome_msg_str(R"({"type":"hello","transport":"websocket","audio_params":{"sample_rate":16000}})");
        BOOST_LOG_TRIVIAL(info) << "发送welcome_msg:" << welcome_msg_str;
        ws_.text(true);
        co_await ws_.async_write(net::buffer(std::move(welcome_msg_str)), net::use_awaitable);
    }
    
    net::awaitable<void> Connection::handle_text(beast::flat_buffer &buffer) {
        auto data_str = boost::beast::buffers_to_string(buffer.data());
        BOOST_LOG_TRIVIAL(info) << "收到文本消息(" << &ws_ << "):" << data_str;
        auto data = boost::json::parse(data_str).as_object();
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
        asr_->detect_opus(std::nullopt);
    }

    net::awaitable<void> Connection::handle_binary(beast::flat_buffer &buffer) {
        if(vad_.is_vad(buffer)) {
            BOOST_LOG_TRIVIAL(debug) << "收到声音(" << &ws_ << "):" << buffer.size();
            asr_->detect_opus(std::move(buffer));
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
                co_return;
            } else if(ec) {
                throw boost::system::system_error(ec);
            }
            if(ws_.got_text()) {
                co_await handle_text(buffer);
            } else if(ws_.got_binary()) {
                co_await handle_binary(buffer);
            }
        }
    }

    Connection::~Connection() {
        is_released_ = true;
    }
}