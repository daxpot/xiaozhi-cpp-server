#include <xz-cpp-server/tts/base.h>
#include <xz-cpp-server/tts/bytedancev3.h>
#include <xz-cpp-server/config/setting.h>

namespace xiaozhi {
    namespace tts {
        std::unique_ptr<Base> createTTS(const net::any_io_executor& executor) {
            auto setting = Setting::getSetting();
            return std::make_unique<BytedanceV3>(executor, setting->config["TTS"]["BytedanceTTS"]);
        }
    }
}