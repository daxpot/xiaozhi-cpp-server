#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <future>
#include <xz-cpp-server/common/request.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_all.hpp>

TEST_CASE("request get") {
    net::io_context ioc;
    std::promise<std::string> promise;
    auto future = promise.get_future();
    net::co_spawn(ioc, [&promise]() -> net::awaitable<void> {
        nlohmann::json header = {
            {"Content-Type", "application/json"}
        };
        auto ret = co_await request::get("https://echo.websocket.org/test?v=1", header);
        promise.set_value(ret);
    }, net::detached);
    ioc.run();
    std::string ret = future.get();
    // REQUIRE(ret.find("Content-Type: application/json") != std::string::npos);
    REQUIRE_THAT(ret, Catch::Matchers::ContainsSubstring("Content-Type") && Catch::Matchers::ContainsSubstring("application/json"));
}

TEST_CASE("request post") {
    net::io_context ioc;
    std::promise<std::string> promise;
    auto future = promise.get_future();
    net::co_spawn(ioc, [&promise]() -> net::awaitable<void> {
        nlohmann::json header = {
            {"Content-Type", "application/json"}
        };
        auto ret = co_await request::post("https://echo.websocket.org/test?v=1", header, R"({"test": "value"})");
        promise.set_value(ret);
    }, net::detached);
    ioc.run();
    std::string ret = future.get();
    // REQUIRE(ret.find("Content-Type: application/json") != std::string::npos);
    REQUIRE_THAT(ret, Catch::Matchers::ContainsSubstring("Content-Type") 
            && Catch::Matchers::ContainsSubstring("application/json")
            && Catch::Matchers::ContainsSubstring("test")
            && Catch::Matchers::ContainsSubstring("value"));
}
TEST_CASE("request stream get") {
    net::io_context ioc;
    std::promise<std::string> promise;
    auto future = promise.get_future();
    net::co_spawn(ioc, [&promise]() -> net::awaitable<void> {
        nlohmann::json header = {
            {"Content-Type", "application/json"}
        };
        std::string value;
        co_await request::stream_get("https://echo.websocket.org/test?v=1", header, [&promise, &value](std::span<const char> data) {
            value.append(data.data(), data.size());
        });
        promise.set_value(value);
    }, net::detached);
    ioc.run();
    std::string ret = future.get();
    // REQUIRE(ret.find("Content-Type: application/json") != std::string::npos);
    REQUIRE_THAT(ret, Catch::Matchers::ContainsSubstring("Content-Type") 
            && Catch::Matchers::ContainsSubstring("application/json"));
}

TEST_CASE("request stream post") {
    net::io_context ioc;
    std::promise<std::string> promise;
    auto future = promise.get_future();
    net::co_spawn(ioc, [&promise]() -> net::awaitable<void> {
        nlohmann::json header = {
            {"Content-Type", "application/json"}
        };
        std::string value;
        co_await request::stream_post("https://echo.websocket.org/test?v=1", header, R"({"stream": true})", [&promise, &value](std::span<const char> data) {
            value.append(data.data(), data.size());
        });
        promise.set_value(value);
    }, net::detached);
    ioc.run();
    std::string ret = future.get();
    // REQUIRE(ret.find("Content-Type: application/json") != std::string::npos);
    REQUIRE_THAT(ret, Catch::Matchers::ContainsSubstring("Content-Type") 
            && Catch::Matchers::ContainsSubstring("application/json")
            && Catch::Matchers::ContainsSubstring("stream")
            && Catch::Matchers::ContainsSubstring("true"));
}