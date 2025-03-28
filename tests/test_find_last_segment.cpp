#include <catch2/catch_test_macros.hpp>
#include <xz-cpp-server/common/tools.h>

TEST_CASE("find last segment") {
    std::string txt = ".";
    auto p = tools::find_last_segment(txt);
    REQUIRE(p == 0);
    txt = "。";
    p = tools::find_last_segment(txt);
    REQUIRE(p == 2);    //只有一个标点符号的情况无视错误

    txt = "你好，我的世界。";
    p = tools::find_last_segment(txt);
    REQUIRE(p == 23);

    txt = "你好，我的世界";
    p = tools::find_last_segment(txt);
    REQUIRE(p == 8);

    txt = "。你好我的世界";
    p = tools::find_last_segment(txt);
    REQUIRE(p == 2);

    txt = "你好,我的世界.";
    p = tools::find_last_segment(txt);
    REQUIRE(p == 19);

    txt = "你好,我的世界";
    p = tools::find_last_segment(txt);
    REQUIRE(p == 6);

    txt = ".你好我的世界";
    p = tools::find_last_segment(txt);
    REQUIRE(p == 0);

    txt = "你好、我的世界";
    p = tools::find_last_segment(txt);
    REQUIRE(p == 8);
    txt = "你好，我的世界！";
    p = tools::find_last_segment(txt);
    REQUIRE(p == 23);
    txt = "你好，我的世界？";
    p = tools::find_last_segment(txt);
    REQUIRE(p == 23);
    txt = "你好，我的世界；";
    p = tools::find_last_segment(txt);
    REQUIRE(p == 23);
    txt = "你好，我的世界：";
    p = tools::find_last_segment(txt);
    REQUIRE(p == 23);
}