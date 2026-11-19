#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
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
///  - Jobs go through a bounded lock-free MPMC ring (D. Vyukov's algorithm):
///    submitting and taking a job is a couple of atomic operations, no mutex.
///    When the ring is full, jobs spill into a mutex-protected overflow list.
///  - Idle workers spin briefly (CPU pause instruction) before parking on a
///    condition variable: back-to-back submissions inside a frame are picked
///    up without a futex wake-up / context switch per job. The spin window is
///    shorter on mobile to protect battery life. The mutex is only taken to
///    park or to wake a parked worker.
///  - parallelFor runs one batch on the calling thread and only wakes as many
///    workers as there are remaining batches.
class JobSystem {
public:
    using Job = std::function<void()>;

    /// workerCount = 0 selects an automatic count.
    explicit JobSystem(u32 workerCount = 0);
    ~JobSystem();
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void submit(Job job, JobCounter* counter = nullptr);
    /// Enqueues many jobs with a single wake-up (cheaper than N submits).
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
        JobCounter* counter = nullptr;
    };
    static constexpr usize kCacheLine = 64;
    static constexpr usize kRingSize = 16384;  ///< Power of two (16k jobs in flight before the overflow list).
    struct Cell {
        std::atomic<usize> sequence{0};
        Task task;
    };

    void workerLoop();
    bool tryRunOne();
    bool hasWork() const;
    bool tryPop(Task& out);
    bool ringPush(Task& task);
    bool ringPop(Task& out);
    void push(Task task);
    void run(Task& task);
    void wake(usize jobCount);

    std::vector<std::thread> m_workers;
    std::unique_ptr<Cell[]> m_ring;
    alignas(kCacheLine) std::atomic<usize> m_enqueuePos{0};
    alignas(kCacheLine) std::atomic<usize> m_dequeuePos{0};
    std::atomic<u32> m_sleeping{0};                    ///< Workers parked on m_cv.
    std::atomic<u32> m_overflowCount{0};
    std::atomic<bool> m_stop{false};
    std::mutex m_mutex;               ///< Guards m_overflow and parking.
    std::condition_variable m_cv;
    std::deque<Task> m_overflow;
};

} // namespace agr
