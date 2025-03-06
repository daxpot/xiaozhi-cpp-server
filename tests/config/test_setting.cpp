#include "yaml-cpp/emitter.h"
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <xz-cpp-server/config/setting.h>

TEST_CASE("load setting") {
    auto setting = xiaozhi::Setting::getSetting();
    REQUIRE(setting->config["server"]["ip"].as<std::string>() == "0.0.0.0");
    REQUIRE(setting->config["server"]["port"].as<int>() == 8000);
    REQUIRE(setting->config["server"]["auth"]["enabled"].as<bool>() == false);
    REQUIRE(setting->config["server"]["auth"]["enabledxx"].IsDefined() == false);
    REQUIRE(setting->config["server"]["authxxx"]["enabled"].IsDefined() == false);
    for(auto i=0; i<setting->config["server"]["auth"]["tokens"].size(); ++i) {
        std::cout <<"token:"
                << setting->config["server"]["auth"]["tokens"][i]["token"].as<std::string>()
                << " name:"
                << setting->config["server"]["auth"]["tokens"][i]["name"].as<std::string>()
                << std::endl;
    }
    YAML::Emitter emitter;
    emitter << YAML::DoubleQuoted << YAML::Flow << YAML::BeginSeq << setting->config["xiaozhi"];
    REQUIRE(std::string(emitter.c_str() + 1) == "{\"type\": \"hello\", \"version\": \"1\", \"transport\": \"websocket\", \"audio_params\": {\"format\": \"opus\", \"sample_rate\": \"16000\", \"channels\": \"1\", \"frame_duration\": \"60\"}}");
}