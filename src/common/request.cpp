#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/http/verb.hpp>
#include <iostream>
#include <xz-cpp-server/common/request.h>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <regex>
namespace ssl = net::ssl;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

namespace request {

    struct UrlInfo {
        bool is_https;
        std::string host;
        std::string port;
        std::string path;
    };

    static UrlInfo parse_url(const std::string& url) {
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

    net::awaitable<std::string> req(const http::verb method, const UrlInfo url_info, const nlohmann::basic_json<>& header, const std::string& data="") {
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

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::dynamic_body> res;
        if(url_info.is_https) {
            co_await http::async_read(stream, buffer, res, net::use_awaitable);
            auto [ec] = co_await stream.async_shutdown(net::as_tuple(net::use_awaitable));

            if(ec && ec != net::ssl::error::stream_truncated)
                throw boost::system::system_error(ec, "shutdown");
        } else {
            co_await http::async_read(stream.next_layer(), buffer, res, net::use_awaitable);
            beast::error_code ec;
            auto r = stream.next_layer().socket().shutdown(net::ip::tcp::socket::shutdown_both, ec);
            if(ec && ec != beast::errc::not_connected)
                throw boost::system::system_error(ec, "shutdown");
        }
        
        std::string ret;
        ret.reserve(res.body().size());
        for(auto buf : res.body().data()) {
            ret.append(static_cast<const char*>(buf.data()), buf.size());
        }
        co_return ret;
    }
    
    net::awaitable<std::string> get(const std::string& url, const nlohmann::basic_json<>& header) {
        return req(http::verb::get, parse_url(url), header, "");
    }

    net::awaitable<std::string> post(const std::string& url, const nlohmann::basic_json<>& header, const std::string& data) {
        return req(http::verb::post, parse_url(url), header, data);
    }
}