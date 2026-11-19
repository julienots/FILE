#include "agr/core/JobSystem.hpp"

#include <algorithm>

#include "agr/core/Log.hpp"
#include "agr/core/Platform.hpp"

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    #include <intrin.h>
#endif

namespace agr {

namespace {

/// Hints the CPU that we are in a spin-wait loop (lower power, frees the
/// sibling hyper-thread, avoids memory-order mis-speculation on exit).
AGR_FORCE_INLINE void cpuRelax() {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    _mm_pause();
#elif defined(__i386__) || defined(__x86_64__)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ volatile("yield");
#else
    std::this_thread::yield();
#endif
}

#if defined(AGR_PLATFORM_MOBILE)
constexpr u32 kSpinIterations = 2'000;   // ~10-20 us on a big core: battery first
#else
constexpr u32 kSpinIterations = 20'000;  // ~50-150 us: covers the gaps between jobs of one frame
#endif

} // namespace

u32 JobSystem::recommendedWorkerCount() {
    const u32 hw = std::max(1u, std::thread::hardware_concurrency());
#if defined(AGR_PLATFORM_MOBILE)
    // Leave the main + render threads alone and avoid waking every little core.
    return std::clamp(hw > 2 ? hw - 2 : 1u, 1u, 4u);
#else
    return std::clamp(hw > 1 ? hw - 1 : 1u, 1u, 16u);
#endif
}

JobSystem::JobSystem(u32 workerCount) : m_ring(new Cell[kRingSize]) {
    for (usize i = 0; i < kRingSize; ++i) m_ring[i].sequence.store(i, std::memory_order_relaxed);
    const u32 count = workerCount == 0 ? recommendedWorkerCount() : workerCount;
    m_workers.reserve(count);
    for (u32 i = 0; i < count; ++i) m_workers.emplace_back([this] { workerLoop(); });
    AGR_LOG_DEBUG("Jobs", "JobSystem started with {} worker(s)", count);
}

JobSystem::~JobSystem() {
    {
        std::lock_guard lock(m_mutex);
        m_stop.store(true, std::memory_order_seq_cst);
    }
    m_cv.notify_all();
    for (auto& t : m_workers) t.join();
    // Run anything left so that counters are always released.
    while (tryRunOne()) {
    }
}

// ----------------------------------------------------------- MPMC ring
bool JobSystem::ringPush(Task& task) {
    usize pos = m_enqueuePos.load(std::memory_order_relaxed);
    for (;;) {
        Cell& cell = m_ring[pos & (kRingSize - 1)];
        const usize seq = cell.sequence.load(std::memory_order_acquire);
        const auto diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);
        if (diff == 0) {
            // seq_cst: pairs with the parking protocol (see wake()).
            if (m_enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                cell.task = std::move(task);
                cell.sequence.store(pos + 1, std::memory_order_release);
                return true;
            }
        } else if (diff < 0) {
            return false;  // full
        } else {
            pos = m_enqueuePos.load(std::memory_order_relaxed);
        }
    }
}

bool JobSystem::ringPop(Task& out) {
    usize pos = m_dequeuePos.load(std::memory_order_relaxed);
    for (;;) {
        Cell& cell = m_ring[pos & (kRingSize - 1)];
        const usize seq = cell.sequence.load(std::memory_order_acquire);
        const auto diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos + 1);
        if (diff == 0) {
            if (m_dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                out = std::move(cell.task);
                cell.task.job = nullptr;  // release captured state now, not at the next lap
                cell.sequence.store(pos + kRingSize, std::memory_order_release);
                return true;
            }
        } else if (diff < 0) {
            return false;  // empty
        } else {
            pos = m_dequeuePos.load(std::memory_order_relaxed);
        }
    }
}

void JobSystem::push(Task task) {
    if (ringPush(task)) return;
    std::lock_guard lock(m_mutex);
    m_overflow.push_back(std::move(task));
    m_overflowCount.fetch_add(1, std::memory_order_seq_cst);
}

bool JobSystem::hasWork() const {
    return m_enqueuePos.load(std::memory_order_seq_cst) != m_dequeuePos.load(std::memory_order_seq_cst) ||
           m_overflowCount.load(std::memory_order_seq_cst) != 0;
}

bool JobSystem::tryPop(Task& out) {
    if (ringPop(out)) return true;
    if (m_overflowCount.load(std::memory_order_acquire) == 0) return false;
    std::lock_guard lock(m_mutex);
    if (m_overflow.empty()) return false;
    out = std::move(m_overflow.front());
    m_overflow.pop_front();
    m_overflowCount.fetch_sub(1, std::memory_order_relaxed);
    return true;
}

// ---------------------------------------------------------- submission
void JobSystem::wake(usize jobCount) {
    // The push above was seq_cst, and so is this load: paired with the parking
    // protocol in workerLoop(), either the worker sees the job before it sleeps,
    // or we see it in m_sleeping here.
    const u32 sleeping = m_sleeping.load(std::memory_order_seq_cst);
    if (sleeping == 0 || jobCount == 0) return;  // spinning / busy workers will pick the work up
    { std::lock_guard lock(m_mutex); }           // the worker is now inside m_cv.wait()
    if (jobCount >= sleeping) m_cv.notify_all();
    else for (usize i = 0; i < jobCount; ++i) m_cv.notify_one();
}

void JobSystem::submit(Job job, JobCounter* counter) {
    if (counter) counter->m_pending.fetch_add(1, std::memory_order_acq_rel);
    push(Task{std::move(job), counter});
    wake(1);
}

void JobSystem::submitBatch(std::vector<Job> jobs, JobCounter* counter) {
    if (jobs.empty()) return;
    if (counter) counter->m_pending.fetch_add(static_cast<u32>(jobs.size()), std::memory_order_acq_rel);
    for (auto& j : jobs) push(Task{std::move(j), counter});  // each job is visible as soon as pushed
    wake(jobs.size());
}

void JobSystem::run(Task& task) {
    task.job();
    task.job = nullptr;
    if (task.counter) task.counter->m_pending.fetch_sub(1, std::memory_order_acq_rel);
}

bool JobSystem::tryRunOne() {
    Task task;
    if (!tryPop(task)) return false;
    run(task);
    return true;
}

// ------------------------------------------------------------- workers
void JobSystem::workerLoop() {
    Task task;
    for (;;) {
        if (tryPop(task)) {
            run(task);
            continue;
        }
        // Spin phase: cheap polling of an atomic, no lock, no syscall.
        bool found = false;
        for (u32 spin = 0; spin < kSpinIterations; ++spin) {
            if (m_enqueuePos.load(std::memory_order_relaxed) != m_dequeuePos.load(std::memory_order_relaxed) ||
                m_overflowCount.load(std::memory_order_relaxed) != 0) {
                found = true;
                break;
            }
            if (m_stop.load(std::memory_order_relaxed)) break;
            cpuRelax();
        }
        if (found) continue;
        // Park.
        std::unique_lock lock(m_mutex);
        m_sleeping.fetch_add(1, std::memory_order_seq_cst);
        m_cv.wait(lock, [this] { return m_stop.load(std::memory_order_seq_cst) || hasWork(); });
        m_sleeping.fetch_sub(1, std::memory_order_relaxed);
        if (m_stop.load(std::memory_order_acquire) && !hasWork()) return;
    }
}

void JobSystem::wait(const JobCounter& counter) {
    u32 idle = 0;
    while (!counter.done()) {
        if (tryRunOne()) {
            idle = 0;
            continue;
        }
        // The remaining jobs run on workers: spin a little, then yield the core.
        if (++idle < 64) cpuRelax();
        else std::this_thread::yield();
    }
}

void JobSystem::parallelFor(u32 count, u32 batchSize, const std::function<void(u32, u32)>& fn) {
    if (count == 0) return;
    batchSize = std::max(1u, batchSize);
    if (count <= batchSize || m_workers.empty()) {
        fn(0, count);  // a single batch: no queueing, no wake-up
        return;
    }
    JobCounter counter;
    std::vector<Job> batch;
    batch.reserve((count + batchSize - 1) / batchSize);
    // The first batch stays on the calling thread; the others go to the workers.
    for (u32 begin = batchSize; begin < count; begin += batchSize) {
        const u32 end = std::min(count, begin + batchSize);
        batch.emplace_back([&fn, begin, end] { fn(begin, end); });
    }
    submitBatch(std::move(batch), &counter);
    fn(0, std::min(count, batchSize));
    wait(counter);
}

} // namespace agr
