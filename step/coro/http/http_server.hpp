//
// Created by yh on 3/11/25.
//


#ifndef CORO_HTTP_SERVER_HPP
#define CORO_HTTP_SERVER_HPP

#include "../net/server.hpp"
#include "../net/client.hpp"
#include "../task.hpp"
#include "../net/io_scheduler.hpp"

#include <unordered_map>
#include <functional>
#include <string>
#include <vector>
#include <sstream>


class Request {
public:
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    size_t content_length = 0;

    static Request parse(const std::string& raw_request);
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
    using Handler = std::function<Task<>(Request&, Response&)>;

    HttpServer(std::shared_ptr<IoScheduler> scheduler, Server::Options opts = {})
        : server_(scheduler, opts) {}

    // 注册路由处理函数
    void Get(const std::string& path, Handler handler) {
        routes_["GET"][path] = handler;
    }

    void Post(const std::string& path, Handler handler) {
        routes_["POST"][path] = handler;
    }

    Task<> start() {
        co_await server_.poll();
        while (true) {
            auto pstatus = co_await server_.poll();
            if (pstatus == PollStatus::Event) {
                Client client = server_.accept();
                if (client.socket().is_valid()) {
                    server_.scheduler()->spawn(handle_client(std::move(client)));
                }
            }
        }
    }

private:
    Server server_;
    std::unordered_map<std::string, std::unordered_map<std::string, Handler>> routes_;
    std::shared_ptr<IoScheduler> scheduler_;

    Task<> handle_client(Client client) {
        std::vector<char> buffer(4096);
        while (true) {
            co_await client.poll(PollOp::Read);
            auto [rstatus, data] = client.recv(buffer);
            if (rstatus != RecvStatus::Ok) break;

            std::string raw_request(data.data(), data.size());
            Request req = Request::parse(raw_request);
            Response resp;

            auto method_handlers = routes_.find(req.method);
            if (method_handlers != routes_.end()) {
                auto handler = method_handlers->second.find(req.path);
                if (handler != method_handlers->second.end()) {
                    co_await handler->second(req, resp);
                } else {
                    resp.status_code = 404;
                    resp.body = "Not Found";
                }
            } else {
                resp.status_code = 405;
                resp.body = "Method Not Allowed";
            }

            std::string response_str = resp.to_string();
            co_await client.poll(PollOp::Write);
            client.send(std::span<const char>{response_str.data(), response_str.size()});
        }
    }
};

// Request解析实现
Request Request::parse(const std::string& raw_request) {
    Request req;
    std::istringstream iss(raw_request);
    std::string line;


    // 解析请求行
    std::getline(iss, line);
    std::istringstream line_stream(line);
    line_stream >> req.method >> req.path;

    // 解析头部
    while (std::getline(iss, line) && line != "\r") {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 2, line.size() - colon - 3); // 去除\r和空格
            req.headers[key] = value;

            if (key == "Content-Length") {
                req.content_length = std::stoul(value);
            }
        }
    }

    // 解析正文（简化版）
    size_t header_end = raw_request.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        req.body = raw_request.substr(header_end + 4, req.content_length);
    }

    return req;
}

// Response生成实现
std::string Response::to_string() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " OK\r\n";
    for (const auto& [key, value] : headers) {
        oss << key << ": " << value << "\r\n";
    }
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "\r\n" << body;
    return oss.str();
}


#endif // CORO_HTTP_SERVER_HPP

