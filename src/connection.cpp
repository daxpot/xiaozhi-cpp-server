#include <xz-cpp-server/connection.h>
#include <xz-cpp-server/common/tools.h>

namespace xiaozhi {
    Connection::Connection(std::shared_ptr<Setting> setting, websocket::stream<beast::tcp_stream> ws, net::any_io_executor executor):
        setting_(setting), 
        cmd_exit_(setting->config["CMD_exit"].as<std::vector<std::string>>()),
        vad_(setting),
        executor_(executor),
        ws_(std::move(ws)),
        strand_(ws_.get_executor()),
        silence_timer_(executor_) {
            auto prompt = setting->config["prompt"].as<std::string>();
            size_t pos = prompt.find("{date_time}");
            if(pos != prompt.npos) {
                auto now = std::chrono::system_clock::now();
                prompt.replace(pos, 11, std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::time_point_cast<std::chrono::seconds>(now)));
            }

            dialogue_.push_back(boost::json::object{{"role", "system"}, {"content", std::move(prompt)}});
            min_silence_tms_ = setting->config["VAD"]["SileroVAD"]["min_silence_duration_ms"].as<int>();
            close_connection_no_voice_time_ = setting->config["close_connection_no_voice_time"].as<int>(),
            asr_ = asr::createASR(executor_);
            llm_ = llm::createLLM(executor_);
            tts_ = tts::createTTS(executor_);
    }

    void Connection::start() {
        auto self = shared_from_this();
        net::co_spawn(executor_, [self] {
            return self->asr_loop(); 
        }, std::bind_front(tools::on_spawn_complete, "Connection asr loop"));
        net::co_spawn(strand_, [self] {
            return self->tts_loop(); 
        }, std::bind_front(tools::on_spawn_complete, "Connection tts loop"));
        net::co_spawn(strand_, [self] {
            return self->handle(); 
        }, std::bind_front(tools::on_spawn_complete, "Connection handle"));
    }

    net::awaitable<void> Connection::asr_loop() {
        while(!is_released_) {
            std::optional<beast::flat_buffer> buf;
            if(!asr_audio_.try_pop(buf)) {
                co_await net::steady_timer(executor_, std::chrono::milliseconds(20)).async_wait(net::use_awaitable);
                continue;
            }
            auto text = co_await asr_->detect_opus(buf);
            if(!buf && text.size() > 0) {
                net::co_spawn(executor_, [self=shared_from_this(), text=std::move(text)] {
                    return self->handle_asr_text(std::move(text));
                }, std::bind_front(tools::on_spawn_complete, "Connection handle asr text"));
            }
        }
        BOOST_LOG_TRIVIAL(info) << "Connection asr loop over";
    }

    net::awaitable<void> Connection::tts_loop() {
        long long tts_stop_end_timestamp = 0;
        std::queue<std::pair<std::string, long long>> tts_sentence_queue;
        while(!is_released_) {
            std::string text;
            auto now = tools::get_tms();
            if(!llm_response_.try_pop(text)) {
                net::steady_timer timer(executor_, std::chrono::milliseconds(60));
                co_await timer.async_wait(net::use_awaitable);
            } else {
                BOOST_LOG_TRIVIAL(info) << "获取大模型输出:" << text;
                if(text == "<abort>") {
                    std::queue<std::pair<std::string, long long>>().swap(tts_sentence_queue);
                    const std::string_view data = R"({"type":"tts","state":"stop"})";
                    co_await ws_.async_write(net::buffer(data.data(), data.size()), net::use_awaitable);
                    tts_stop_end_timestamp = 0;
                    continue;
                }
                if(tts_stop_end_timestamp == 0) {
                    ws_.text(true);
                    const std::string_view data = R"({"type":"tts","state":"start"})";
                    co_await ws_.async_write(net::buffer(data.data(), data.size()), net::use_awaitable);
                    now = tools::get_tms();
                    tts_stop_end_timestamp = now;
                    BOOST_LOG_TRIVIAL(debug) << "tts start:" << now;
                }
                auto audio = co_await tts_->text_to_speak(text);
                tts_sentence_queue.push({text, tts_stop_end_timestamp});
                for(auto& data: audio) {
                    ws_.binary(true);
                    co_await ws_.async_write(net::buffer(data), net::use_awaitable);
                    tts_stop_end_timestamp += 60;       //60ms一段音频
                }
            }
            while(!tts_sentence_queue.empty()) {
                auto front = tts_sentence_queue.front();
                now = tools::get_tms();
                if(now < front.second) {
                    break;
                }
                ws_.text(true);
                boost::json::object obj = {
                    {"type", "tts"},
                    {"state", "sentence_start"},
                    {"text", front.first}
                };
                co_await ws_.async_write(net::buffer(boost::json::serialize(obj)), net::use_awaitable);
                tts_sentence_queue.pop();
                // BOOST_LOG_TRIVIAL(debug) << "tts sentence start:" << front.first << ",now:" << now << ",plan:" << front.second;
            }
            now = tools::get_tms();
            if(tts_stop_end_timestamp != 0 && now > tts_stop_end_timestamp) {
                ws_.text(true);
                const std::string_view data = R"({"type":"tts","state":"stop"})";
                co_await ws_.async_write(net::buffer(data.data(), data.size()), net::use_awaitable);
                tts_stop_end_timestamp = 0;
                BOOST_LOG_TRIVIAL(debug) << "tts end:" << now;
            }
        }
        BOOST_LOG_TRIVIAL(info) << "Connection tts loop over";
    }

    void Connection::push_llm_response(std::string str) {
        //去除首位空格和markdown的*号#号
        str = std::regex_replace(str, std::regex(R"((^\s+)|(\s+$)[\*\#])"), "");
        if(str.size() > 0) {
            llm_response_.push(std::move(str));
        }
    }

    net::awaitable<void> Connection::handle_asr_text(std::string text) {
        BOOST_LOG_TRIVIAL(info) << "Connection handle asr text:" << text;
        for(auto& cmd : cmd_exit_) {
            if(text == cmd) {
                is_released_ = true;
                co_return;
            }
        }
        dialogue_.push_back(boost::json::object{{"role", "user"}, {"content", text}});
        std::string message;
        size_t pos = 0;
        co_await llm_->response(dialogue_, [this, &message, &pos](std::string_view res) {
            message.append(res.data(), res.size());
            auto p = tools::find_last_segment(message);
            if(p != message.npos && p - pos + 1 > 6) {
                push_llm_response(message.substr(pos, p-pos+1));
                pos = p+1;
            }
        });
        if(pos < message.size()) {
            push_llm_response(message.substr(pos));
        }
        dialogue_.push_back(boost::json::object{{"role", "assistant"}, {"content", message}});
    }

    net::awaitable<void> Connection::send_welcome() {
        session_id_ = tools::generate_uuid();
        boost::json::object welcome {
            {"type", "hello"},
            {"transport", setting_->config["welcome"]["transport"].as<std::string>()},
            {"audio_params", {
                {"sample_rate", setting_->config["welcome"]["audio_params"]["sample_rate"].as<int>()}
            }}
        };
        auto welcome_msg_str = boost::json::serialize(welcome);
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
        } else if(data["type"] == "abort") {
            llm_response_.clear();
            llm_response_.push("<abort>");
        }
        co_return;
    }

    net::awaitable<void> Connection::handle_binary(beast::flat_buffer &buffer) {
        if(vad_.is_vad(buffer)) {
            // BOOST_LOG_TRIVIAL(debug) << "收到声音(" << &ws_ << "):" << buffer.size();
            asr_audio_.push(std::move(buffer));
            silence_timer_.cancel();
            silence_timer_.expires_after(std::chrono::milliseconds(min_silence_tms_));
            silence_timer_.async_wait([self=shared_from_this()](const boost::system::error_code& ec) {
                if(ec != net::error::operation_aborted) {
                    self->asr_audio_.push(std::nullopt);
                }
            });
        } else {
            // BOOST_LOG_TRIVIAL(debug) << "收到音频(" << &ws_ << "):" << buffer.size();
        }
        co_return;
    }


    net::awaitable<void> Connection::handle() {
        while(!is_released_) {
            beast::flat_buffer buffer;
            beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(close_connection_no_voice_time_));
            auto [ec, _] = co_await ws_.async_read(buffer, net::as_tuple(net::use_awaitable));
            if(ec == websocket::error::closed) {
                BOOST_LOG_TRIVIAL(debug) << "handle closed";
                break;
            } else if(ec) {
                BOOST_LOG_TRIVIAL(debug) << "handle error" << ec.message();
                break;
            }
            if(ws_.got_text()) {
                co_await handle_text(buffer);
            } else if(ws_.got_binary()) {
                co_await handle_binary(buffer);
            }
        }
        is_released_ = true;
        BOOST_LOG_TRIVIAL(debug) << "handle ended";
    }

    Connection::~Connection() {
        is_released_ = true;
        BOOST_LOG_TRIVIAL(info) << "Connection destroyed";
    }
}