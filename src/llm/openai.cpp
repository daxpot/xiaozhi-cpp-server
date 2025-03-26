#include <boost/json/object.hpp>
#include <exception>
#include <xz-cpp-server/llm/openai.h>
#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <format>
#include <memory>
#include <xz-cpp-server/llm/openai.h>
#include <xz-cpp-server/config/setting.h>
#include <boost/beast.hpp>
#include <xz-cpp-server/common/request.h>
#include <boost/json.hpp>


namespace xiaozhi {
    namespace llm {
        class Openai::Impl {
            private:
                std::string conversation_id_;
                const std::string url_;
                const std::string api_key_;
                const std::string model_name_;
                boost::json::object header_;
            public:
                Impl(const net::any_io_executor &executor, const YAML::Node& config):
                    url_(config["url"].as<std::string>()),
                    api_key_(config["api_key"].as<std::string>()),
                    model_name_(config["model_name"].as<std::string>()),
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
                    const std::string url = std::format("{}/chat/completions", url_);
                    boost::json::object data = {
                        {"model", model_name_},
                        {"messages", dialogue},
                        {"stream", true}
                    };
                    bool is_active = true;
                    std::string last_line;  //处理json分段的情况
                    co_await request::stream_post(url, header_, boost::json::serialize(data), [&callback, this, &is_active, &last_line](std::string res) {
                        std::vector<std::string> result;
                        boost::split(result, res, boost::is_any_of("\n"));
                        for(auto& line : result) {
                            if(line.size() == 0)
                                continue;
                            if(!line.starts_with("data:")) {
                                line = last_line + line;
                                last_line = "";
                                BOOST_LOG_TRIVIAL(info) << "llm response segment connected:" << line;
                            }
                            if(line.starts_with("data:") && line != "data: [DONE]") {
                                boost::json::object rej;
                                try {
                                    rej = boost::json::parse(std::string_view(line.data() + 5, line.size() - 5)).as_object();
                                } catch(std::exception e) {
                                    BOOST_LOG_TRIVIAL(error) << "llm can't parse response:" << line;
                                    last_line = line;
                                    continue;
                                }
                                if(rej.contains("choices")) {
                                    for(auto& value : rej["choices"].as_array()) {
                                        auto& item = value.as_object();
                                        if(item.contains("delta") && item["delta"].as_object().contains("content")) {
                                            auto content = std::string_view(item["delta"].as_object()["content"].as_string());
                                            if(content.find("<think>") != content.npos) {
                                                is_active = false;
                                            }
                                            auto p = content.find("</think>");
                                            if(p != content.npos) {
                                                is_active = true;
                                                content = content.substr(p + 8);
                                            }
                                            if(is_active) {
                                                callback(content);
                                            }
                                        }
                                    }
                                }
                            } else if(line != "data: [DONE]") {
                                BOOST_LOG_TRIVIAL(info) << "llm response exception:" << line;
                            }
                        }
                    });
                }
                
        };

        Openai::Openai(const net::any_io_executor &executor, const YAML::Node& config) {
            impl_ = std::make_unique<Impl>(executor, config);
        }

        Openai::~Openai() {
            
        }

        net::awaitable<std::string> Openai::create_session() {
            co_return co_await impl_->create_session();
        }

        net::awaitable<void> Openai::response(const boost::json::array& dialogue, const std::function<void(std::string_view)>& callback) {
            co_await impl_->response(dialogue, callback);
        }
    }
}