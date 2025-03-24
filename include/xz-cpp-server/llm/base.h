#pragma once
#include <string>
#include <boost/asio.hpp>
#include <boost/json/array.hpp>
namespace net = boost::asio;

namespace xiaozhi {
    namespace llm {
        class Base {
            public:
                virtual ~Base() = default;
                virtual net::awaitable<std::string> create_session() = 0;
                virtual net::awaitable<void> response(const boost::json::array& dialogue, const std::function<void(std::string_view)>& callback) = 0;
        };
        std::unique_ptr<Base> createLLM(const net::any_io_executor& executor);
    }
}