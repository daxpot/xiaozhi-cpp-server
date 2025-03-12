#include "nlohmann/json_fwd.hpp"
#include "xz-cpp-server/asr/doubao.h"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/log/trivial.hpp>
#include <chrono>
#include <fstream>
#include <optional>
#include <opus/opus.h>
#include <opus/opus_defines.h>
#include <string>
#include <xz-cpp-server/connection.h>
#include <xz-cpp-server/silero_vad/vad.h>
#include <xz-cpp-server/common/tools.h>
#include <nlohmann/json.hpp>

namespace xiaozhi {
    Connection::Connection(std::shared_ptr<Setting> setting, net::any_io_executor executor):
        setting(setting), 
        vad(setting),
        timer(executor) {

    }

    net::awaitable<void> Connection::send_welcome(websocket::stream<beast::tcp_stream> &ws) {
        session_id = tools::generate_uuid();
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
            ws.text(true);
            co_await ws.async_write(net::buffer("{\"type\":\"tts\",\"state\":\"sentence_start\",\"text\":\"测试\"}"), net::use_awaitable);
            co_await ws.async_write(net::buffer("{\"type\":\"tts\",\"state\":\"start\"}"), net::use_awaitable);
            ws.binary(true);
            int error;
            auto encoder = opus_encoder_create(16000, 1, OPUS_APPLICATION_AUDIO, &error);
            // auto asr = co_await DoubaoASR::createInstance();
            // asr.connect();
            // for(auto &buf : bufs) {
            //     asr.send_opus(buf);
            // }
            // asr.send_opus(std::nullopt);
            timer.expires_after(std::chrono::seconds(3));
            timer.async_wait([&ws](const boost::system::error_code& ec) {
                ws.text(true);
                ws.async_write(net::buffer("{\"type\":\"tts\",\"state\":\"stop\"}"), [](beast::error_code ec, std::size_t bytes_transferred) {
                    BOOST_LOG_TRIVIAL(info) << "write stop callback:" << ec;
                });
            });
        }
        co_return;
    }

    net::awaitable<void> Connection::handle_binary(websocket::stream<beast::tcp_stream> &ws, beast::flat_buffer &buffer) {
        auto pcm = vad.check_vad(buffer);
        if(pcm != std::nullopt) {
            BOOST_LOG_TRIVIAL(info) << "收到声音(" << &ws << "):" << buffer.size() << ":" << pcm->size();
            // pcm_bufs.insert(pcm_bufs.end(), pcm->begin(), pcm->begin() + pcm->size());
            bufs.push_back(buffer);
        } else {
            BOOST_LOG_TRIVIAL(info) << "收到音频(" << &ws << "):" << buffer.size();
        }
        // auto pcm = vad.merge_voice(buffer);
        // if(pcm != std::nullopt) {
        //     // std::ofstream outFile("tmp/opus_size.opus", std::ios::binary);
        //     // if (!outFile) {
        //     //     BOOST_LOG_TRIVIAL(error) << "无法打开文件进行写入！";
        //     // } else {
        //     //     size_t size = pcm->size();
        //     //     outFile.write(reinterpret_cast<const char*>(&size), sizeof(size)); // 写入元素个数
        //     //     outFile.write(reinterpret_cast<const char*>(pcm->data()), size * sizeof(float)); // 写入数据
        //     //     outFile.close();
        //     //     BOOST_LOG_TRIVIAL(info) << "数据已写入 " << "tmp/opus_size.opus";
        //     // }
        // }
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