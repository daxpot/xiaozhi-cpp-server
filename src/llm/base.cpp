#include <xz-cpp-server/llm/base.h>
#include <xz-cpp-server/llm/cozev3.h>
#include <xz-cpp-server/llm/dify.h>
#include <xz-cpp-server/config/setting.h>

namespace xiaozhi {
    namespace llm {
        std::unique_ptr<Base> createLLM(const net::any_io_executor& executor) {
            auto setting = Setting::getSetting();
            return std::make_unique<CozeV3>(executor, setting->config["LLM"]["CozeLLMV3"]);
        }
    }
}