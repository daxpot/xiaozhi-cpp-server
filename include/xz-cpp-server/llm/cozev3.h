#pragma once
#include <xz-cpp-server/llm/base.h>

namespace xiaozhi {
    namespace llm {
        class CozeV3: public Base {
            public:
                CozeV3(net::any_io_executor &executor);
                ~CozeV3();
                net::awaitable<std::string> create_session() override;
                net::awaitable<void> response(const std::vector<Dialogue>& dialogue, const std::function<void(std::string_view)>& callback) override;
            private:
                class Impl;
                std::unique_ptr<Impl> impl_;
        };
    }
}