#include "coro/sync_wait.hpp"

namespace coro::detail {

    SyncWaitEvent::SyncWaitEvent(bool set) : m_set(set) {}

    void SyncWaitEvent::set() noexcept {
        m_set.exchange(true, std::memory_order::seq_cst);
        std::unique_lock <std::mutex> lk{m_mutex};
        m_cv.notify_all();
    }

    void SyncWaitEvent::reset() noexcept { m_set.exchange(false, std::memory_order::seq_cst); }

    void SyncWaitEvent::wait() noexcept {
        std::unique_lock <std::mutex> lk{m_mutex};
        m_cv.wait(lk, [&]() {
            return m_set.load(std::memory_order::seq_cst);
        });
    }

}