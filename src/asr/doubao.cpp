#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/lockfree/policies.hpp>
#include <boost/log/trivial.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <xz-cpp-server/asr/doubao.h>
#include <nlohmann/json.hpp>
#include <xz-cpp-server/common/tools.h>
#include <xz-cpp-server/config/setting.h>
#include <boost/lockfree/spsc_queue.hpp>
#include <ogg/ogg.h>

using tcp = net::ip::tcp;
namespace ssl = net::ssl;
namespace websocket = beast::websocket;
namespace http = beast::http;

const std::string host{"openspeech.bytedance.com"};
const std::string port{"443"};
const std::string path{"/api/v2/asr"};
const bool is_gzip = true;

// Helper to construct the header as per protocol (big-endian)
std::vector<uint8_t> make_header(uint8_t msg_type, uint8_t flags, bool json_serialization, bool gzip_compression) {
    std::vector<uint8_t> header(4);
    header[0] = (0x1 << 4) | 0x1; // Protocol version 1, Header size 4 bytes
    header[1] = (msg_type << 4) | flags; // Message type and flags
    header[2] = (json_serialization ? 0x1 : 0x0) << 4 | (gzip_compression ? 0x1 : 0x0); // Serialization and compression
    header[3] = 0x00; // Reserved
    return header;
}

std::vector<uint8_t> build_payload(uint8_t msg_type, uint8_t flags, std::string payload_str) {
    if(is_gzip) {
        payload_str = tools::gzip_compress(payload_str);
    }
    std::vector<uint8_t> payload(payload_str.begin(), payload_str.end());
    
    auto data = make_header(msg_type, flags, true, is_gzip); // Full client request, JSON, Gzip
    uint32_t payload_size = payload.size();
    data.insert(data.end(), {(uint8_t)(payload_size >> 24), (uint8_t)(payload_size >> 16),
                            (uint8_t)(payload_size >> 8), (uint8_t)payload_size}); // Big-endian size
    data.insert(data.end(), payload.begin(), payload.end());
    return data;
}

std::optional<nlohmann::basic_json<>> parse_reponse(std::string payload, bool gzip_compression) {
    int header_len = (payload[0] & 0x0f) << 2;
    int message_type = (payload[1] & 0xf0) >> 4;
    int message_serial = (payload[2] & 0xf0) >> 4;
    int message_compress = payload[2] & 0x0f;
    uint32_t payload_len = 0;
    uint32_t payload_offset = 0;

    if (message_type == 0b1001) {
        payload_len = *(unsigned int *) (payload.data() + header_len);
        payload_len = boost::endian::big_to_native(payload_len);
        payload_offset = header_len + 4;
    } else if (message_type == 0b1111) {
        uint32_t error_code = *(unsigned int *) (payload.data() + header_len);
        error_code = boost::endian::big_to_native(error_code);
        payload_len = *(unsigned int *) (payload.data() + header_len + 4);
        payload_len = boost::endian::big_to_native(payload_len);
        payload_offset = header_len + 8;
    } else {
        BOOST_LOG_TRIVIAL(error) << "DoubaoASR parse response unsupported message type:" << message_type;
        return std::nullopt;
    }
    if(message_compress) {
        payload = tools::gzip_decompress(payload.substr(payload_offset, payload_len));
    }
    return nlohmann::json::parse(payload);
}

// 分段转换并发送 Opus 数据
class OggOpusStreamer {
private:
    ogg_stream_state os;
    ogg_int64_t granulepos;
    int packetno;
public:
    OggOpusStreamer() : granulepos(0), packetno(0) {
        ogg_stream_init(&os, 1); // 初始化 Ogg 流，序列号设为 1
    }

    ~OggOpusStreamer() {
        ogg_stream_clear(&os);
    }

    std::vector<unsigned char> build_ogg_page(ogg_stream_state* os, ogg_packet* op) {
        ogg_stream_packetin(os, op);
        ogg_page og;
        std::vector<unsigned char> page_data;
        while (ogg_stream_pageout(os, &og)) {
            // 合并 header 和 body 为一个连续缓冲区
            page_data.insert(page_data.end(), og.header, og.header + og.header_len);
            page_data.insert(page_data.end(), og.body, og.body + og.body_len);
        }
        return page_data;
    }

    std::vector<unsigned char> build_id_headers() {
        // 1. ID Header
        unsigned char id_header[] = {
            'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', // "OpusHead"
            1,                                      // 版本号
            1,                                      // 通道数 (单声道)
            0, 0,                                   // 预跳样本数 (可调整)
            0x80, 0x3E, 0, 0,                      // 采样率 16000 Hz (little-endian: 0x3E80)
            0, 0,                                   // 输出增益
            0                                       // 映射族
        };
        ogg_packet id_packet;
        id_packet.packet = id_header;
        id_packet.bytes = sizeof(id_header);
        id_packet.b_o_s = 1; // 流开始
        id_packet.e_o_s = 0;
        id_packet.granulepos = 0;
        id_packet.packetno = packetno++;

        return build_ogg_page(&os, &id_packet);
    }
        // 发送 ID Header 和 Comment Header
    std::vector<unsigned char> build_comment_headers() {

        // 2. Comment Header
        unsigned char comment_header[] = {
            'O', 'p', 'u', 's', 'T', 'a', 'g', 's', // "OpusTags"
            0, 0, 0, 0,                            // 厂商字符串长度
            0, 0, 0, 0                             // 用户注释数量 (0)
        };
        ogg_packet comment_packet;
        comment_packet.packet = comment_header;
        comment_packet.bytes = sizeof(comment_header);
        comment_packet.b_o_s = 0;
        comment_packet.e_o_s = 0;
        comment_packet.granulepos = 0;
        comment_packet.packetno = packetno++;

        return build_ogg_page(&os, &comment_packet);
    }

    // 处理并发送一个 beast::flat_buffer
    std::vector<unsigned char> process_buffer(const beast::flat_buffer& buffer, bool is_last = false) {
        // 从 flat_buffer 获取数据指针和大小
        const unsigned char* data = net::buffer_cast<const unsigned char*>(buffer.data());
        size_t size = net::buffer_size(buffer.data());

        ogg_packet data_packet;
        data_packet.packet = const_cast<unsigned char*>(data); // ogg_packet 需要非 const 指针
        data_packet.bytes = size;
        data_packet.b_o_s = 0;
        data_packet.e_o_s = is_last ? 1 : 0; // 最后一个包标记结束
        data_packet.granulepos = granulepos;
        data_packet.packetno = packetno++;

        // 每帧 60ms，16000 Hz 采样率，每帧 960 个样本
        granulepos += 960;

        return build_ogg_page(&os, &data_packet);
    }
};

namespace xiaozhi {
    std::shared_ptr<DoubaoASR> DoubaoASR::createInstance(net::any_io_executor &executor) {
        return std::make_shared<DoubaoASR>(executor);
    }
    DoubaoASR::DoubaoASR(net::any_io_executor &executor) {
        auto setting = xiaozhi::Setting::getSetting();
        impl_ = std::make_unique<ASRImpl>(setting, executor);
    }
    
    class DoubaoASR::ASRImpl {
        public:
            net::any_io_executor executor_; //需要比resolver和ws先初始化，所以申明在前面
        private:
            bool is_connecting_ = false;
            bool is_connected_ = false;
            const std::string appid_;
            const std::string access_token_;
            const std::string cluster_;
            tcp::resolver resolver_;
            ssl::context ctx_;
            websocket::stream<ssl::stream<beast::tcp_stream>> ws_;
            boost::lockfree::spsc_queue<std::optional<beast::flat_buffer>, boost::lockfree::capacity<128>> queue;
            std::function<void(std::string)> on_detect_cb_;
        public:
            ASRImpl(std::shared_ptr<Setting> setting, net::any_io_executor &executor):
                executor_(executor),
                appid_(setting->config["ASR"]["DoubaoASR"]["appid"].as<std::string>()),
                access_token_(setting->config["ASR"]["DoubaoASR"]["access_token"].as<std::string>()),
                cluster_(setting->config["ASR"]["DoubaoASR"]["cluster"].as<std::string>()),
                resolver_(executor_),
                ctx_(ssl::context::sslv23_client),
                ws_(executor_, ctx_) {
                    ctx_.set_verify_mode(ssl::verify_peer);
                    ctx_.set_default_verify_paths();
            }
            net::awaitable<void> connect() {
                if(is_connecting_ || is_connected_) {
                    co_return;
                }
                is_connecting_ = true;
                BOOST_LOG_TRIVIAL(info) << "DoubaoASR websocket start connect";
                co_await start();
            }
            net::awaitable<void> close() {
                if(is_connected_ || is_connecting_) {
                    clear();
                    auto [ec] = co_await ws_.async_close(websocket::close_code::normal, net::as_tuple(net::use_awaitable));
                    BOOST_LOG_TRIVIAL(info) << "DoubaoASR websocket close: " << (ec ? ec.message() : "success");
                } else {
                    BOOST_LOG_TRIVIAL(info) << "DoubaoASR websocket already closed.";
                }
            }
            void send_opus(std::optional<beast::flat_buffer> buf) {
                if(queue.write_available()) {
                    queue.push(std::move(buf));
                } else {
                    BOOST_LOG_TRIVIAL(info) << "DoubaoASR queue not write available:";
                }
            }

            void on_detect(std::function<void(std::string)> callback) {
                on_detect_cb_ = callback;
            }
        private:
            void clear() {
                is_connected_ = false;
                is_connecting_ = false;
            }
            void clear(std::string title) {
                clear();
                BOOST_LOG_TRIVIAL(error) << title;
            }
            void clear(std::string title, beast::error_code ec) {
                clear();
                if(ec) {
                    BOOST_LOG_TRIVIAL(error) << title << ec.message();
                }
            }
            net::awaitable<void> start() {
                auto [ec, results] = co_await resolver_.async_resolve(host, port, net::as_tuple(net::use_awaitable));
                if(ec) {
                    clear("DoubaoASR resolve:", ec);
                    co_return;
                }
                tcp::endpoint ep;
                std::tie(ec, ep) = co_await beast::get_lowest_layer(ws_).async_connect(results, net::as_tuple(net::use_awaitable));
                if(ec) {
                    clear("DoubaoASR connect:", ec);
                    co_return;
                }
                // Set a timeout on the operation
                beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
                if(!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host.c_str())) {
                    auto ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                        net::error::get_ssl_category());
                    clear("DoubaoASR ssl:", ec);
                    co_return;
                }
                std::tie(ec) = co_await ws_.next_layer().async_handshake(ssl::stream_base::client, net::as_tuple(net::use_awaitable));
                if(ec) {
                    clear("DoubaoASR ssl handshake:", ec);
                    co_return;
                }
                beast::get_lowest_layer(ws_).expires_never();
                ws_.set_option(
                    websocket::stream_base::timeout::suggested(
                        beast::role_type::client));
                ws_.set_option(websocket::stream_base::decorator(
                    [&token = access_token_](websocket::request_type& req) {
                        req.set(http::field::authorization, std::string("Bearer; ") + token);
                    }));
                std::tie(ec) = co_await ws_.async_handshake(host + ':' + port, path, net::as_tuple(net::use_awaitable));
                if(ec) {
                    clear("DoubaoASR handshake:", ec);
                    co_return;
                }
                co_await send_full_client();
            }
            net::awaitable<void> send_full_client() {
                auto uuid = tools::generate_uuid();
                nlohmann::json obj = {
                    {"app", {
                            {"appid", appid_},
                            {"token", access_token_},
                            {"cluster", cluster_}
                        }
                    }, {
                        "user", {
                            {"uid", uuid}
                        }
                    }, {
                        "audio", {
                            {"format", "ogg"},
                            {"rate", 16000},
                            {"bits", 16},
                            {"channel", 1},
                            {"codec", "opus"},
                            {"language", "zh-CN"}
                        }
                    }, {
                        "request", {
                            {"reqid", uuid},
                            {"show_utterances", false},
                            {"sequence", 1}
                        }
                    }
                };
                auto data = build_payload(0x1, 0x0, obj.dump());
                ws_.binary(true);
                auto [ec, bytes_transferred] = co_await ws_.async_write(net::buffer(data), net::as_tuple(net::use_awaitable));
                if(ec) {
                    clear("DoubaoASR send full client:", ec);
                    co_return;
                }
                beast::flat_buffer buffer;
                std::tie(ec, bytes_transferred) = co_await ws_.async_read(buffer, net::as_tuple(net::use_awaitable));
                if(ec) {
                    clear("DoubaoASR recv full server:", ec);
                    co_return;
                }
                auto rej = parse_reponse(beast::buffers_to_string(buffer.data()), is_gzip);
                if(rej == std::nullopt || (*rej)["code"] != 1000) {
                    clear();
                    co_return;
                }
                is_connecting_ = false;
                is_connected_ = true;
                BOOST_LOG_TRIVIAL(info) << "DoubaoASR websocket connected";
                co_await loop();
            }
            net::awaitable<void> loop() {
                OggOpusStreamer streamer;
                auto data = streamer.build_id_headers();
                co_await send(std::string(data.begin(), data.end()), false);
                data = streamer.build_comment_headers();
                co_await send(std::string(data.begin(), data.end()), false);
                while(true) {
                    std::optional<beast::flat_buffer> buf;
                    if(!queue.pop(buf)) {
                        net::steady_timer timer(executor_, std::chrono::milliseconds(10));
                        co_await timer.async_wait(net::use_awaitable);
                    } else {
                        bool is_last = false;
                        if(buf == std::nullopt) {
                            buf = beast::flat_buffer{};
                            is_last = true;
                        }
                        auto data = streamer.process_buffer(*buf, is_last);
                        co_await send(std::string(data.begin(), data.end()), is_last);
                        if(is_last) {
                            co_await close();
                            break;
                        }
                    }
                }
            }

            net::awaitable<void> send(std::string audio, bool is_last) {
                std::vector<uint8_t> data;
                data = build_payload(0x2, is_last ? 0x2 : 0x0, audio);
                ws_.binary(true);
                auto [ec, bytes_transferred] = co_await ws_.async_write(net::buffer(data), net::as_tuple(net::use_awaitable));
                if(ec) {
                    clear("DoubaoASR send audio:", ec);
                    co_return;
                }
                beast::flat_buffer buffer;
                std::tie(ec, bytes_transferred) = co_await ws_.async_read(buffer, net::as_tuple(net::use_awaitable));
                if(ec) {
                    clear("DoubaoASR recv server:", ec);
                    co_return;
                }
                auto rej = parse_reponse(beast::buffers_to_string(buffer.data()), is_gzip);
                if(rej == std::nullopt || (*rej)["code"] != 1000) {
                    BOOST_LOG_TRIVIAL(error) << "DoubaoASR recv error:" << (rej == std::nullopt ? "" : rej->dump());
                    co_return;
                }
                auto& result = (*rej)["result"];
                if(result.is_array()) {
                    BOOST_LOG_TRIVIAL(info) << "DoubaoASR detect:" << rej->dump();
                    std::string sentence;
                    for(const auto &item : result) {
                        sentence += item["text"].get<std::string>();
                    }
                    if(on_detect_cb_) {
                        on_detect_cb_(std::move(sentence));
                    }
                }
            }
    };
    
    void DoubaoASR::connect() {
        net::co_spawn(impl_->executor_, [self = shared_from_this()]() {return self->impl_->connect();}, 
        [](std::exception_ptr e) {
            if(e) {
                try {
                    std::rethrow_exception(e);
                } catch(std::exception& e) {
                    BOOST_LOG_TRIVIAL(error) << "DoubaoASR connect error:" << e.what();
                }
            }
        });
    }
    void DoubaoASR::close() {
        net::co_spawn(impl_->executor_, [self = shared_from_this()]() {return self->impl_->close();}, 
        [](std::exception_ptr e) {
            if(e) {
                try {
                    std::rethrow_exception(e);
                } catch(std::exception& e) {
                    BOOST_LOG_TRIVIAL(error) << "DoubaoASR close error:" << e.what();
                }
            }
        });
    }
    void DoubaoASR::send_opus(std::optional<beast::flat_buffer> buf) {
        return impl_->send_opus(std::move(buf));
    }

    void DoubaoASR::on_detect(std::function<void(std::string)> callback) {
        return impl_->on_detect(callback);
    }
}