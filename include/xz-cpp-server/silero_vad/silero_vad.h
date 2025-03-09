#pragma once
#include <memory>
#include "onnxruntime_cxx_api.h"
#include <vector>

namespace xiaozhi {
    class VoiceDetector {
        private:
            std::shared_ptr<Ort::Session> session = nullptr;
            Ort::MemoryInfo memory_info;
            const int window_size = 512; // Silero-VAD 窗口大小
            std::vector<float> state;    // Silero-VAD 隐藏状态
            int64_t sample_rate = 16000;
            float threshold = 0.5;
        public:
        VoiceDetector();
        ~VoiceDetector();
        float predict(std::vector<float> &pcm_data);
    };
    class SileroVad {
        public:
            static std::shared_ptr<SileroVad> getVad();
            // ~Vad();
            float predict(std::vector<float> &pcm_data);
            
        private:
            VoiceDetector detector;
            SileroVad();
    };
}