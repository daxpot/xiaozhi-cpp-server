#pragma once
#include <boost/beast.hpp>
namespace net = boost::asio;
namespace beast = boost::beast;

namespace xiaozhi {
    namespace asr {
        class Base {
            public:
                virtual ~Base()=default;
                virtual void detect_opus(std::optional<beast::flat_buffer> buf) = 0;
                virtual void on_detect(const std::function<void(std::string)>& callback) = 0;
        };
        std::unique_ptr<Base> createASR(const net::any_io_executor& executor);
    }
}