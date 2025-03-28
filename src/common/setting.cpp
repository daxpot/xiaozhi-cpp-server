#include <xz-cpp-server/common/setting.h>

static std::shared_ptr<xiaozhi::Setting> setting = nullptr;
static std::once_flag settingFlag;

std::shared_ptr<xiaozhi::Setting> xiaozhi::Setting::getSetting(const char* path) {
    std::call_once(settingFlag, [&] {
        setting = std::shared_ptr<Setting>(new Setting(path));
    });
    return setting;
}

xiaozhi::Setting::Setting(const char* path) {
    config = YAML::LoadFile(path == 0 ? "config.yaml" : path);
}