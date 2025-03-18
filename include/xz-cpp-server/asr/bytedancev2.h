#pragma once
#include <xz-cpp-server/asr/base.h>
#include <boost/asio.hpp>
#include <yaml-cpp/yaml.h>

namespace xiaozhi {
    namespace asr {
        class BytedanceV2: public Base {
            public:
                BytedanceV2(const net::any_io_executor& executor, const YAML::Node& config);
                ~BytedanceV2();
                void detect_opus(const std::optional<beast::flat_buffer>& buf) override;
                void on_detect(const std::function<void(std::string)>& callback) override;
            private:
                class Impl;
                std::unique_ptr<Impl> impl_;
        };
    }
}