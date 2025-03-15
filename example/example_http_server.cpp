#include <coro/coro.hpp>
#include <unordered_map>

#include "json.hpp"
using nlohmann::json;

#include "json.hpp"
using nlohmann::json;

using namespace coro;

namespace http = coro::net::tcp::http;

enum ErrorCodes {
    SUCESS = 0,
    ERR_NETWORK = -1,
    ERR_UNKNOWN = 1000,
    ERR_PWD,
    ERR_VarifyExpired,
    ERR_JSON_PARSE,
};

// 用户数据库模拟
std::unordered_map<std::string, std::string> user_db = {
        {"admin", "123456"},
        {"John", "123456"},
        {"user", "password"}
};



int main() {
    auto scheduler = IoScheduler::make_shared(IoScheduler::Options{.execution_strategy = io_exec_thread_pool});
    http::HttpServer server(scheduler, {.address = net::IpAddress::from_string("0.0.0.0"), .port = 8080});

    // 注册路由
    server.Get("/", [](http::Request& req, http::Response& resp) -> Task<> {
        resp.body = "Hello from coro HTTP server!";
        co_return;
    });

    server.Get("/api", [](http::Request& req, http::Response& resp) -> Task<> {
        resp.headers["Content-Type"] = "application/json";
        resp.body = R"({"status": "ok"})";
        co_return;
    });

    server.Post("/get_varifycode", [](http::Request& req, http::Response& resp) -> Task<> {
        try {
            json j = json::parse(req.body);
            std::string email = j["email"];
            std::cerr << "email: " << email << "\n";

            json response_json;
            response_json["error"] = ErrorCodes::SUCESS;

            resp.headers["Content-Type"] = "application/json";
            resp.body = response_json.dump();

        } catch (const json::exception& e) {
            resp.status_code = 400;
            resp.body = R"({"code":400, "message": "无效的json格式"})";
        }

        co_return ;
    });

    server.Post("/test", [](http::Request& req, http::Response& resp) -> Task<> {
        try {
            json j = json::parse(req.body);
            std::string name = j["name"];
            int age = j["age"];
            std::cerr << "name: " << name << "\n";
            std::cerr << "age: " << age << "\n";

            json response_json;
            response_json["name"] = name;
            response_json["age"] = age;
            response_json["error"] = ErrorCodes::SUCESS;

            resp.headers["Content-Type"] = "application/json";
            resp.body = response_json.dump();

        } catch (const json::exception& e) {
            resp.status_code = 400;
            resp.body = R"({"code":400, "message": "无效的json格式"})";
        }

        co_return ;
    });

    server.Post("/login", [](http::Request& req, http::Response& resp) -> Task<> {
        try {
            json j = json::parse(req.body);
            std::string username = j["username"];
            std::string password = j["password"];
            std::cerr << "username: " << username << "\n";
            std::cerr << "password: " << password << "\n";

            json response_json;
            if (user_db.count(username) && user_db[username] == password) {
                response_json["name"] = username;
                response_json["password"] = password;
                response_json["error"] = ErrorCodes::SUCESS;
            } else {
                std::cerr << "密码不对\n";
                response_json["error"] = ErrorCodes::ERR_PWD;
            }
            resp.headers["Content-Type"] = "application/json";
            resp.body = response_json.dump();

        } catch (const json::exception& e) {
            resp.status_code = 400;
            resp.body = R"({"code":400, "message": "无效的json格式"})";
        }

        co_return ;
    });

    sync_wait(server.start());
    return 0;
}