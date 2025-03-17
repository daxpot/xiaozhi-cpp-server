#pragma once
#include <boost/asio/awaitable.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ssl = net::ssl;

namespace request {
    struct UrlInfo {
        bool is_https;
        std::string host;
        std::string port;
        std::string path;
    };
    UrlInfo parse_url(const std::string& url);
    net::awaitable<ssl::stream<beast::tcp_stream>> connect(const UrlInfo& url_info);
    net::awaitable<void> send(ssl::stream<beast::tcp_stream>& stream, const http::verb method, const UrlInfo& url_info, const nlohmann::basic_json<>& header, const std::string& data="");
    net::awaitable<std::string> request(const http::verb method, const std::string& url, const nlohmann::basic_json<>& header, const std::string& data="");
    net::awaitable<std::string> get(const std::string& url, const nlohmann::basic_json<>& header);
    net::awaitable<std::string> post(const std::string& url, const nlohmann::basic_json<>& header, const std::string& data);
    net::awaitable<void> stream_request(const http::verb method, const std::string& url, const nlohmann::basic_json<>& header, const std::string& data, std::function<void(std::span<const char>)> callback);
    net::awaitable<void> stream_get(const std::string& url, const nlohmann::basic_json<>& header, std::function<void(std::span<const char>)> callback);
    net::awaitable<void> stream_post(const std::string& url, const nlohmann::basic_json<>& header, const std::string& data, std::function<void(std::span<const char>)> callback);
}