#ifndef CORO_CORO_HPP
#define CORO_CORO_HPP

#include "coro/concepts/awaitable.hpp"
#include "coro/concepts/buffer.hpp"

#ifdef NETWORKING

#include "coro/net/connect_status.hpp"
#include "coro/net/ip_address.hpp"
#include "coro/net/recv_status.hpp"
#include "coro/net/send_status.hpp"
#include "coro/net/socket.hpp"
#include "coro/net/tcp/client.hpp"
#include "coro/net/tcp/http/http_server.hpp"
#include "coro/net/tcp/server.hpp"

#endif

#include "coro/io_scheduler.hpp"
#include "coro/poll.hpp"
#include "coro/sync_wait.hpp"
#include "coro/task.hpp"
#include "coro/thread_pool.hpp"
#include "coro/when_all.hpp"

#endif  // CORO_CORO_HPP
