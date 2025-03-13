#include <iostream>
#include <xz-cpp-server/asr/doubao.h>
#include <xz-cpp-server/config/logger.h>

net::awaitable<void> test() {
    auto asr = co_await xiaozhi::DoubaoASR::createInstance();
    asr->connect();
    std::cout << "test end" << std::endl;
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