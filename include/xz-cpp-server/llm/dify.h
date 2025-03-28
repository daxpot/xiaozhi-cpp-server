#pragma once
#include "base.h"

namespace xiaozhi {
    namespace llm {
        class Dify: public Base {
            public:
                Dify(const net::any_io_executor &executor, const YAML::Node& config);
                ~Dify();
                net::awaitable<std::string> create_session() override;
                net::awaitable<void> response(const boost::json::array& dialogue, const std::function<void(std::string_view)>& callback) override;
            private:
                class Impl;
                std::unique_ptr<Impl> impl_;
        };
    }
}