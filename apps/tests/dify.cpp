#include <iostream>
#include <vector>
#include <xz-cpp-server/llm/dify.h>
#include <xz-cpp-server/config/logger.h>
#include <iostream>
#include <string>

net::awaitable<void> test() {
    auto executor = co_await net::this_coro::executor;
    auto llm = xiaozhi::llm::Dify(executor);
    auto session_id = co_await llm.create_session();
    BOOST_LOG_TRIVIAL(info) << "session_id:" << session_id;
    std::vector<xiaozhi::llm::Dialogue> dialogue = {
        {"user", "你好，小智"}
    };
    co_await llm.response(dialogue, [](const std::string_view text) {
        BOOST_LOG_TRIVIAL(info) << text;
    });
}

int main() {
    init_logging("DEBUG");
    boost::asio::io_context ioc;
    net::co_spawn(ioc, test(), [](std::exception_ptr e) {
        if(e) {
            try {
                std::rethrow_exception(e);
            } catch(std::exception const& e) {
                std::cerr<< "Test error:" << e.what();
            }
        }
    });
    ioc.run();
}