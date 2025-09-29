
## 功能特性
**已实现功能**

✅ 协程基础框架

✅ 线程池支持

✅ I/O 调度器

✅ 同步等待原语

✅ TCP 客户端/服务器

✅ HTTP 服务器

## Requirements
```
C++20 Compiler with coroutine support
    g++ [10.2.1, 10.3.1, 11, 12, 13]
    clang++ [16, 17]
    MSVC Windows 2022 CL
        Does not currently support:
            `NETWORKING`

CMake
make or ninja
python（用于构建脚本）
```


## 构建

### 方法一：使用构建脚本（推荐）

```bash
# 构建所有目标（库、测试、示例）
python3 scripts/build.py --test --example

# 并行构建（加速构建过程）
python3 scripts/build.py --test --example -j 8
```

### 方法二：使用 CMake 直接构建

```bash
cmake -B build
cmake --build build --parallel 8
```

**CMake 选项**

选项名称 |	默认值 |	描述
--- | --- | ---
BUILD_TESTS	 | OFF	| 是否构建测试用例
BUILD_EXAMPLES	| OFF | 是否构建示例程序


**网络功能说明**

网络功能在以下平台自动启用：
Linux 系统

网络功能在以下平台自动禁用：
Windows 系统


## 测试
构建完成后可以运行测试：

```bash
# 使用构建脚本运行测试
python3 scripts/build.py --test

# 或直接使用 ctest
cd build && ctest -V
```


## 性能测试

### 测试环境
- WSL2 环境: Arch Linux

- CPU: AMD Ryzen 7 7745HX @ 3.60GHz 

- 内存: 15GB (WSL2分配) + 4GB Swap



[example/http_200_ok_server.cpp](https://github.com/Jiangnanxiaosheng/coro/blob/main/example/http_200_ok_server.cpp)

```
$ wrk -t16 -c1000 -d20s http://127.0.0.1:8080/
Running 20s test @ http://127.0.0.1:8080/
  16 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.62ms    5.02ms  61.94ms   84.15%
    Req/Sec    11.70k     2.18k   22.58k    67.37%
  3732926 requests in 20.08s, 206.48MB read
  Socket errors: connect 0, read 0, write 0, timeout 72
Requests/sec: 185859.95
Transfer/sec:     10.28MB
```

[example/tcp_echo_server.cpp](https://github.com/Jiangnanxiaosheng/coro/blob/main/example/tcp_echo_server.cpp)
```
$ ab -n 10000000 -c 1000 -k http://127.0.0.1:8080/
This is ApacheBench, Version 2.3 <$Revision: 1923142 $>
Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/
Licensed to The Apache Software Foundation, http://www.apache.org/

Benchmarking 127.0.0.1 (be patient)
Completed 1000000 requests
Completed 2000000 requests
Completed 3000000 requests
Completed 4000000 requests
Completed 5000000 requests
Completed 6000000 requests
Completed 7000000 requests
Completed 8000000 requests
Completed 9000000 requests
Completed 10000000 requests
Finished 10000000 requests


Server Software:
Server Hostname:        127.0.0.1
Server Port:            8080

Document Path:          /
Document Length:        0 bytes

Concurrency Level:      1000
Time taken for tests:   121.297 seconds
Complete requests:      10000000
Failed requests:        0
Non-2xx responses:      10000000
Keep-Alive requests:    10000000
Total transferred:      1060000000 bytes
HTML transferred:       0 bytes
Requests per second:    82442.11 [#/sec] (mean)
Time per request:       12.130 [ms] (mean)
Time per request:       0.012 [ms] (mean, across all concurrent requests)
Transfer rate:          8534.05 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.3      0      44
Processing:     1   12   3.7     11      91
Waiting:        1   12   3.7     11      91
Total:          1   12   3.7     11     122

Percentage of the requests served within a certain time (ms)
  50%     11
  66%     12
  75%     12
  80%     13
  90%     16
  95%     21
  98%     24
  99%     26
 100%    122 (longest request)
```
