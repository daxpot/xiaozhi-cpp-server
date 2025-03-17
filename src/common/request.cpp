#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http/verb.hpp>
#include <xz-cpp-server/common/request.h>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <regex>
using tcp = net::ip::tcp;

namespace request {
    UrlInfo parse_url(const std::string& url) {
        UrlInfo info{false, "", "", "/"};
        std::regex url_regex(R"(^(https?)://([^:/]+)(?::(\d+))?(/.*)?$)");
        std::smatch matches;
        if (std::regex_match(url, matches, url_regex)) {
            info.is_https = (matches[1] == "https");
            info.host = matches[2];
            info.port = matches[3].matched ? std::string(matches[3]) : (info.is_https ? "443" : "80");
            info.path = matches[4].matched ? std::string(matches[4]) : "/";
        } else {
            throw std::invalid_argument("Invalid URL format");
        }

        return info;
    }
    static ssl::context get_ssl_context() {
        ssl::context ctx{ssl::context::sslv23_client};
        ctx.set_verify_mode(ssl::verify_peer);
        ctx.set_default_verify_paths();
        return ctx;
    }

    net::awaitable<ssl::stream<beast::tcp_stream>> connect(const UrlInfo& url_info) {
        auto executor = co_await net::this_coro::executor;
        auto ctx = get_ssl_context();
        tcp::resolver resolver{executor};
        ssl::stream<beast::tcp_stream> stream{executor, ctx};

        if(url_info.is_https && !SSL_set_tlsext_host_name(stream.native_handle(), url_info.host.c_str())) {
            throw boost::system::system_error(
                static_cast<int>(::ERR_get_error()),
                net::error::get_ssl_category());
        }
        auto const results = co_await resolver.async_resolve(url_info.host, url_info.port, net::use_awaitable);
        beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));
        co_await beast::get_lowest_layer(stream).async_connect(results, net::use_awaitable);
            // Set the timeout.
        if(url_info.is_https) {
            beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));
            co_await stream.async_handshake(ssl::stream_base::client, net::use_awaitable);
        }
        co_return std::move(stream);
    }
    net::awaitable<void> send(ssl::stream<beast::tcp_stream>& stream, const http::verb method, const UrlInfo& url_info, const nlohmann::basic_json<>& header, const std::string& data) {
        http::request<http::string_body> req{ method, url_info.path, 11 };
        req.set(http::field::host, url_info.host);
        for(auto& item : header.items()) {
            req.set(item.key(), item.value());
        }
        if(method == http::verb::post) {
            req.body() = data;
            req.prepare_payload();
        }

        // Set the timeout.
        beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

        if(url_info.is_https) {
            co_await http::async_write(stream, req, net::use_awaitable);
        } else {
            co_await http::async_write(stream.next_layer(), req, net::use_awaitable);
        }
        co_return;
    }

    net::awaitable<void> close(ssl::stream<beast::tcp_stream>& stream, bool is_https) {
        if(is_https) {
            auto [ec] = co_await stream.async_shutdown(net::as_tuple(net::use_awaitable));
            if(ec && ec != net::ssl::error::stream_truncated)
                throw boost::system::system_error(ec, "shutdown");
        } else {
            beast::error_code ec;
            auto r = stream.next_layer().socket().shutdown(net::ip::tcp::socket::shutdown_both, ec);
            if(ec && ec != beast::errc::not_connected)
                throw boost::system::system_error(ec, "shutdown");
        }
    }

    net::awaitable<std::string> request(const http::verb method, const std::string& url, const nlohmann::basic_json<>& header, const std::string& data) {
        auto url_info = parse_url(url);
        auto stream = co_await connect(url_info);
        co_await send(stream, method, url_info, header, data);
        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::dynamic_body> res;
        if(url_info.is_https) {
            co_await http::async_read(stream, buffer, res, net::use_awaitable);
        } else {
            co_await http::async_read(stream.next_layer(), buffer, res, net::use_awaitable);
        }
        co_await close(stream, url_info.is_https);
        
        std::string ret;
        ret.reserve(res.body().size());
        for(auto buf : res.body().data()) {
            ret.append(static_cast<const char*>(buf.data()), buf.size());
        }
        co_return ret;
    }
    
    net::awaitable<std::string> get(const std::string& url, const nlohmann::basic_json<>& header) {
        co_return co_await request(http::verb::get, url, header);
    }

    net::awaitable<std::string> post(const std::string& url, const nlohmann::basic_json<>& header, const std::string& data) {
        co_return co_await request(http::verb::post, url, header, data);
    }

    net::awaitable<void> stream_request(const http::verb method, const std::string& url, const nlohmann::basic_json<>& header, const std::string& data, std::function<void(std::span<const char>)> callback) {
        auto url_info = parse_url(url);
        auto stream = co_await connect(url_info);
        co_await send(stream, method, url_info, header, data);

        beast::flat_buffer buffer;
        http::response_parser<http::dynamic_body> parser;
        parser.body_limit( 1 * 1024 * 1024);  //1MB
        co_await http::async_read_header(stream, buffer, parser, net::use_awaitable);

        while (!parser.is_done()) {
            beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));
            auto bytes_transferred = url_info.is_https
                ? co_await http::async_read_some(stream, buffer, parser, net::use_awaitable)
                : co_await http::async_read_some(stream.next_layer(), buffer, parser, net::use_awaitable);
            if (bytes_transferred > 0) {
                for (auto buf : parser.get().body().data()) {
                    callback(std::span<const char>(static_cast<const char*>(buf.data()), buf.size()));
                }
                parser.get().body().consume(bytes_transferred);
                buffer.consume(bytes_transferred);
            }
        }
        co_await close(stream, url_info.is_https);
    }

    net::awaitable<void> stream_get(const std::string& url, const nlohmann::basic_json<>& header, std::function<void(std::span<const char>)> callback) {
        co_return co_await stream_request(http::verb::get, url, header, "", callback);
    }

    net::awaitable<void> stream_post(const std::string& url, const nlohmann::basic_json<>& header, const std::string& data, std::function<void(std::span<const char>)> callback) {
        co_return co_await stream_request(http::verb::post, url, header, data, callback);
    }
}