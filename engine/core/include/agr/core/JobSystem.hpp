#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "agr/core/Types.hpp"

namespace agr {

/// Tracks completion of a group of jobs. Wait on it with JobSystem::wait().
class JobCounter {
public:
    bool done() const { return m_pending.load(std::memory_order_acquire) == 0; }
    u32 pending() const { return m_pending.load(std::memory_order_acquire); }

private:
    friend class JobSystem;
    std::atomic<u32> m_pending{0};
};

/// Fixed pool of worker threads executing short tasks.
///
/// Design notes:
///  - Workers = hardware threads - 1 (main thread participates while waiting),
///    capped for mobile big.LITTLE CPUs where too many threads hurt battery.
///  - wait() executes pending jobs on the calling thread instead of blocking,
///    so nested parallelism never dead-locks.
class JobSystem {
public:
    using Job = std::function<void()>;

    /// workerCount = 0 selects an automatic count.
    explicit JobSystem(u32 workerCount = 0);
    ~JobSystem();
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void submit(Job job, JobCounter* counter = nullptr);
    /// Enqueues many jobs with a single lock / wake-up (cheaper than N submits).
    void submitBatch(std::vector<Job> jobs, JobCounter* counter = nullptr);

    /// Splits [0, count) into batches of `batchSize` and runs fn(begin, end) in parallel.
    void parallelFor(u32 count, u32 batchSize, const std::function<void(u32 begin, u32 end)>& fn);

    /// Blocks until `counter` reaches zero, helping with queued work meanwhile.
    void wait(const JobCounter& counter);

    u32 workerCount() const { return static_cast<u32>(m_workers.size()); }
    static u32 recommendedWorkerCount();

private:
    struct Task {
        Job job;
        JobCounter* counter;
    };
    void workerLoop();
    bool tryRunOne();
    void run(Task& task);

    std::vector<std::thread> m_workers;
    std::deque<Task> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    u32 m_sleeping = 0;  ///< Workers blocked on m_cv (guarded by m_mutex).
    bool m_stop = false;
};

} // namespace agr
