#pragma once

namespace xiaozhi {
    namespace asr {
        class Base {
            public:
                virtual ~Base();
                virtual net::awaitable<std::string> detect_opus(const std::optional<beast::flat_buffer>& buf) = 0;
        };
        std::unique_ptr<Base> createASR(const net::any_io_executor& executor);
    }
}