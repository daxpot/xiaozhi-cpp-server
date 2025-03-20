#pragma once
#include <yaml-cpp/yaml.h>
#include <memory>

namespace xiaozhi {
    class Setting {
        public:
            static std::shared_ptr<Setting> getSetting(const char* path=0);
            // ~Setting();
            YAML::Node config;
        private:
            Setting(const char* path=0);
    };
}
