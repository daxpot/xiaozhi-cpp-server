#include <xz-cpp-server/config/logger.h>

// 自定义格式化函数，根据日志级别设置颜色
void color_formatter(boost::log::record_view const& rec, boost::log::formatting_ostream& strm) {
    auto severity = rec[boost::log::trivial::severity];
    if (severity) {
        // 根据级别设置颜色
        if (severity.get() == boost::log::trivial::info) {
            strm << "\033[32m";  // 绿色
        } else if (severity.get() == boost::log::trivial::error) {
            strm << "\033[31m";  // 红色
        } else {
            strm << "\033[0m";   // 默认颜色（无色）
        }
        // 提取 TimeStamp 的值并格式化
        auto timestamp = rec["TimeStamp"];
        if (timestamp) {
            boost::posix_time::ptime time = timestamp.extract<boost::posix_time::ptime>().get();
            strm << "[" << boost::posix_time::to_simple_string(time) << "] ";
        }
        // 输出格式
        strm << "[" << severity << "] "
             << "\033[0m"
             << rec[boost::log::expressions::smessage];
    }
}

void init_logging(std::string log_level) {
    // 设置日志输出到终端
    auto console_sink = boost::log::add_console_log(std::cout);
    // 设置自定义格式化函数
    console_sink->set_formatter(&color_formatter);

    // 设置日志输出到文件
    boost::log::add_file_log(
        boost::log::keywords::file_name = "tmp/server_%N.log",  // 文件名模式，%N 为文件序号
        boost::log::keywords::rotation_size = 10 * 1024 * 1024, // 10MB 轮换
        boost::log::keywords::format = "[%TimeStamp%] [%Severity%]: %Message%" // 日志格式
    );

    // 添加常用属性，如时间戳
    boost::log::add_common_attributes();
    auto level = boost::log::trivial::info;
    if(log_level == "DEBUG") {
        level = boost::log::trivial::debug;
    } else if(log_level == "ERROR") {
        level = boost::log::trivial::error;
    } else if(log_level == "WARNING") {
        level = boost::log::trivial::warning;
    }
    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= level
    );
}