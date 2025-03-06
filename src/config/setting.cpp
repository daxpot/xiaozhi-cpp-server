#include <mutex>
#include <string>
#include <xz-cpp-server/config/setting.h>

static std::shared_ptr<Setting> setting = nullptr;
static std::once_flag settingFlag;

std::shared_ptr<Setting> Setting::getSetting() {
    std::call_once(settingFlag, [&] {
        setting = std::shared_ptr<Setting>(new Setting());
    });
    return setting;
}

Setting::Setting() {
    config = YAML::LoadFile("config.yaml");
}