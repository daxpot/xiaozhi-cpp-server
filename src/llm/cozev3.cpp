#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/log/trivial.hpp>
#include <boost/system/detail/error_code.hpp>
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
#include <boost/json.hpp>

namespace xiaozhi {
    namespace llm {
        class CozeV3::Impl {
            private:
                std::string conversation_id_;
                std::string bot_id_;
                std::string user_id_;
                boost::json::object header_;
            public:
                Impl(const net::any_io_executor &executor, const YAML::Node& config):
                    bot_id_(config["bot_id"].as<std::string>()),
                    user_id_(config["user_id"].as<std::string>()),
                    header_({
                        {"Authorization", std::format("Bearer {}", config["personal_access_token"].as<std::string>())},
                        {"Content-Type", "application/json"},
                        {"Connection", "keep-alive"}
                    }) {
                }

                net::awaitable<std::string> create_session() {
                    const std::string url = "https://api.coze.cn/v1/conversation/create";
                    auto res = co_await request::post(url, header_, "");
                    auto rej = boost::json::parse(res).as_object();
                    if(!rej.contains("code") || rej.at("code").as_int64() != 0) {
                        BOOST_LOG_TRIVIAL(error) << "CozeV3 create session failed:" << res;
                        co_return "";
                    }
                    conversation_id_ = rej.at("data").at("id").as_string();
                    co_return conversation_id_;
                }

                net::awaitable<void> response(const boost::json::array& dialogue, const std::function<void(std::string_view)>& callback) {
                    std::string query;
                    for(auto it = dialogue.rbegin(); it != dialogue.rend(); --it) {
                        if(it->at("role").as_string() == "user") {
                            query = it->at("content").as_string();
                            break;
                        }
                    }
                    const std::string url = std::format("https://api.coze.cn/v3/chat?conversation_id={}", conversation_id_);
                    boost::json::object data = {
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
                    co_await request::stream_post(url, header_, boost::json::serialize(data), [&callback](const std::string res) {
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
                                auto rej = boost::json::parse(std::string_view(line.data() + 5, line.size() - 5)).as_object();
                                if(rej.contains("role") && rej.contains("type") && rej["role"] == "assistant" && rej["type"] == "answer") {
                                    callback(rej["content"].as_string());
                                }
                            }
                        }
                    });
                }
                
        };

        CozeV3::CozeV3(const net::any_io_executor &executor, const YAML::Node& config) {
            impl_ = std::make_unique<Impl>(executor, config);
        }

        CozeV3::~CozeV3() {
            
        }

        net::awaitable<std::string> CozeV3::create_session() {
            co_return co_await impl_->create_session();
        }

        net::awaitable<void> CozeV3::response(const boost::json::array& dialogue, const std::function<void(std::string_view)>& callback) {
            co_await impl_->response(dialogue, callback);
        }
    }
}