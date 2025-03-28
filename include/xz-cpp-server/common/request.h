#pragma once

namespace request {
    struct UrlInfo {
        bool is_https;
        std::string host;
        std::string port;
        std::string path;
    };
    UrlInfo parse_url(const std::string& url);
    net::awaitable<ssl::stream<beast::tcp_stream>> connect(const UrlInfo& url_info);
    net::awaitable<void> send(ssl::stream<beast::tcp_stream>& stream, const http::verb method, const UrlInfo& url_info, const json::value& header, const std::string& data="");
    net::awaitable<std::string> request(const http::verb method, const std::string& url, const json::value& header, const std::string& data="");
    net::awaitable<std::string> get(const std::string& url, const json::value& header);
    net::awaitable<std::string> post(const std::string& url, const json::value& header, const std::string& data);
    net::awaitable<void> stream_post(const std::string& url, const json::value& header, const std::string& data, const std::function<void(std::string)>& callback);
}