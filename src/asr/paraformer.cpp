#include <atomic>
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
                std::atomic<bool> is_released_ = false;
                ThreadSafeQueue<std::optional<beast::flat_buffer>> queue;
                std::function<void(std::string)> on_detect_cb_;
                funasr::ParaformerOnline* online_model;
                OpusDecoder* decoder_;
                net::any_io_executor executor_;

                net::awaitable<void> run() {
                    std::vector<float> pcm(960*11);
                    int decoded_length = 0;
                    std::string full_result;
                    while(!is_released_) {
                        std::optional<beast::flat_buffer> buf;
                        if(!queue.try_pop(buf)) {
                            net::steady_timer timer(executor_, std::chrono::milliseconds(60));
                            co_await timer.async_wait(net::use_awaitable);
                            continue;
                        }
                        if(buf) {
                            int decoded_samples = opus_decode_float(decoder_, 
                                static_cast<const unsigned char*>(buf->data().data()), 
                                buf->size(), pcm.data() + decoded_length, 960, 0);
                            if(decoded_samples < 0) {
                                BOOST_LOG_TRIVIAL(error) << "Paraformer opus 解码失败:" << opus_strerror(decoded_samples);
                                continue;
                            }
                            decoded_length += decoded_samples;
                        }
                        if(decoded_length >= 9600 || !buf) {
                            std::string result = online_model->Forward(pcm.data(), decoded_length, !buf);
                            full_result += result;
                            if(!result.empty()) {
                                BOOST_LOG_TRIVIAL(debug) << "Paraformer detect asr:" << result << ",pcm length:" << decoded_length;
                            }
                            decoded_length = 0;
                        }
                        if(!buf) {
                            on_detect_cb_(std::move(full_result));
                        }
                    }

                }
            public:
                Impl(const net::any_io_executor& executor, const YAML::Node& config):
                    executor_(executor) {
                    auto offline_model = ParaformerSingleton::get_instance(config);
                    online_model = static_cast<funasr::ParaformerOnline*>(funasr::CreateModel(offline_model, {5, 10, 5}));
                    int error;
                    decoder_ = opus_decoder_create(16000, 1, &error);
                    if (error != OPUS_OK) throw std::runtime_error("Paraformer Opus 解码器初始化失败");
                    net::co_spawn(executor, run(), [](std::exception_ptr e) {
                        if(e) {
                            try {
                                std::rethrow_exception(e);
                            } catch(std::exception& e) {
                                BOOST_LOG_TRIVIAL(error) << "Paraformer run error:" << e.what();
                            } catch(...) {
                                BOOST_LOG_TRIVIAL(error) << "Paraformer run unknown error";
                            }
                        }
                    });
                }
                

                ~Impl() {
                    is_released_ = true;
                    delete online_model;
                    opus_decoder_destroy(decoder_);
                }

                void detect_opus(std::optional<beast::flat_buffer> buf) {
                    queue.push(std::move(buf));
                }

                void on_detect(const std::function<void(std::string)>& callback) {
                    on_detect_cb_ = callback;
                }
        };

        Paraformer::Paraformer(const net::any_io_executor& executor, const YAML::Node& config) {
            impl_ = std::make_unique<Impl>(executor, config);
        }
        
        Paraformer::~Paraformer() {

        }

        void Paraformer::detect_opus(std::optional<beast::flat_buffer> buf) {
            return impl_->detect_opus(std::move(buf));
        }

        void Paraformer::on_detect(const std::function<void(std::string)>& callback) {
            return impl_->on_detect(callback);
        }
    }
}