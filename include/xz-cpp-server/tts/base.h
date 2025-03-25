#pragma once
#include <boost/asio/awaitable.hpp>
namespace net = boost::asio;

namespace xiaozhi {
    namespace tts {
        class Base {
            public:
                virtual ~Base();
                virtual net::awaitable<std::vector<std::vector<uint8_t>>> text_to_speak(const std::string& text) = 0;
        };
        std::unique_ptr<Base> createTTS(const net::any_io_executor& executor);
    }
}