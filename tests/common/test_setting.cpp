#include <catch2/catch_test_macros.hpp>
#include <xz-cpp-server/common/setting.h>

TEST_CASE("load setting") {
    auto setting = xiaozhi::Setting::getSetting();
    REQUIRE(setting->config["server"]["ip"].as<std::string>() == "0.0.0.0");
    REQUIRE(setting->config["server"]["port"].as<int>() == 8000);
}