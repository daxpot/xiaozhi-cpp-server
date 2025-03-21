#include <boost/log/trivial.hpp>
#include <cstdint>
#include <sstream>
#include <unordered_set>
#include <xz-cpp-server/common/tools.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <chrono>

// 常见的中文标点符号（UTF-8 编码）
static const std::unordered_set<uint32_t> chineseSegments {
    0xE38081, // 、
    0xEFBC8C, // ，
    0xE38082, // 。
    0xEFBC81, // ！
    0xEFBC9F,  // ？
    0xEFBC9B, // ；
    0xEFBC9A // ：
};

namespace tools {
    SegmentRet is_segment(const std::string& str, std::string::size_type pos) {
        unsigned char byte = static_cast<unsigned char>(str[pos]);

        // 单字节英文标点符号
        if (byte < 0x80) {
            return std::string(",.!?;:").find(byte) == std::string::npos
                ? SegmentRet::NONE
                : SegmentRet::EN;
        }

        // 多字节中文标点符号（UTF-8 编码）
        if (byte >= 0xE0) {
            // 将 3 个字节组合成一个 uint32_t（只用低 24 位）
            uint32_t seq = (static_cast<uint32_t>(static_cast<unsigned char>(str[pos])) << 16) |
                       (static_cast<uint32_t>(static_cast<unsigned char>(str[pos + 1])) << 8) |
                       static_cast<uint32_t>(static_cast<unsigned char>(str[pos + 2]));
            if(chineseSegments.contains(seq)) {
                return SegmentRet::CHINESE;
            }
        }
        return SegmentRet::NONE;
    }
    
    std::string::size_type find_last_segment(const std::string& input) {
        if (input.empty()) {
            return std::string::npos;
        }
    
        // 从后往前遍历字节
        for (std::string::size_type i = input.size() - 1; i != std::string::npos; --i) {
            auto ret = is_segment(input, i);
            if(ret == SegmentRet::EN) {
                return i;
            } else if(ret == SegmentRet::CHINESE) {
                return i+2;
            }
        }
    
        return std::string::npos;
    }

    std::string generate_uuid() {
        auto uuid = boost::uuids::random_generator()();
        return boost::uuids::to_string(uuid);
    }

    std::string gzip_compress(const std::string &data) {
        std::stringstream compressed;
        std::stringstream origin(data);
        boost::iostreams::filtering_streambuf <boost::iostreams::input> out;
        out.push(boost::iostreams::gzip_compressor());
        out.push(origin);
        boost::iostreams::copy(out, compressed);
        return compressed.str();
    }
    
    std::string gzip_decompress(const std::string &data) {
        std::stringstream compressed(data);
        std::stringstream decompressed;
    
        boost::iostreams::filtering_streambuf <boost::iostreams::input> out;
        out.push(boost::iostreams::gzip_decompressor());
        out.push(compressed);
        boost::iostreams::copy(out, decompressed);
    
        return decompressed.str();
    }

    long long get_tms() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        return ms.count();
    }

    std::tuple<OpusEncoder*, OpusDecoder*> create_opus_coders(int sample_rate, bool create_encoder, bool create_decoder) {
        int error;
        OpusEncoder* encoder = nullptr;
        OpusDecoder* decoder = nullptr;
        if(create_encoder) {
            encoder = opus_encoder_create(sample_rate, 1, OPUS_APPLICATION_AUDIO, &error);
            if (error != OPUS_OK) {
                BOOST_LOG_TRIVIAL(error) << "Failed to create opus encoder:" << opus_strerror(error);
                return {nullptr, nullptr};
            }
        }
        if(create_decoder) {
            decoder = opus_decoder_create(sample_rate, 1, &error);
            if (error != OPUS_OK) {
                BOOST_LOG_TRIVIAL(error) << "Failed to create opus decoder:" << opus_strerror(error);
                if(encoder)
                    opus_encoder_destroy(encoder);
                return {nullptr, nullptr};
            }
        }
        return {encoder, decoder};
    }
}
