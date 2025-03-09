#pragma once
#include <opus/opus.h>
#include <boost/beast.hpp>
#include <vector>

namespace beast = boost::beast;

namespace xiaozhi {
    class Vad {
        private:
            // std::vector<float> pcm_buffer;
            OpusDecoder* decoder;
        public:
            Vad();
            ~Vad();
            std::optional<std::vector<float>> check_vad(beast::flat_buffer &buffer);
    };
}