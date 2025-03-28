#pragma once

// 自定义格式化函数，根据日志级别设置颜色
void color_formatter(boost::log::record_view const& rec, boost::log::formatting_ostream& strm);

void init_logging(std::string log_level);