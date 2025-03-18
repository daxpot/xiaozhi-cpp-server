#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/log/trivial.hpp>
#include <format>
#include <functional>
#include <boost/algorithm/string.hpp>
#include <memory>
#include <string>
#include <vector>
#include <xz-cpp-server/llm/cozev3.h>
#include <xz-cpp-server/config/setting.h>
#include <boost/beast.hpp>
#include <xz-cpp-server/common/request.h>
#include <nlohmann/json.hpp>

namespace xiaozhi {
    namespace llm {
        class CozeV3::Impl {
            private:
                std::string conversation_id_;
                std::string bot_id_;
                std::string user_id_;
                // std::string access_token_;
                nlohmann::json header_;
            public:
                Impl(std::shared_ptr<Setting> setting, net::any_io_executor &executor):
                    bot_id_(setting->config["LLM"]["CozeLLMV3"]["bot_id"].as<std::string>()),
                    user_id_(setting->config["LLM"]["CozeLLMV3"]["user_id"].as<std::string>()),
                    header_({
                        {"Authorization", std::format("Bearer {}", setting->config["LLM"]["CozeLLMV3"]["personal_access_token"].as<std::string>())},
                        {"Content-Type", "application/json"},
                        {"Connection", "keep-alive"}
                    }) {
                }

                net::awaitable<std::string> create_session() {
                    const std::string url = "https://api.coze.cn/v1/conversation/create";
                    auto res = co_await request::post(url, header_, "");
                    auto rej = nlohmann::json::parse(res);
                    if(!rej["code"].is_number_integer() || rej["code"] != 0) {
                        BOOST_LOG_TRIVIAL(error) << "CozeV3 create session failed:" << res;
                        co_return "";
                    }
                    conversation_id_ = rej["data"]["id"];
                    co_return conversation_id_;
                }

                net::awaitable<void> response(const std::vector<Dialogue>& dialogue, const std::function<void(std::string)>& callback) {
                    std::string query;
                    auto it = std::find_if(dialogue.rbegin(), dialogue.rend(), [](auto& x) {return x.role == "user";});
                    if(it != dialogue.rend()) {
                        query = it->content;
                    }
                    const std::string url = std::format("https://api.coze.cn/v3/chat?conversation_id={}", conversation_id_);
                    nlohmann::json data = {
                        {"bot_id", bot_id_},
                        {"user_id", user_id_},
                        {"auto_save_history", true},
                        {"stream", true},
                        {"additional_messages", {
                            {
                                {"role", "user"},
                                {"content", std::move(query)}
                            }
                        }}
                    };
                    co_await request::stream_post(url, header_, data.dump(), [&callback](const std::string res) {
                        std::vector<std::string> result;
                        boost::split(result, res, boost::is_any_of("\n"));
                        bool is_delta = false;
                        for(auto& line : result) {
                            if(line.size() == 0)
                                continue;
                            if(line == "event:conversation.message.delta") {
                                is_delta = true;
                                continue;
                            }
                            if(is_delta && line.starts_with("data:")) {
                                auto rej = nlohmann::json::parse(line.begin()+5, line.end());
                                if(rej["role"] == "assistant" && rej["type"] == "answer") {
                                    callback(rej["content"].get<std::string>());
                                }
                            }
                        }
                    });
                }
                
        };

        CozeV3::CozeV3(net::any_io_executor &executor) {
            auto setting = Setting::getSetting();
            impl_ = std::make_unique<Impl>(setting, executor);
        }

        CozeV3::~CozeV3() {
            
        }

        net::awaitable<std::string> CozeV3::create_session() {
            co_return co_await impl_->create_session();
        }

        net::awaitable<void> CozeV3::response(const std::vector<Dialogue>& dialogue, const std::function<void(std::string)>& callback) {
            co_await impl_->response(dialogue, callback);
        }
    }
}