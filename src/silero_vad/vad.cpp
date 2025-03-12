#include <boost/beast/core/flat_buffer.hpp>
#include <xz-cpp-server/silero_vad/silero_vad.h>
#include <boost/log/trivial.hpp>
#include <xz-cpp-server/silero_vad/vad.h>
#include <xz-cpp-server/common/tools.h>

namespace xiaozhi {
    Vad::Vad(std::shared_ptr<Setting> setting) {
        int error;
        decoder_ = opus_decoder_create(16000, 1, &error);
        if (error != OPUS_OK) throw std::runtime_error("Opus 解码器初始化失败");
        threshold_ = setting->config["VAD"]["SileroVAD"]["threshold"].as<float>();
    }

    Vad::~Vad() {
        opus_decoder_destroy(decoder_);
    }

    bool Vad::is_vad(beast::flat_buffer &buffer) {
        std::vector<float> pcm(960); // 最大帧大小
        int decoded_samples = opus_decode_float(decoder_, 
            static_cast<const unsigned char*>(buffer.data().data()), 
            buffer.size(), pcm.data(), 960, 0);
        if (decoded_samples > 0) {
            // todo:考虑到buffer实际为960样本的数据，所以暂时不缓存到pcm_buffer中累积
            if(pcm.size() > 512) {
                auto vad = SileroVad::getVad();
                auto ret = vad->predict(pcm);
                if(ret > threshold_) {
                    return true;
                }
            } else { 
                BOOST_LOG_TRIVIAL(error) <<  "音频样本不足512，无法检测vad: " << pcm.size();
            }
        } else {
            BOOST_LOG_TRIVIAL(error) << "opus解码失败: " << opus_strerror(decoded_samples);
        }
        return false;
    }
}