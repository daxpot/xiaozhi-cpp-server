#pragma once
#include "yaml-cpp/yaml.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/log/trivial.hpp>
#include <boost/json.hpp>
#include <boost/beast/ssl.hpp>
#include <regex>
#include <atomic>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace json = boost::json;
namespace http = beast::http;
namespace ssl = net::ssl;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;