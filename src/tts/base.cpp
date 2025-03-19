#include <xz-cpp-server/tts/base.h>
#include <xz-cpp-server/tts/bytedancev3.h>
#include <xz-cpp-server/config/setting.h>

namespace xiaozhi {
    namespace tts {
        std::unique_ptr<Base> createTTS(const net::any_io_executor& executor) {
            auto setting = Setting::getSetting();
            auto selected_module = setting->config["selected_module"]["TTS"].as<std::string>();
            if(selected_module == "BytedanceTTSV3") {
                return std::make_unique<BytedanceV3>(executor, setting->config["TTS"][selected_module], setting->config["xiaozhi"]["audio_params"]["sample_rate"].as<int>());
            } else {
                throw std::invalid_argument("Selected_module TTS not be supported");
            }
        }
    }
}