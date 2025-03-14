#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <cstdint>
#include <memory>
#include <vector>
namespace net = boost::asio;

namespace xiaozhi {
    class BytedanceV3TTS {
        public:
            BytedanceV3TTS(net::any_io_executor &executor);
            ~BytedanceV3TTS();
            net::awaitable<std::vector<std::vector<uint8_t>>> text_to_speak(const std::string& text);
            // void on_detect(std::function<void(std::string)> callback);
        private:
            class TTSImpl;
            std::unique_ptr<TTSImpl> impl_;
    };
}