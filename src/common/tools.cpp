#include <sstream>
#include <xz-cpp-server/common/tools.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>

namespace tools {
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
}
