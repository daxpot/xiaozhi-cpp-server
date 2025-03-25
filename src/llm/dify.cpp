#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <format>
#include <memory>
#include <xz-cpp-server/llm/dify.h>
#include <xz-cpp-server/config/setting.h>
#include <boost/beast.hpp>
#include <xz-cpp-server/common/request.h>
#include <boost/json.hpp>


namespace xiaozhi {
    namespace llm {
        class Dify::Impl {
            private:
                std::string conversation_id_;
                const std::string base_url_;
                const std::string api_key_;
                boost::json::object header_;
            public:
                Impl(const net::any_io_executor &executor, const YAML::Node& config):
                    base_url_(config["base_url"].as<std::string>()),
                    api_key_(config["api_key"].as<std::string>()),
                    header_({
                        {"Authorization", std::format("Bearer {}", api_key_)},
                        {"Content-Type", "application/json"},
                        {"Connection", "keep-alive"}
                    }) {
                }

                net::awaitable<std::string> create_session() {
                    co_return "";
                }

                net::awaitable<void> response(const boost::json::array& dialogue, const std::function<void(std::string_view)>& callback) {
                    std::string query;
                    for(auto it = dialogue.rbegin(); it != dialogue.rend(); --it) {
                        if(it->at("role").as_string() == "user") {
                            query = it->at("content").as_string();
                            break;
                        }
                    }
                    const std::string url = std::format("{}/chat-messages", base_url_);
                    boost::json::object data = {
                        {"query", std::move(query)},
                        {"response_mode", "streaming"},
                        {"user", "143523"},
                        {"inputs", boost::json::object({})}
                    };
                    if(!conversation_id_.empty()) {
                        data["conversation_id"] = conversation_id_;
                    }
                    co_await request::stream_post(url, header_, boost::json::serialize(data), [&callback, this](std::string res) {
                        std::vector<std::string> result;
                        boost::split(result, res, boost::is_any_of("\n"));
                        for(auto& line : result) {
                            if(line.size() == 0)
                                continue;
                            if(line.starts_with("data:")) {
                                auto rej = boost::json::parse(std::string_view(line.data() + 5, line.size() - 5)).as_object();
                                if(conversation_id_.empty() && rej.contains("conversation_id")) {
                                    conversation_id_ = rej["conversation_id"].as_string();
                                }
                                if(rej.at("event") == "error") {
                                    BOOST_LOG_TRIVIAL(error) << "Dify api error:" << line;
                                } else if(rej.contains("answer")) {
                                    callback(rej.at("answer").as_string());
                                }
                            }
                        }
                    });
                }
                
        };

        Dify::Dify(const net::any_io_executor &executor, const YAML::Node& config) {
            impl_ = std::make_unique<Impl>(executor, config);
        }

        Dify::~Dify() {
            
        }

        net::awaitable<std::string> Dify::create_session() {
            co_return co_await impl_->create_session();
        }

        net::awaitable<void> Dify::response(const boost::json::array& dialogue, const std::function<void(std::string_view)>& callback) {
            co_await impl_->response(dialogue, callback);
        }
    }
}