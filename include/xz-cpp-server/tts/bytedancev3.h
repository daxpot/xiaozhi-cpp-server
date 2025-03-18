#pragma once
#include <xz-cpp-server/tts/base.h>
#include <boost/asio.hpp>
#include <yaml-cpp/yaml.h>

namespace xiaozhi {
    namespace tts {
        class BytedanceV3: public Base {
            public:
                BytedanceV3(const net::any_io_executor& executor, const YAML::Node& config);
                ~BytedanceV3();
                net::awaitable<std::vector<std::vector<uint8_t>>> text_to_speak(const std::string& text) override;
            private:
                class Impl;
                std::unique_ptr<Impl> impl_;
        };
    }
}