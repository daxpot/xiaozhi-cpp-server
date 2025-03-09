#pragma once
#include <opus/opus.h>
#include <boost/beast.hpp>
#include <vector>
#include <xz-cpp-server/config/setting.h>

namespace beast = boost::beast;

namespace xiaozhi {
    class Vad {
        private:
            std::vector<float> pcm_buffer;
            OpusDecoder* decoder;
            std::optional<std::vector<float>> check_vad(beast::flat_buffer &buffer);
            long long get_tms();
            long long last_voice_tms = 0;
            bool is_last_voice = false;
            unsigned int min_silence_duration_ms;
            float threshold;
        public:
            Vad(std::shared_ptr<Setting> setting);
            ~Vad();
            std::optional<std::vector<float>> merge_voice(beast::flat_buffer &buffer);
    };
}