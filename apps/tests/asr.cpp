#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/log/trivial.hpp>
#include <fstream>
#include <xz-cpp-server/config/setting.h>
#include <xz-cpp-server/asr/base.h>
#include <xz-cpp-server/config/logger.h>

net::awaitable<void> test() {
    auto executor = co_await net::this_coro::executor;
    auto setting = xiaozhi::Setting::getSetting();
    auto asr = xiaozhi::asr::createASR(executor);
    for(size_t index=0; index <= 92; index++) {
        std::ifstream file(std::format("tmp/example/opus_data_{}.opus", index), std::ifstream::binary);
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        beast::flat_buffer buffer;
        // 准备写入数据
        auto writable = buffer.prepare(data.size()); // 分配空间
        net::buffer_copy(writable, net::buffer(data)); // 复制数据
        buffer.commit(data.size()); // 提交数据
        co_await asr->detect_opus(buffer);
    }
    auto text = co_await asr->detect_opus(std::nullopt);
    BOOST_LOG_TRIVIAL(info) << "asr detect:" << text;
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