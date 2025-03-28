#pragma once
#include <opus/opus.h>
#include <xz-cpp-server/common/setting.h>

namespace xiaozhi {
    class Vad {
        private:
            OpusDecoder* decoder_;
            float threshold_;
        public:
            Vad(std::shared_ptr<Setting> setting);
            ~Vad();
            bool is_vad(beast::flat_buffer &buffer);
    };
}