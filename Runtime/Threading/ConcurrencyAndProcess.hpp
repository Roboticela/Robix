#pragma once

#include "../../Compiler/Semantic/Diagnostics.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Robix::Runtime {

// ============================================================================
// 1. HIERARCHICAL CANCELLATION TOKEN & STRUCTURED ASYNC CANCELLATION
// ============================================================================
class CancellationState {
public:
    void Cancel() noexcept {
        m_cancelled.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lock(m_mtx);
        for (auto& child : m_children) {
            if (child) child->Cancel();
        }
    }

    [[nodiscard]] bool IsCancelled() const noexcept {
        return m_cancelled.load(std::memory_order_acquire);
    }

    void AddChild(std::shared_ptr<CancellationState> child) {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (IsCancelled() && child) {
            child->Cancel();
        }
        m_children.push_back(std::move(child));
    }

private:
    std::atomic<bool> m_cancelled{ false };
    std::mutex m_mtx;
    std::vector<std::shared_ptr<CancellationState>> m_children;
};

class CancellationToken {
public:
    CancellationToken() : m_state(std::make_shared<CancellationState>()) {}
    explicit CancellationToken(std::shared_ptr<CancellationState> state) : m_state(std::move(state)) {}

    [[nodiscard]] bool IsCancellationRequested() const noexcept {
        return m_state && m_state->IsCancelled();
    }

    [[nodiscard]] CancellationToken CreateLinkedChildToken() const {
        auto childState = std::make_shared<CancellationState>();
        if (m_state) {
            m_state->AddChild(childState);
        }
        return CancellationToken(std::move(childState));
    }

    void RequestCancel() noexcept {
        if (m_state) m_state->Cancel();
    }

private:
    std::shared_ptr<CancellationState> m_state;
};

// Executes an async task with guaranteed LIFO execution of `defer`/`using` blocks
// even when cancelled midway through execution!
class AsyncTaskContext {
public:
    explicit AsyncTaskContext(CancellationToken token) : m_token(std::move(token)) {}
    ~AsyncTaskContext() { RunDeferStackLIFO(); }

    void RegisterDeferCleanup(std::function<void()> cleanupFn) {
        m_deferStack.push_back(std::move(cleanupFn));
    }

    [[nodiscard]] bool CheckCancellationPoint() noexcept {
        if (m_token.IsCancellationRequested()) {
            m_wasCancelled = true;
            RunDeferStackLIFO();
            return true;
        }
        return false;
    }

    [[nodiscard]] bool WasCancelled() const noexcept { return m_wasCancelled; }
    [[nodiscard]] const CancellationToken& Token() const noexcept { return m_token; }

    void RunDeferStackLIFO() noexcept {
        while (!m_deferStack.empty()) {
            auto fn = std::move(m_deferStack.back());
            m_deferStack.pop_back();
            if (fn) {
                try { fn(); } catch (...) {}
            }
        }
    }

private:
    CancellationToken                  m_token;
    bool                               m_wasCancelled = false;
    std::vector<std::function<void()>> m_deferStack;
};

class StructuredAsyncScope {
public:
    StructuredAsyncScope() = default;
    ~StructuredAsyncScope() {
        CancelAllChildren();
        JoinAll();
    }

    void SpawnTask(std::function<void(AsyncTaskContext&)> taskBody);
    void CancelAllChildren() noexcept { m_rootToken.RequestCancel(); }
    void JoinAll();

    [[nodiscard]] std::size_t CompletedTaskCount() const noexcept { return m_completedTasks.load(); }
    [[nodiscard]] std::size_t CancelledTaskCount() const noexcept { return m_cancelledTasks.load(); }

private:
    CancellationToken              m_rootToken;
    std::vector<std::future<void>> m_futures;
    std::atomic<std::size_t>       m_completedTasks{ 0 };
    std::atomic<std::size_t>       m_cancelledTasks{ 0 };
};

// ============================================================================
// 2. SYNCHRONIZATION PRIMITIVES, CHANNEL<T> & DEADLOCK DETECTOR
// ============================================================================
class RobixMutex {
public:
    template <typename Fn>
    auto WithLock(Fn&& fn) {
        std::lock_guard<std::mutex> lock(m_mtx);
        return fn();
    }

private:
    std::mutex m_mtx;
};

class ReadWriteLock {
public:
    template <typename Fn>
    auto WithReadLock(Fn&& fn) {
        std::shared_lock<std::shared_mutex> lock(m_rw);
        return fn();
    }

    template <typename Fn>
    auto WithWriteLock(Fn&& fn) {
        std::unique_lock<std::shared_mutex> lock(m_rw);
        return fn();
    }

private:
    std::shared_mutex m_rw;
};

class Semaphore {
public:
    explicit Semaphore(std::size_t initialPermits) : m_permits(initialPermits) {}

    void Acquire() {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cv.wait(lock, [this]() { return m_permits > 0; });
        --m_permits;
    }

    void Release() {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            ++m_permits;
        }
        m_cv.notify_one();
    }

    [[nodiscard]] std::size_t AvailablePermits() const noexcept {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_permits;
    }

private:
    mutable std::mutex      m_mtx;
    std::condition_variable m_cv;
    std::size_t             m_permits = 0;
};

template <typename T>
class Channel {
public:
    explicit Channel(std::size_t capacity = 64) : m_capacity(capacity) {}

    bool Send(T item) {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_notFull.wait(lock, [this]() { return m_closed || m_queue.size() < m_capacity; });
        if (m_closed) return false;
        m_queue.push_back(std::move(item));
        m_notEmpty.notify_one();
        return true;
    }

    [[nodiscard]] std::optional<T> Receive() {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_notEmpty.wait(lock, [this]() { return m_closed || !m_queue.empty(); });
        if (m_queue.empty()) return std::nullopt;
        T val = std::move(m_queue.front());
        m_queue.pop_front();
        m_notFull.notify_one();
        return val;
    }

    void Close() noexcept {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_closed = true;
        }
        m_notEmpty.notify_all();
        m_notFull.notify_all();
    }

private:
    std::mutex              m_mtx;
    std::condition_variable m_notEmpty;
    std::condition_variable m_notFull;
    std::deque<T>           m_queue;
    std::size_t             m_capacity = 64;
    bool                    m_closed   = false;
};

// Detects lock-order inversion cycles (`LockA -> LockB` vs `LockB -> LockA`)
class DeadlockDetector {
public:
    explicit DeadlockDetector(Frontend::DiagnosticBag& diagnostics) noexcept
        : m_diagnostics(diagnostics) {}

    bool RecordLockOrderEdge(const std::string& heldLock,
                             const std::string& requestedLock,
                             Frontend::SourceLocation loc = {});

private:
    Frontend::DiagnosticBag& m_diagnostics;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_lockGraph;

    [[nodiscard]] bool HasPath(const std::string& from, const std::string& to) const;
};

// ============================================================================
// 3. THREAD POOL & MULTIPROCESSING (Process, IPCPipe, SharedMemory)
// ============================================================================
class ThreadPool {
public:
    explicit ThreadPool(std::size_t workerCount = 4);
    ~ThreadPool();

    template <typename Fn>
    auto Submit(Fn&& fn) -> std::future<decltype(fn())> {
        using RetType = decltype(fn());
        auto taskPkg = std::make_shared<std::packaged_task<RetType()>>(std::forward<Fn>(fn));
        std::future<RetType> fut = taskPkg->get_future();
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_tasks.emplace_back([taskPkg]() { (*taskPkg)(); });
        }
        m_cv.notify_one();
        return fut;
    }

    void Shutdown();

private:
    std::vector<std::thread>          m_workers;
    std::deque<std::function<void()>> m_tasks;
    std::mutex                        m_mtx;
    std::condition_variable           m_cv;
    bool                              m_stop = false;
};

struct ProcessResult {
    int         exitCode = 0;
    std::string stdoutText;
    std::string stderrText;
    bool        timedOut = false;
};

class ProcessManager {
public:
    static ProcessResult LaunchRobixSubprocess(
        std::string_view workerScriptName,
        const std::vector<std::string>& args,
        const std::function<int(const std::vector<std::string>&, std::string&)>& workerEntry);
};

class SharedMemoryRegion {
public:
    explicit SharedMemoryRegion(std::string name, std::size_t sizeBytes)
        : m_name(std::move(name)), m_buffer(sizeBytes, 0) {}

    bool WriteBytes(std::size_t offset, const void* src, std::size_t len);
    bool ReadBytes(std::size_t offset, void* dst, std::size_t len) const;
    [[nodiscard]] const std::string& Name() const noexcept { return m_name; }
    [[nodiscard]] std::size_t SizeBytes() const noexcept { return m_buffer.size(); }

private:
    std::string              m_name;
    mutable std::shared_mutex m_rw;
    std::vector<std::uint8_t> m_buffer;
};

} // namespace Robix::Runtime

