#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <xz-cpp-server/tts/base.h>
#include <xz-cpp-server/config/logger.h>
#include <iostream>
#include <fstream>
#include <string>

bool write_binary_to_file(const std::string& filename, const std::vector<std::string>& data) {
    // 以二进制模式打开文件
    std::ofstream out_file(filename, std::ios::binary);
    
    // 检查文件是否成功打开
    if (!out_file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    // 写入数据
    for(auto& item : data) {
        out_file.write(item.data(), item.size());
    } 

    // 检查写入是否成功
    if (!out_file.good()) {
        std::cerr << "Error occurred while writing to file: " << filename << std::endl;
        return false;
    }

    // 关闭文件（析构时会自动关闭，但显式关闭是好习惯）
    out_file.close();
    return true;
}

net::awaitable<void> test() {
    auto executor = co_await net::this_coro::executor;
    auto asr = xiaozhi::tts::createTTS(executor);
    auto audio = co_await asr->text_to_speak("你好小智，我是你的朋友");
    // write_binary_to_file("../tmp/test.opus", audio);
    // std::cout << "test end" << std::endl;
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