#pragma once
#include <yaml-cpp/yaml.h>
#include <memory>

class Setting {
    public:
        static std::shared_ptr<Setting> getSetting();
        // ~Setting();
        YAML::Node config;
    private:
        Setting();
};