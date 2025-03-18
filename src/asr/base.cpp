#include <xz-cpp-server/asr/base.h>
#include <xz-cpp-server/asr/bytedancev2.h>
#include <xz-cpp-server/config/setting.h>

namespace xiaozhi {
    namespace asr {
        std::unique_ptr<Base> createASR(const net::any_io_executor& executor) {
            auto setting = Setting::getSetting();
            return std::make_unique<BytedanceV2>(executor, setting->config["ASR"]["DoubaoASR"]);
        }
    }
}