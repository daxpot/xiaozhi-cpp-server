#pragma once
#include <string>
#include <vector>

namespace funasr {
    std::string PathAppend(const std::string &p1, const std::string &p2);
    void FindMax(float *din, int len, float &max_val, int &max_idx);
    std::vector<std::string> split(const std::string &s, char delim);
}