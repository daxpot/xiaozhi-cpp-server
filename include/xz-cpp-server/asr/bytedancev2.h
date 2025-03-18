#pragma once
#include <xz-cpp-server/asr/base.h>
#include <boost/asio.hpp>

namespace xiaozhi {
    namespace asr {
        class BytedanceV2: public Base {
            public:
                BytedanceV2(net::any_io_executor& executor, YAML::Node config);
                ~BytedanceV2();
                void detect_opus(const std::optional<beast::flat_buffer>& buf) override;
                void on_detect(const std::function<void(std::string)>& callback) override;
            private:
                class Impl;
                std::unique_ptr<Impl> impl_;
        };
    }
}