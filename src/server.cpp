#include <iostream>
#include <xz-cpp-server/server.h>

Server::Server(std::shared_ptr<Setting> setting):
    setting(setting),
    ioc(net::io_context{setting->config["threads"].as<int>()}) {
}

net::awaitable<bool> Server::authenticate(websocket::stream<beast::tcp_stream>& ws, http::request<http::string_body>& req) {
    beast::flat_buffer buffer;
    co_await http::async_read(ws.next_layer(), buffer, req, net::use_awaitable);

    if(!setting->config["server"]["auth"]["enabled"].IsDefined() || !setting->config["server"]["auth"]["enabled"].as<bool>()) {
        co_return true;
    }

    auto auth = req.find("Authorization");
    if(auth == req.end()) {
        std::cout << "not valid auth" << std::endl;
        co_return false;
    }
    auto val  = auth->value();
    val = val.substr(7); //"Bearer token"  去除 "Bearer "
    for(auto i=0; i<setting->config["server"]["auth"]["tokens"].size(); ++i) {
        if(val == setting->config["server"]["auth"]["tokens"][i]["token"].as<std::string>()) {
            co_return true;
        }
    }
    co_return false;
}

net::awaitable<void> Server::run_session(websocket::stream<beast::tcp_stream> ws) {
    ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    http::request<http::string_body> req;
    auto ret = co_await authenticate(ws, req);
    if(!ret) {
        co_return;
    }
    co_await ws.async_accept(req, net::use_awaitable);
    while(true) {
        beast::flat_buffer buffer;
        
        auto [ec, _] = co_await ws.async_read(buffer, net::as_tuple(net::use_awaitable));
        if(ec == websocket::error::closed) {
            co_return;
        } else if(ec) {
            throw boost::system::system_error(ec);
        }
        ws.text(ws.got_text());
        co_await ws.async_write(buffer.data(), net::use_awaitable);
    }
}

net::awaitable<void> Server::listen(net::ip::tcp::endpoint endpoint) {
    auto executor = co_await net::this_coro::executor;
    auto acceptor = net::ip::tcp::acceptor{executor, endpoint};
    while(true) {
        net::co_spawn(executor,
            run_session(websocket::stream<beast::tcp_stream>{
                co_await acceptor.async_accept(net::use_awaitable)
            }),
            [](std::exception_ptr e) {
                if(e) {
                    try {
                        std::rethrow_exception(e);
                    } catch(std::exception& e) {
                        std::cerr << "Session error:" << e.what() << std::endl;
                    }
                }
            }
        );
    }
}

void Server::run() {
    auto address = net::ip::make_address(setting->config["server"]["ip"].as<std::string>());
    net::co_spawn(ioc, 
        listen(net::ip::tcp::endpoint{address, setting->config["server"]["port"].as<unsigned short>()}),
        [](std::exception_ptr e) {
            if(e) {
                try {
                    std::rethrow_exception(e);
                } catch(std::exception const& e) {
                    std::cerr << "Listen error:" << e.what() << std::endl;
                }
            }
        }  
    );
    std::vector<std::thread> v;
    auto threads = setting->config["threads"].as<int>();
    v.reserve(threads-1);
    for(int i=0; i<threads-1; ++i) {
        v.emplace_back([&] {ioc.run();});
    }
    ioc.run();
}
