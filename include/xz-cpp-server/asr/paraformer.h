#pragma once
#include <xz-cpp-server/asr/base.h>
#include <boost/asio.hpp>
#include <yaml-cpp/yaml.h>

namespace xiaozhi {
    namespace asr {
        class Paraformer: public Base {
            public:
                Paraformer(const net::any_io_executor& executor, const YAML::Node& config);
                ~Paraformer();
                net::awaitable<std::string> detect_opus(const std::optional<beast::flat_buffer>& buf) override;
            private:
                class Impl;
                std::unique_ptr<Impl> impl_;
        };
    }
}