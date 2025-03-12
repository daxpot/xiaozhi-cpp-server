#pragma once
#include <opus/opus.h>
#include <boost/beast.hpp>
#include <vector>
#include <xz-cpp-server/config/setting.h>

namespace beast = boost::beast;

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