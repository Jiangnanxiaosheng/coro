#ifndef CORO_HTTP_SERVER_HPP
#define CORO_HTTP_SERVER_HPP

#include <unordered_map>
#include <functional>
#include <string>
#include <vector>
#include <sstream>

#include "coro/net/tcp/server.hpp"
#include "coro/task.hpp"


namespace coro::net::tcp::http {

    class Request {
    public:
        std::string method;
        std::string path;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
        size_t content_length = 0;

        static Request parse(const std::string &raw_request);
    };

    class Response {
    public:
        int status_code = 200;
        std::unordered_map<std::string, std::string> headers;
        std::string body;

        std::string to_string() const;
    };

    class HttpServer {
    public:
        using Handler = std::function<Task<>(Request & , Response & )>;

        HttpServer(std::shared_ptr<IoScheduler> scheduler, Server::LocalEndPoint opts = {});

        // 注册路由处理函数
        void Get(const std::string &path, Handler handler);

        void Post(const std::string &path, Handler handler);

        Task<> start();

    private:
        std::shared_ptr<IoScheduler> m_scheduler;
        Server m_server;
        std::unordered_map<std::string, std::unordered_map<std::string, Handler>> m_routes;


        Task<> handle_client(Client client);

    };

} // namespace coro::net::tcp::http


#endif //CORO_HTTP_SERVER_HPP
