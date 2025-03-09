#include "xz-cpp-server/silero_vad/silero_vad.h"
#include <iostream>
#include <optional>
#include <opus/opus.h>
#include <xz-cpp-server/silero_vad/vad.h>

namespace xiaozhi {
    Vad::Vad() {
        int error;
        decoder = opus_decoder_create(16000, 1, &error);
        if (error != OPUS_OK) throw std::runtime_error("Opus 解码器初始化失败");
    }

    Vad::~Vad() {
        opus_decoder_destroy(decoder);
    }

    std::optional<std::vector<float>> Vad::check_vad(beast::flat_buffer &buffer) {
        std::vector<float> pcm(960); // 最大帧大小
        int decoded_samples = opus_decode_float(decoder, 
            static_cast<const unsigned char*>(buffer.data().data()), 
            buffer.size(), pcm.data(), 960, 0);
        if (decoded_samples > 0) {
            // todo:考虑到buffer实际为960样本的数据，所以暂时不缓存到pcm_buffer中累积
            // pcm_buffer.insert(pcm_buffer.end(), pcm.begin(), pcm.begin() + decoded_samples);
            if(pcm.size() > 512) {
                auto vad = SileroVad::getVad();
                auto ret = vad->predict(pcm);
                // std::cout << "predict:" << buffer.size() << " - " << ret << std::endl; 
                return std::move(pcm);
            } else {
                std::cerr << "音频样本不足512，无法检测vad: " << pcm.size() << std::endl;    
            }
        } else {
            std::cerr << "opus解码失败: " << opus_strerror(decoded_samples) << std::endl;
        }
        return std::nullopt;
    }
}