#include <xz-cpp-server/asr/base.h>
#include <xz-cpp-server/asr/bytedancev2.h>
#include <xz-cpp-server/asr/paraformer.h>
#include <xz-cpp-server/common/setting.h>

namespace xiaozhi {
    namespace asr {
        std::unique_ptr<Base> createASR(const net::any_io_executor& executor) {
            auto setting = Setting::getSetting();
            auto selected_module = setting->config["selected_module"]["ASR"].as<std::string>();
            if(selected_module == "BytedanceASRV2") {
                return std::make_unique<BytedanceV2>(executor, setting->config["ASR"][selected_module]);
            } else if(selected_module == "Paraformer") {
                return std::make_unique<Paraformer>(executor, setting->config["ASR"][selected_module]);
            } else {
                throw std::invalid_argument("Selected_module ASR not be supported");
            }
        }
        Base::~Base() {
            BOOST_LOG_TRIVIAL(debug) << "ASR base destroyed";
        }
    }
}