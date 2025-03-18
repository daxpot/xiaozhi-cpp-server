#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/log/trivial.hpp>
#include <xz-cpp-server/config/setting.h>
#include <xz-cpp-server/asr/base.h>
#include <xz-cpp-server/config/logger.h>

net::awaitable<void> test() {
    auto executor = co_await net::this_coro::executor;
    auto setting = xiaozhi::Setting::getSetting();
    auto asr = xiaozhi::asr::createASR(executor);
    asr->detect_opus(std::nullopt);
    net::steady_timer timer1(executor, std::chrono::milliseconds(1000));
    co_await timer1.async_wait(net::use_awaitable);

    beast::flat_buffer buffer;
    // 准备写入数据
    std::string data = "Hello, world!";
    auto writable = buffer.prepare(data.size()); // 分配空间
    net::buffer_copy(writable, net::buffer(data)); // 复制数据
    buffer.commit(data.size()); // 提交数据

    asr->detect_opus(buffer);
    asr->detect_opus(std::nullopt);
    net::steady_timer timer2(executor, std::chrono::milliseconds(1000));
    co_await timer2.async_wait(net::use_awaitable);
    BOOST_LOG_TRIVIAL(info) << "test end";
    while(true) {
        net::steady_timer timer(executor, std::chrono::milliseconds(1000));
        co_await timer.async_wait(net::use_awaitable);
    }
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