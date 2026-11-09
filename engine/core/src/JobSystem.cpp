#include "agr/core/JobSystem.hpp"

#include <algorithm>

#include "agr/core/Log.hpp"
#include "agr/core/Platform.hpp"

namespace agr {

u32 JobSystem::recommendedWorkerCount() {
    const u32 hw = std::max(1u, std::thread::hardware_concurrency());
#if defined(AGR_PLATFORM_MOBILE)
    // Leave the main + render threads alone and avoid waking every little core.
    return std::clamp(hw > 2 ? hw - 2 : 1u, 1u, 4u);
#else
    return std::clamp(hw > 1 ? hw - 1 : 1u, 1u, 16u);
#endif
}

JobSystem::JobSystem(u32 workerCount) {
    const u32 count = workerCount == 0 ? recommendedWorkerCount() : workerCount;
    m_workers.reserve(count);
    for (u32 i = 0; i < count; ++i) m_workers.emplace_back([this] { workerLoop(); });
    AGR_LOG_DEBUG("Jobs", "JobSystem started with {} worker(s)", count);
}

JobSystem::~JobSystem() {
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    for (auto& t : m_workers) t.join();
    // Run anything left so that counters are always released.
    while (tryRunOne()) {
    }
}

void JobSystem::submit(Job job, JobCounter* counter) {
    if (counter) counter->m_pending.fetch_add(1, std::memory_order_acq_rel);
    bool wake;
    {
        std::lock_guard lock(m_mutex);
        m_queue.push_back(Task{std::move(job), counter});
        wake = m_sleeping > 0;  // skip the futex syscall when every worker is busy
    }
    if (wake) m_cv.notify_one();
}

void JobSystem::submitBatch(std::vector<Job> jobs, JobCounter* counter) {
    if (jobs.empty()) return;
    if (counter) counter->m_pending.fetch_add(static_cast<u32>(jobs.size()), std::memory_order_acq_rel);
    u32 sleeping;
    {
        std::lock_guard lock(m_mutex);
        for (auto& j : jobs) m_queue.push_back(Task{std::move(j), counter});
        sleeping = m_sleeping;
    }
    if (sleeping == 0) return;
    if (jobs.size() >= sleeping) m_cv.notify_all();
    else for (usize i = 0; i < jobs.size(); ++i) m_cv.notify_one();
}

void JobSystem::run(Task& task) {
    task.job();
    if (task.counter) task.counter->m_pending.fetch_sub(1, std::memory_order_acq_rel);
}

bool JobSystem::tryRunOne() {
    Task task;
    {
        std::lock_guard lock(m_mutex);
        if (m_queue.empty()) return false;
        task = std::move(m_queue.front());
        m_queue.pop_front();
    }
    run(task);
    return true;
}

void JobSystem::workerLoop() {
    for (;;) {
        Task task;
        {
            std::unique_lock lock(m_mutex);
            if (m_queue.empty() && !m_stop) {
                ++m_sleeping;
                m_cv.wait(lock, [this] { return m_stop || !m_queue.empty(); });
                --m_sleeping;
            }
            if (m_queue.empty()) return;  // m_stop and nothing left.
            task = std::move(m_queue.front());
            m_queue.pop_front();
        }
        run(task);
    }
}

void JobSystem::wait(const JobCounter& counter) {
    while (!counter.done()) {
        if (!tryRunOne()) std::this_thread::yield();
    }
}

void JobSystem::parallelFor(u32 count, u32 batchSize, const std::function<void(u32, u32)>& fn) {
    if (count == 0) return;
    batchSize = std::max(1u, batchSize);
    JobCounter counter;
    std::vector<Job> batch;
    batch.reserve((count + batchSize - 1) / batchSize);
    for (u32 begin = 0; begin < count; begin += batchSize) {
        const u32 end = std::min(count, begin + batchSize);
        batch.emplace_back([&fn, begin, end] { fn(begin, end); });
    }
    submitBatch(std::move(batch), &counter);
    wait(counter);
}

} // namespace agr
