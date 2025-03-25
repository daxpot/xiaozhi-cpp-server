#include <cmath>
#include <sstream>
#include <funasr/paraformer/utils.h>

namespace funasr {
    std::string PathAppend(const std::string &p1, const std::string &p2) {
        char sep = '/';
        std::string tmp = p1;
        if (p1[p1.length()-1] != sep) { // Need to add a
            tmp += sep;               // path separator
            return (tmp + p2);
        } else
            return (p1 + p2);
    }

    void FindMax(float *din, int len, float &max_val, int &max_idx) {
        int i;
        max_val = -INFINITY;
        max_idx = -1;
        for (i = 0; i < len; i++) {
            if (din[i] > max_val) {
                max_val = din[i];
                max_idx = i;
            }
        }
    }

    std::vector<std::string> split(const std::string &s, char delim) {
        std::vector<std::string> elems;
        std::stringstream ss(s);
        std::string item;
        while(std::getline(ss, item, delim)) {
          elems.push_back(item);
        }
        return elems;
      }
}