#pragma once
#include <boost/asio/awaitable.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>

namespace net = boost::asio;

namespace request {
    net::awaitable<std::string> get(const std::string& url, const nlohmann::basic_json<>& header);
    net::awaitable<std::string> post(const std::string& url, const nlohmann::basic_json<>& header, const std::string& data);
}