#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/log/trivial.hpp>
#include <optional>
#include <opus/opus.h>
#include <opus/opus_defines.h>
#include <string>
#include <vector>
#include <xz-cpp-server/asr/paraformer.h>
#include <xz-cpp-server/common/threadsafe_queue.hpp>
#include <funasr/paraformer/model.h>
#include <funasr/paraformer/paraformer-online.h>

namespace xiaozhi {
    namespace asr {
        class ParaformerSingleton {
            public:
                static funasr::Model* get_instance(const YAML::Node& config) {
                    std::call_once(init_flag_, [&config]() {
                        std::map<std::string, std::string> model_path;
                        model_path[MODEL_DIR] = config["model_dir"].as<std::string>(); // 替换为你的模型目录
                        model_path[QUANTIZE] = config["quantize"].as<std::string>();
                        int thread_num = config["thread_num"].as<int>();
                        instance_ = funasr::CreateModel(model_path, thread_num);
                    });
                    return instance_;
                }
                ParaformerSingleton(const ParaformerSingleton&) = delete;
                ParaformerSingleton& operator=(const ParaformerSingleton&) = delete;

            private:
                ParaformerSingleton() = default; // 私有构造函数
                static funasr::Model* instance_;
                static std::once_flag init_flag_;
        };
        
        funasr::Model* ParaformerSingleton::instance_ = nullptr;
        std::once_flag ParaformerSingleton::init_flag_;

        class Paraformer::Impl {
            private:
            
                std::vector<float> pcm_;
                int decoded_length_ = 0;
                std::string full_result_;

                funasr::ParaformerOnline* online_model;
                OpusDecoder* decoder_;

            public:
                Impl(const net::any_io_executor& executor, const YAML::Node& config) {
                    auto offline_model = ParaformerSingleton::get_instance(config);
                    online_model = static_cast<funasr::ParaformerOnline*>(funasr::CreateModel(offline_model, {5, 10, 5}));
                    int error;
                    decoder_ = opus_decoder_create(16000, 1, &error);
                    if (error != OPUS_OK) throw std::runtime_error("Paraformer Opus 解码器初始化失败");
                    pcm_.reserve(960*11);
                }
                

                ~Impl() {
                    BOOST_LOG_TRIVIAL(debug) << "Paraformer asr destroyed";
                }

                net::awaitable<std::string> detect_opus(const std::optional<beast::flat_buffer>& buf) {
                    if(buf) {
                        int decoded_samples = opus_decode_float(decoder_, 
                            static_cast<const unsigned char*>(buf->data().data()), 
                            buf->size(), pcm_.data() + decoded_length_, 960, 0);
                        if(decoded_samples < 0) {
                            BOOST_LOG_TRIVIAL(error) << "Paraformer opus 解码失败:" << opus_strerror(decoded_samples);
                            co_return "";
                        }
                        decoded_length_ += decoded_samples;
                    }
                    if(decoded_length_ >= 9600 || !buf) {
                        std::string result = online_model->Forward(pcm_.data(), decoded_length_, !buf);
                        full_result_ += result;
                        if(!result.empty()) {
                            BOOST_LOG_TRIVIAL(debug) << "Paraformer detect asr:" << result << ",pcm length:" << decoded_length_;
                        }
                        decoded_length_ = 0;
                    }
                    if(!buf) {
                        co_return std::move(full_result_);
                    }
                    co_return "";
                }
        };

        Paraformer::Paraformer(const net::any_io_executor& executor, const YAML::Node& config) {
            impl_ = std::make_unique<Impl>(executor, config);
        }
        
        Paraformer::~Paraformer() {

        }

        net::awaitable<std::string> Paraformer::detect_opus(const std::optional<beast::flat_buffer>& buf) {
            co_return co_await impl_->detect_opus(buf);
        }
    }
}