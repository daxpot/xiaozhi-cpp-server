#include <atomic>
#include <boost/json/serialize.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <xz-cpp-server/asr/bytedancev2.h>
#include <boost/json.hpp>
#include <xz-cpp-server/common/tools.h>
#include <boost/log/trivial.hpp>
#include <ogg/ogg.h>
#include <xz-cpp-server/common/threadsafe_queue.hpp>
#include <boost/beast/ssl.hpp>

using tcp = net::ip::tcp;
namespace ssl = net::ssl;
namespace websocket = beast::websocket;
namespace http = beast::http;
namespace json = boost::json;
using wss_stream = websocket::stream<ssl::stream<beast::tcp_stream>>;

const std::string host{"openspeech.bytedance.com"};
const std::string port{"443"};
const std::string path{"/api/v2/asr"};
const bool is_gzip = true;

// Helper to construct the header as per protocol (big-endian)
static std::vector<uint8_t> make_header(uint8_t msg_type, uint8_t flags, bool json_serialization, bool gzip_compression) {
    std::vector<uint8_t> header(4);
    header[0] = (0x1 << 4) | 0x1; // Protocol version 1, Header size 4 bytes
    header[1] = (msg_type << 4) | flags; // Message type and flags
    header[2] = (json_serialization ? 0x1 : 0x0) << 4 | (gzip_compression ? 0x1 : 0x0); // Serialization and compression
    header[3] = 0x00; // Reserved
    return header;
}

static std::vector<uint8_t> build_payload(uint8_t msg_type, uint8_t flags, std::vector<uint8_t>& payload) {
    if(is_gzip) {
        auto payload_str = tools::gzip_compress(std::string(payload.begin(), payload.end()));
        payload = std::vector<uint8_t>(payload_str.begin(), payload_str.end());
    }
    
    auto data = make_header(msg_type, flags, true, is_gzip); // Full client request, JSON, Gzip
    uint32_t payload_size = payload.size();
    data.insert(data.end(), {(uint8_t)(payload_size >> 24), (uint8_t)(payload_size >> 16),
                            (uint8_t)(payload_size >> 8), (uint8_t)payload_size}); // Big-endian size
    data.insert(data.end(), payload.begin(), payload.end());
    return data;
}

static std::optional<json::object> parse_response(std::string payload, bool gzip_compression) {
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
        BOOST_LOG_TRIVIAL(error) << "BytedanceASRV2 parse response unsupported message type:" << message_type;
        return std::nullopt;
    }
    if(message_compress) {
        payload = tools::gzip_decompress(payload.substr(payload_offset, payload_len));
    }
    return json::parse(payload).as_object();
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

enum WebsocketState {
    Idle,
    Connecting,
    Connected
};


namespace xiaozhi {
    namespace asr {
        class BytedanceV2::Impl {
            private:
                std::atomic<bool> is_released_ = false;
                WebsocketState ws_state_ = WebsocketState::Idle;

                const std::string appid_;
                const std::string access_token_;
                const std::string cluster_;

                ssl::context ctx_;
                net::any_io_executor executor_;
                std::unique_ptr<wss_stream> ws_;
                std::unique_ptr<OggOpusStreamer> ogg_streamer_;

                void clear(std::string_view title, beast::error_code ec=beast::error_code{}) {
                    ws_ = nullptr;
                    ws_state_ = WebsocketState::Idle;
                    ogg_streamer_ = nullptr;
                    if(ec) {
                        BOOST_LOG_TRIVIAL(error) << title << ec.message();
                    }
                }

                net::awaitable<bool> connect() {
                    BOOST_LOG_TRIVIAL(debug) << "BytedanceASRV2 begin connect";
                    ws_state_ = WebsocketState::Connecting;
                    
                    tcp::resolver resolver {co_await net::this_coro::executor};
                    auto [ec, results] = co_await resolver.async_resolve(host, port, net::as_tuple(net::use_awaitable));
                    if(ec) {
                        clear("BytedanceASRV2 resolve:", ec);
                        co_return false;
                    }
                    
                    ws_ = std::make_unique<wss_stream>(executor_, ctx_);
                    tcp::endpoint ep;
                    std::tie(ec, ep) = co_await beast::get_lowest_layer(*ws_).async_connect(results, net::as_tuple(net::use_awaitable));
                    if(ec) {
                        clear("BytedanceASRV2 connect:", ec);
                        co_return false;
                    }
                    // Set a timeout on the operation
                    beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(30));
                    if(!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), host.c_str())) {
                        auto ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                            net::error::get_ssl_category());
                        clear("BytedanceASRV2 ssl:", ec);
                        co_return false;
                    }
                    std::tie(ec) = co_await ws_->next_layer().async_handshake(ssl::stream_base::client, net::as_tuple(net::use_awaitable));
                    if(ec) {
                        clear("BytedanceASRV2 ssl handshake:", ec);
                        co_return false;
                    }
                    beast::get_lowest_layer(*ws_).expires_never();
                    ws_->set_option(
                        websocket::stream_base::timeout::suggested(
                            beast::role_type::client));
                    ws_->set_option(websocket::stream_base::decorator(
                        [&token = access_token_](websocket::request_type& req) {
                            req.set(http::field::authorization, std::string("Bearer; ") + token);
                        }));
                    std::tie(ec) = co_await ws_->async_handshake(host + ':' + port, path, net::as_tuple(net::use_awaitable));
                    if(ec) {
                        clear("BytedanceASRV2 handshake:", ec);
                        co_return false;
                    }
                    co_return co_await send_full_client();
                }

                net::awaitable<bool> send_full_client() {
                    BOOST_LOG_TRIVIAL(debug) << "BytedanceASRV2 send full client";
                    auto uuid = tools::generate_uuid();
                    json::value obj = {
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
                    auto payload_str = json::serialize(obj);
                    auto payload = std::vector<uint8_t>(payload_str.begin(), payload_str.end());
                    auto data = build_payload(0x1, 0x0, payload);
                    ws_->binary(true);
                    auto [ec, bytes_transferred] = co_await ws_->async_write(net::buffer(data), net::as_tuple(net::use_awaitable));
                    if(ec) {
                        clear("BytedanceASRV2 send full client:", ec);
                        co_return false;
                    }
                    beast::flat_buffer buffer;
                    std::tie(ec, bytes_transferred) = co_await ws_->async_read(buffer, net::as_tuple(net::use_awaitable));
                    if(ec) {
                        clear("BytedanceASRV2 recv full server:", ec);
                        co_return false;
                    }
                    auto rej = parse_response(beast::buffers_to_string(buffer.data()), is_gzip);
                    if(!rej || rej->at("code").as_int64() != 1000) {
                        clear("");
                        BOOST_LOG_TRIVIAL(error) << "BytedanceASRV2 websocket full connect error:" << (rej ? boost::json::serialize(rej.value()) : "null response.");
                        co_return false;
                    }
                    ws_state_ = WebsocketState::Connected;
                    BOOST_LOG_TRIVIAL(info) << "BytedanceASRV2 websocket connected";
                    co_return true;
                }

                net::awaitable<std::string> send(std::vector<uint8_t>& audio, bool is_last) {
                    std::string sentence;
                    std::vector<uint8_t> data = build_payload(0x2, is_last ? 0x2 : 0x0, audio);
                    ws_->binary(true);
                    auto [ec, bytes_transferred] = co_await ws_->async_write(net::buffer(data), net::as_tuple(net::use_awaitable));
                    if(ec) {
                        clear("BytedanceASRV2 send audio:", ec);
                        co_return sentence;
                    }
                    if(is_last) {
                        while(true) {
                            beast::flat_buffer buffer;
                            std::tie(ec, bytes_transferred) = co_await ws_->async_read(buffer, net::as_tuple(net::use_awaitable));
                            if(ec) {
                                clear("BytedanceASRV2 recv server:", ec);
                                co_return sentence;
                            }
                            auto rej = parse_response(beast::buffers_to_string(buffer.data()), is_gzip);
                            if(!rej || rej->at("code").as_int64() != 1000) {
                                BOOST_LOG_TRIVIAL(error) << "BytedanceASRV2 recv error:" << (rej ?  json::serialize(rej.value()) : "");
                                co_return sentence;
                            }
                            if(rej->contains("result") && rej->at("sequence").as_int64() < 0) {
                                auto& result = rej->at("result").as_array();
                                BOOST_LOG_TRIVIAL(info) << "BytedanceASRV2 detect:" << json::serialize(rej.value());
                                for(const auto &item : result) {
                                    sentence += item.at("text").as_string();
                                }
                                co_return sentence;
                            }
                        }
                    }
                    co_return sentence;
                }
            public:
                Impl(const net::any_io_executor& executor, const YAML::Node& config):
                    appid_(config["appid"].as<std::string>()),
                    access_token_(config["access_token"].as<std::string>()),
                    cluster_(config["cluster"].as<std::string>()),
                    ctx_(ssl::context::tlsv13_client),
                    executor_(executor) {
                        ctx_.set_verify_mode(ssl::verify_peer);
                        ctx_.set_default_verify_paths();
                }

                ~Impl() {
                    BOOST_LOG_TRIVIAL(debug) << "BytedanceASRV2 destroyed";
                }

                net::awaitable<std::string> detect_opus(const std::optional<beast::flat_buffer>& buf) {
                    std::string sentence;
                    if(ws_state_ == WebsocketState::Idle) {
                        if(!co_await connect()) {
                            co_return sentence;
                        }
                        ogg_streamer_ = std::make_unique<OggOpusStreamer>();
                        auto data = ogg_streamer_->build_id_headers();
                        co_await send(data, false);
                        data = ogg_streamer_->build_comment_headers();
                        co_await send(data, false);
                    }
                    if(!buf) {
                        auto data = ogg_streamer_->process_buffer({}, true);
                        sentence = co_await send(data, true);
                        auto [ec] = co_await ws_->async_close(websocket::close_code::normal, net::as_tuple(net::use_awaitable));
                        clear("BytedanceASRV2 websocket close:", ec);
                        BOOST_LOG_TRIVIAL(info) << "BytedanceASRV2 websocket closed";
                    } else {
                        auto data = ogg_streamer_->process_buffer(buf.value(), false);
                        co_await send(data, false);
                    }
                    co_return sentence;
                }
        };

        BytedanceV2::BytedanceV2(const net::any_io_executor& executor, const YAML::Node& config) {
            impl_ = std::make_unique<Impl>(executor, config);
        }
        
        BytedanceV2::~BytedanceV2() {

        }

        net::awaitable<std::string> BytedanceV2::detect_opus(const std::optional<beast::flat_buffer>& buf) {
            co_return co_await impl_->detect_opus(buf);
        }
    }
}