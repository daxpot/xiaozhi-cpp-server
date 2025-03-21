#pragma once
#include <opus/opus.h>
#include <string>
namespace tools {
    enum SegmentRet {
        NONE = 0,
        EN = 1,
        CHINESE = 3
    };
    SegmentRet is_segment(const std::string& str, std::string::size_type pos);
    std::string::size_type find_last_segment(const std::string& input);
    std::string generate_uuid();
    std::string gzip_compress(const std::string &data);
    std::string gzip_decompress(const std::string &data);
    long long get_tms();
    std::tuple<OpusEncoder*, OpusDecoder*> create_opus_coders(int sample_rate, bool create_encoder=true, bool create_decoder=true);
}