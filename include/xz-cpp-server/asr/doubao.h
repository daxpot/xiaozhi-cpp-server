#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <optional>
namespace net = boost::asio;
namespace beast = boost::beast;

namespace xiaozhi {
    class DoubaoASR:public std::enable_shared_from_this<DoubaoASR> {
        private:
            class ASRImpl;
            std::unique_ptr<ASRImpl> impl_;
        public:
            DoubaoASR(net::any_io_executor &executor);
            static net::awaitable<std::shared_ptr<DoubaoASR>> createInstance();
            void connect();
            void close();
            void send_opus(std::optional<beast::flat_buffer> &buf);
    };
}