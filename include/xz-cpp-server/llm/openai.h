#pragma once
#include <xz-cpp-server/llm/base.h>
#include <yaml-cpp/yaml.h>

namespace xiaozhi {
    namespace llm {
        class Openai: public Base {
            public:
                Openai(const net::any_io_executor &executor, const YAML::Node& config);
                ~Openai();
                net::awaitable<std::string> create_session() override;
                net::awaitable<void> response(const boost::json::array& dialogue, const std::function<void(std::string_view)>& callback) override;
            private:
                class Impl;
                std::unique_ptr<Impl> impl_;
        };
    }
}