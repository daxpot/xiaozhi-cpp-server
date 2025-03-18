#pragma once
#include <string>
#include <boost/asio.hpp>
namespace net = boost::asio;

namespace xiaozhi {
    namespace llm {
        struct Dialogue {
            std::string role;
            std::string content;
        };

        class Base {
            public:
                virtual ~Base() = default;
                virtual net::awaitable<std::string> create_session() = 0;
                virtual net::awaitable<void> response(const std::vector<Dialogue>& dialogue, const std::function<void(std::string)>& callback) = 0;
        };
    }
}