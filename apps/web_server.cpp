#include <xz-cpp-server/server.h>
#include <xz-cpp-server/config/setting.h>

int main() {
    auto setting = Setting::getSetting();
    auto server = Server(setting);
    server.run();
    return 0;
}