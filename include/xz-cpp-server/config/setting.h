#pragma once
#include <yaml-cpp/yaml.h>
#include <memory>

namespace xiaozhi {
    class Setting {
        public:
            static std::shared_ptr<Setting> getSetting();
            // ~Setting();
            YAML::Node config;
        private:
            Setting();
    };
}
