#include "nlohmann/json_fwd.hpp"
#include "xz-cpp-server/asr/doubao.h"
#include <boost/asio/placeholders.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/log/trivial.hpp>
#include <chrono>
#include <fstream>
#include <optional>
#include <opus/opus.h>
#include <opus/opus_defines.h>
#include <string>
#include <utility>
#include <xz-cpp-server/connection.h>
#include <xz-cpp-server/silero_vad/vad.h>
#include <xz-cpp-server/common/tools.h>
#include <nlohmann/json.hpp>

namespace xiaozhi {
    Connection::Connection(std::shared_ptr<Setting> setting, net::any_io_executor executor):
        setting_(setting), 
        vad_(setting),
        silence_timer_(executor) {
            min_silence_tms = setting->config["VAD"]["SileroVAD"]["min_silence_duration_ms"].as<int>();
    }

    net::awaitable<void> Connection::send_welcome(websocket::stream<beast::tcp_stream> &ws) {
        session_id_ = tools::generate_uuid();
        std::string welcome_msg_str("{\"type\":\"hello\",\"transport\":\"websocket\",\"audio_params\":{\"sample_rate\":16000}}");
        BOOST_LOG_TRIVIAL(info) << "发送welcome_msg:" << welcome_msg_str;
        ws.text(true);
        co_await ws.async_write(net::buffer(std::move(welcome_msg_str)), net::use_awaitable);
    }
    
    net::awaitable<void> Connection::handle_text(websocket::stream<beast::tcp_stream> &ws, beast::flat_buffer &buffer) {
        auto data_str = boost::beast::buffers_to_string(buffer.data());
        BOOST_LOG_TRIVIAL(info) << "收到文本消息(" << &ws << "):" << data_str;
        auto data = nlohmann::json::parse(data_str);
        if(data["type"] == "hello") {
            co_await send_welcome(ws);
        } else if(data["type"] == "listen" && data["state"] == "detect") {
            // ws.text(true);
            // co_await ws.async_write(net::buffer("{\"type\":\"llm\",\"emotion\":\"happy\",\"text\":\"😀\"}"), net::use_awaitable);
            // co_await ws.async_write(net::buffer("{\"type\":\"tts\",\"state\":\"sentence_start\",\"text\":\"测试\"}"), net::use_awaitable);
            // co_await ws.async_write(net::buffer("{\"type\":\"tts\",\"state\":\"start\"}"), net::use_awaitable);
            // ws.binary(true);
            // int error;
            // auto encoder = opus_encoder_create(16000, 1, OPUS_APPLICATION_AUDIO, &error);
            // auto asr = co_await DoubaoASR::createInstance();
            // asr->connect();
            // for(auto i=0; i<bufs.size(); ++i) {
            //     auto is_last = i == (bufs.size() - 1);
            //     asr->send_opus(std::make_pair(bufs[i], is_last));
            // }
            // timer.expires_after(std::chrono::seconds(3));
            // timer.async_wait([&ws](const boost::system::error_code& ec) {
            //     ws.text(true);
            //     ws.async_write(net::buffer("{\"type\":\"tts\",\"state\":\"stop\"}"), [](beast::error_code ec, std::size_t bytes_transferred) {
            //         BOOST_LOG_TRIVIAL(info) << "write stop callback:" << ec;
            //     });
            // });
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

    net::awaitable<void> Connection::handle_binary(websocket::stream<beast::tcp_stream> &ws, beast::flat_buffer &buffer) {
        if(vad_.is_vad(buffer)) {
            BOOST_LOG_TRIVIAL(debug) << "收到声音(" << &ws << "):" << buffer.size();
            asr_->connect();
            asr_->send_opus(std::move(buffer));
            silence_timer_.cancel();
            silence_timer_.expires_after(std::chrono::milliseconds(min_silence_tms));
            silence_timer_.async_wait(beast::bind_front_handler(&Connection::audio_silence_end, this));
        } else {
            BOOST_LOG_TRIVIAL(debug) << "收到音频(" << &ws << "):" << buffer.size();
        }
        co_return;
    }


    net::awaitable<void> Connection::handle(websocket::stream<beast::tcp_stream> &ws) {
        asr_ = co_await DoubaoASR::createInstance();
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