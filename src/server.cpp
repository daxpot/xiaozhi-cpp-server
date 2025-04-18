#include <xz-cpp-server/common/tools.h>
#include <xz-cpp-server/server.h>
#include <xz-cpp-server/connection.h>

namespace xiaozhi {
    Server::Server(std::shared_ptr<Setting> setting):
        setting(setting),
        ioc(net::io_context{setting->config["threads"].as<int>()}) {
    }

    net::awaitable<bool> Server::authenticate(websocket::stream<beast::tcp_stream> &ws, http::request<http::string_body> &req) {
        beast::flat_buffer buffer;
        co_await http::async_read(ws.next_layer(), buffer, req, net::use_awaitable);

        if(!setting->config["server"]["auth"]["enabled"].IsDefined() || !setting->config["server"]["auth"]["enabled"].as<bool>()) {
            co_return true;
        }

        auto auth = req.find("Authorization");
        if(auth == req.end()) {
            BOOST_LOG_TRIVIAL(info) << "not valid auth";
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
        auto executor = co_await net::this_coro::executor;
        auto conn = std::make_shared<Connection>(setting, std::move(ws), executor);
        conn->start();
    }

    net::awaitable<void> Server::listen(net::ip::tcp::endpoint endpoint) {
        auto executor = co_await net::this_coro::executor;
        auto acceptor = net::ip::tcp::acceptor{executor, endpoint};
        BOOST_LOG_TRIVIAL(info) << "Server is running at " << endpoint.address().to_string() << ":" << endpoint.port();
        while(true) {
            net::co_spawn(executor,
                run_session(websocket::stream<beast::tcp_stream>{
                    co_await acceptor.async_accept(net::use_awaitable)
                }),
                std::bind_front(tools::on_spawn_complete, "Session"));
        }
    }

    void Server::run() {
        auto address = net::ip::make_address(setting->config["server"]["ip"].as<std::string>());
        net::co_spawn(ioc, 
            listen(net::ip::tcp::endpoint{address, setting->config["server"]["port"].as<unsigned short>()}),
            std::bind_front(tools::on_spawn_complete, "Listen"));
        std::vector<std::thread> v;
        auto threads = setting->config["threads"].as<int>();
        v.reserve(threads-1);
        for(int i=0; i<threads-1; ++i) {
            v.emplace_back([&] {ioc.run();});
        }
        ioc.run();
    }
}