#pragma once
#include <xz-cpp-server/llm/base.h>
#include <yaml-cpp/yaml.h>

namespace xiaozhi {
    namespace llm {
        class Dify: public Base {
            public:
                Dify(net::any_io_executor &executor, const YAML::Node& config);
                ~Dify();
                net::awaitable<std::string> create_session() override;
                net::awaitable<void> response(const std::vector<Dialogue>& dialogue, const std::function<void(std::string_view)>& callback) override;
            private:
                class Impl;
                std::unique_ptr<Impl> impl_;
        };
    }
}