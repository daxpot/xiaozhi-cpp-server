#include <mutex>
#include <string>
#include <xz-cpp-server/config/setting.h>

static std::shared_ptr<xiaozhi::Setting> setting = nullptr;
static std::once_flag settingFlag;

std::shared_ptr<xiaozhi::Setting> xiaozhi::Setting::getSetting() {
    std::call_once(settingFlag, [&] {
        setting = std::shared_ptr<Setting>(new Setting());
    });
    return setting;
}

xiaozhi::Setting::Setting() {
    config = YAML::LoadFile("config.yaml");
}