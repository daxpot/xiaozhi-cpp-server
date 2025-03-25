#include <boost/log/trivial.hpp>
#include <stdexcept>
#include <xz-cpp-server/llm/base.h>
#include <xz-cpp-server/llm/cozev3.h>
#include <xz-cpp-server/llm/dify.h>
#include <xz-cpp-server/llm/openai.h>
#include <xz-cpp-server/config/setting.h>

namespace xiaozhi {
    namespace llm {
        std::unique_ptr<Base> createLLM(const net::any_io_executor& executor) {
            auto setting = Setting::getSetting();
            auto selected_module = setting->config["selected_module"]["LLM"].as<std::string>();
            if(selected_module == "CozeLLMV3") {
                return std::make_unique<CozeV3>(executor, setting->config["LLM"][selected_module]);
            } else if(selected_module == "DifyLLM") {
                return std::make_unique<Dify>(executor, setting->config["LLM"][selected_module]);
            } else if(setting->config["LLM"][selected_module]["type"].IsDefined() && setting->config["LLM"][selected_module]["type"].as<std::string>() == "openai") {
                return std::make_unique<Openai>(executor, setting->config["LLM"][selected_module]);
            } else {
                throw std::invalid_argument("Selected_module LLM not be supported");
            }
        }
        Base::~Base() {
            BOOST_LOG_TRIVIAL(debug) << "LLM base destroyed";
        }
    }
}