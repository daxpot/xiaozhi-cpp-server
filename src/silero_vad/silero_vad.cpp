#include <xz-cpp-server/silero_vad/silero_vad.h>
#include <xz-cpp-server/common/setting.h>

namespace xiaozhi {

    static std::shared_ptr<SileroVad> vad = nullptr;
    static std::once_flag vadFlag;
    
    std::shared_ptr<SileroVad> SileroVad::getVad() {
        std::call_once(vadFlag, [&] {
            vad = std::shared_ptr<SileroVad>(new SileroVad());
        });
        return vad;
    }
    
    SileroVad::SileroVad() {
    }
    
    float SileroVad::predict(std::vector<float> &pcm_data) {
        return detector.predict(pcm_data);
    }
    
    VoiceDetector::VoiceDetector()
                : memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
        const auto setting = Setting::getSetting();
    
        // 初始化 ONNX Runtime 和 Silero-VAD 模型
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "SileroVAD");
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetInterOpNumThreads(1);
        session_ = std::make_shared<Ort::Session>(env, setting->config["VAD"]["SileroVAD"]["model_path"].as<std::string>().c_str(), session_options);
    
        // 初始化隐藏状态
        state_.resize(2 * 128, 0.0f);
    }
        
    VoiceDetector::~VoiceDetector() {
    }
        
    //todo: 考虑速度，暂时只检测前512个样本，后续如果有问题再改为滑动窗口检测后面的样本
    float VoiceDetector::predict(std::vector<float> &pcm_data) {
        // 准备 Silero-VAD 输入张量
        std::vector<int64_t> input_shape = {1, window_size_};
        std::vector<int64_t> sr_shape = {1};
        std::vector<int64_t> state_shape = {2, 1, 128};
    
        auto input_tensor = Ort::Value::CreateTensor<float>(memory_info_, pcm_data.data(), window_size_, 
                                                            input_shape.data(), input_shape.size());
        auto sr_tensor = Ort::Value::CreateTensor<int64_t>(memory_info_, &sample_rate_, 1, 
                                                            sr_shape.data(), sr_shape.size());
        auto state_tensor = Ort::Value::CreateTensor<float>(memory_info_, state_.data(), state_.size(), 
                                                            state_shape.data(), state_shape.size());
    
        std::vector<Ort::Value> input_tensors;
        input_tensors.push_back(std::move(input_tensor));
        input_tensors.push_back(std::move(sr_tensor));
        input_tensors.push_back(std::move(state_tensor));
    
        // 运行 Silero-VAD
        const char* input_names[] = {"input", "sr", "state"};
        const char* output_names[] = {"output", "stateN"};
        auto output_tensors = session_->Run(Ort::RunOptions{nullptr}, input_names, input_tensors.data(), 3, 
                                            output_names, 2);
    
        // 获取语音概率
        float probability = output_tensors[0].GetTensorMutableData<float>()[0];
    
        // 更新隐藏状态
        float* stateN = output_tensors[1].GetTensorMutableData<float>();
        std::memcpy(state_.data(), stateN, state_.size() * sizeof(float));
    
        // 判断是否有人声
        return probability;
    }
}
