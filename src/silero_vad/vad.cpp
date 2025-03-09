#include "xz-cpp-server/silero_vad/silero_vad.h"
#include <boost/log/trivial.hpp>
#include <optional>
#include <xz-cpp-server/silero_vad/vad.h>

namespace xiaozhi {
    Vad::Vad(std::shared_ptr<Setting> setting) {
        int error;
        decoder = opus_decoder_create(16000, 1, &error);
        if (error != OPUS_OK) throw std::runtime_error("Opus 解码器初始化失败");
        min_silence_duration_ms = setting->config["VAD"]["SileroVAD"]["min_silence_duration_ms"].as<unsigned int>();
        threshold = setting->config["VAD"]["SileroVAD"]["threshold"].as<float>();
    }

    Vad::~Vad() {
        opus_decoder_destroy(decoder);
    }

    long long Vad::get_tms() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        return ms.count();
    }

    std::optional<std::vector<float>> Vad::check_vad(beast::flat_buffer &buffer) {
        std::vector<float> pcm(960); // 最大帧大小
        int decoded_samples = opus_decode_float(decoder, 
            static_cast<const unsigned char*>(buffer.data().data()), 
            buffer.size(), pcm.data(), 960, 0);
        if (decoded_samples > 0) {
            // todo:考虑到buffer实际为960样本的数据，所以暂时不缓存到pcm_buffer中累积
            if(pcm.size() > 512) {
                auto vad = SileroVad::getVad();
                auto ret = vad->predict(pcm);
                if(ret > threshold) {
                    return std::move(pcm);
                }
            } else { 
                BOOST_LOG_TRIVIAL(error) <<  "音频样本不足512，无法检测vad: " << pcm.size();
            }
        } else {
            BOOST_LOG_TRIVIAL(error) << "opus解码失败: " << opus_strerror(decoded_samples);
        }
        return std::nullopt;
    }
    std::optional<std::vector<float>> Vad::merge_voice(beast::flat_buffer &buffer) {
        auto pcm = check_vad(buffer);
        auto now = get_tms();
        if(pcm == std::nullopt) {
            is_last_voice = false;
            if(now - last_voice_tms > min_silence_duration_ms && pcm_buffer.size() > 0) {
                BOOST_LOG_TRIVIAL(debug) << "获取整段声音:" << pcm_buffer.size() << " " << now << " " << last_voice_tms << " " << now-last_voice_tms;
                return std::move(pcm_buffer);
            }
        } else {
            last_voice_tms = now;
            is_last_voice = true;
            pcm_buffer.insert(pcm_buffer.end(), pcm->begin(), pcm->begin() + pcm->size());
            BOOST_LOG_TRIVIAL(debug) << "获取声音片段" << now << ":" << last_voice_tms;
        }
        return std::nullopt;
    }
}