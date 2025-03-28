#include <xz-cpp-server/server.h>
#include <xz-cpp-server/common/setting.h>
#include <xz-cpp-server/common/logger.h>


int main(int argc, char* argv[]) {
    auto setting = xiaozhi::Setting::getSetting(argc > 1 ? argv[1] : 0);
    init_logging(setting->config["log"]["log_level"].as<std::string>());
    auto server = xiaozhi::Server(setting);
    server.run();
    return 0;
}