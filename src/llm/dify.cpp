#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <format>
#include <memory>
#include <xz-cpp-server/llm/dify.h>
#include <xz-cpp-server/config/setting.h>
#include <boost/beast.hpp>
#include <xz-cpp-server/common/request.h>
#include <nlohmann/json.hpp>

namespace xiaozhi {
    namespace llm {
        class Dify::Impl {
            private:
                std::string conversation_id_;
                const std::string base_url_;
                const std::string api_key_;
                nlohmann::json header_;
            public:
                Impl(std::shared_ptr<Setting> setting, net::any_io_executor &executor):
                    base_url_(setting->config["LLM"]["DifyLLM"]["base_url"].as<std::string>()),
                    api_key_(setting->config["LLM"]["DifyLLM"]["api_key"].as<std::string>()),
                    header_({
                        {"Authorization", std::format("Bearer {}", api_key_)},
                        {"Content-Type", "application/json"},
                        {"Connection", "keep-alive"}
                    }) {
                }

                net::awaitable<std::string> create_session() {
                    co_return "";
                }

                net::awaitable<void> response(const std::vector<Dialogue>& dialogue, const std::function<void(std::string)>& callback) {
                    std::string query;
                    auto it = std::find_if(dialogue.rbegin(), dialogue.rend(), [](auto& x) {return x.role == "user";});
                    if(it != dialogue.rend()) {
                        query = it->content;
                    }
                    const std::string url = std::format("{}/chat-messages", base_url_);
                    nlohmann::json data = {
                        {"query", std::move(query)},
                        {"response_mode", "streaming"},
                        {"user", "143523"},
                        {"inputs", nlohmann::json({})}
                    };
                    co_await request::stream_post(url, header_, data.dump(), [&callback](std::string res) {
                        std::vector<std::string> result;
                        boost::split(result, res, boost::is_any_of("\n"));
                        for(auto& line : result) {
                            if(line.size() == 0)
                                continue;
                            if(line.starts_with("data:")) {
                                auto rej = nlohmann::json::parse(line.begin()+5, line.end());
                                if(rej["event"] == "error") {
                                    BOOST_LOG_TRIVIAL(error) << "Dify api error:" << line;
                                } else if(!rej["answer"].empty()) {
                                    callback(rej["answer"].get<std::string>());
                                }
                            }
                        }
                    });
                }
                
        };

        Dify::Dify(net::any_io_executor &executor) {
            auto setting = Setting::getSetting();
            impl_ = std::make_unique<Impl>(setting, executor);
        }

        Dify::~Dify() {
            
        }

        net::awaitable<std::string> Dify::create_session() {
            co_return co_await impl_->create_session();
        }

        net::awaitable<void> Dify::response(const std::vector<Dialogue>& dialogue, const std::function<void(std::string)>& callback) {
            co_await impl_->response(dialogue, callback);
        }
    }
}