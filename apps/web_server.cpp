#include <xz-cpp-server/server.h>
#include <xz-cpp-server/config/setting.h>
#include <xz-cpp-server/config/logger.h>


int main() {
    auto setting = xiaozhi::Setting::getSetting();
    init_logging(setting->config["log"]["log_level"].as<std::string>());
    auto server = xiaozhi::Server(setting);
    server.run();
    return 0;
}