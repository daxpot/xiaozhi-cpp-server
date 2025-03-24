#include <boost/json/serialize.hpp>
#include <boost/log/trivial.hpp>
#include <iostream>
#include <vector>
#include <xz-cpp-server/llm/base.h>
#include <xz-cpp-server/config/logger.h>
#include <iostream>
#include <string>

net::awaitable<void> test() {
    auto executor = co_await net::this_coro::executor;
    auto llm = xiaozhi::llm::createLLM(executor);
    auto session_id = co_await llm->create_session();
    BOOST_LOG_TRIVIAL(info) << "session_id:" << session_id;
    boost::json::array dialogue = {
        boost::json::object{{"role", "system"}, {"content", R"(你是一个叫小智/小志的台湾女孩，说话机车，声音好听，习惯简短表达，爱用网络梗。
  请注意，要像一个人一样说话，请不要回复表情符号、代码、和xml标签。
  现在我正在和你进行语音聊天，我们开始吧。
  如果用户希望结束对话，请在最后说“拜拜”或“再见”。)"}},
        boost::json::object{{"role", "user"}, {"content", "你好，小智。今天天气怎么样"}}
    };
    co_await llm->response(dialogue, [](const std::string_view text) {
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