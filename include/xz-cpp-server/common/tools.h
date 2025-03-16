#pragma once
#include <string>
namespace tools {
    std::string generate_uuid();
    std::string gzip_compress(const std::string &data);
    std::string gzip_decompress(const std::string &data);
    long long get_tms();
}