#pragma once
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

// 自定义格式化函数，根据日志级别设置颜色
void color_formatter(boost::log::record_view const& rec, boost::log::formatting_ostream& strm);

void init_logging(std::string log_level);