#include "ConcurrencyAndProcess.hpp"

#include <cstring>

namespace Robix::Runtime {

// ============================================================================
// 1. STRUCTURED ASYNC SCOPE IMPLEMENTATION
// ============================================================================

void StructuredAsyncScope::SpawnTask(std::function<void(AsyncTaskContext&)> taskBody) {
    CancellationToken childTok = m_rootToken.CreateLinkedChildToken();
    m_futures.push_back(std::async(std::launch::async, [this, childTok, body = std::move(taskBody)]() {
        AsyncTaskContext ctx(childTok);
        if (!ctx.CheckCancellationPoint() && body) {
            body(ctx);
        }
        ctx.RunDeferStackLIFO();
        if (ctx.WasCancelled() || ctx.Token().IsCancellationRequested()) {
            m_cancelledTasks.fetch_add(1, std::memory_order_relaxed);
        } else {
            m_completedTasks.fetch_add(1, std::memory_order_relaxed);
        }
    }));
}

void StructuredAsyncScope::JoinAll() {
    for (auto& fut : m_futures) {
        if (fut.valid()) {
            fut.wait();
        }
    }
    m_futures.clear();
}

// ============================================================================
// 2. DEADLOCK DETECTOR IMPLEMENTATION
// ============================================================================

bool DeadlockDetector::HasPath(const std::string& from, const std::string& to) const {
    if (from == to) return true;
    std::unordered_set<std::string> visited;
    std::deque<std::string> q;
    visited.insert(from);
    q.push_back(from);

    while (!q.empty()) {
        std::string cur = q.front();
        q.pop_front();
        auto it = m_lockGraph.find(cur);
        if (it == m_lockGraph.end()) continue;
        for (const auto& nxt : it->second) {
            if (nxt == to) return true;
            if (visited.insert(nxt).second) {
                q.push_back(nxt);
            }
        }
    }
    return false;
}

bool DeadlockDetector::RecordLockOrderEdge(const std::string& heldLock,
                                           const std::string& requestedLock,
                                           Frontend::SourceLocation loc) {
    if (heldLock.empty() || requestedLock.empty()) return true;

    // If requestedLock already transitively acquires heldLock, a deadlock cycle exists!
    if (HasPath(requestedLock, heldLock)) {
        m_diagnostics.ReportError(
            "T7001",
            "Potential deadlock cycle detected: Thread holding '" + heldLock +
                "' is requesting '" + requestedLock + "', violating established lock acquisition order.",
            loc,
            "Acquire locks in a consistent global order across all threads.");
        return false;
    }

    m_lockGraph[heldLock].insert(requestedLock);
    return true;
}

// ============================================================================
// 3. THREAD POOL, PROCESS & SHARED MEMORY IMPLEMENTATION
// ============================================================================

ThreadPool::ThreadPool(std::size_t workerCount) {
    const std::size_t n = (workerCount == 0) ? 1 : workerCount;
    m_workers.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        m_workers.emplace_back([this]() {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_mtx);
                    m_cv.wait(lock, [this]() { return m_stop || !m_tasks.empty(); });
                    if (m_stop && m_tasks.empty()) {
                        return;
                    }
                    task = std::move(m_tasks.front());
                    m_tasks.pop_front();
                }
                if (task) {
                    task();
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    Shutdown();
}

void ThreadPool::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (m_stop) return;
        m_stop = true;
    }
    m_cv.notify_all();
    for (auto& w : m_workers) {
        if (w.joinable()) {
            w.join();
        }
    }
}

ProcessResult ProcessManager::LaunchRobixSubprocess(
    std::string_view workerScriptName,
    const std::vector<std::string>& args,
    const std::function<int(const std::vector<std::string>&, std::string&)>& workerEntry) {

    (void)workerScriptName;
    ProcessResult res;
    auto fut = std::async(std::launch::async, [&]() {
        if (workerEntry) {
            res.exitCode = workerEntry(args, res.stdoutText);
        }
    });
    fut.get();
    return res;
}

bool SharedMemoryRegion::WriteBytes(std::size_t offset, const void* src, std::size_t len) {
    std::unique_lock<std::shared_mutex> lock(m_rw);
    if (!src || (offset + len) > m_buffer.size()) return false;
    std::memcpy(m_buffer.data() + offset, src, len);
    return true;
}

bool SharedMemoryRegion::ReadBytes(std::size_t offset, void* dst, std::size_t len) const {
    std::shared_lock<std::shared_mutex> lock(m_rw);
    if (!dst || (offset + len) > m_buffer.size()) return false;
    std::memcpy(dst, m_buffer.data() + offset, len);
    return true;
}

} // namespace Robix::Runtime

