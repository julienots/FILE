#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <numeric>

#include "agr/core/JobSystem.hpp"

using namespace agr;

TEST_SUITE("jobs") {

TEST_CASE("parallelFor covers every index exactly once") {
    JobSystem jobs(4);
    std::vector<std::atomic<int>> hits(10007);
    jobs.parallelFor(static_cast<u32>(hits.size()), 64, [&](u32 b, u32 e) {
        for (u32 i = b; i < e; ++i) hits[i].fetch_add(1);
    });
    for (auto& h : hits) REQUIRE(h.load() == 1);
}

TEST_CASE("counters track groups of jobs") {
    JobSystem jobs(2);
    JobCounter counter;
    std::atomic<int> sum{0};
    for (int i = 1; i <= 100; ++i) jobs.submit([&sum, i] { sum += i; }, &counter);
    jobs.wait(counter);
    CHECK(counter.done());
    CHECK(sum.load() == 5050);
}

TEST_CASE("nested waits do not dead-lock with a single worker") {
    JobSystem jobs(1);
    std::atomic<int> leaves{0};
    JobCounter outer;
    for (int i = 0; i < 8; ++i) {
        jobs.submit(
            [&] {
                JobCounter inner;
                for (int k = 0; k < 8; ++k) jobs.submit([&] { ++leaves; }, &inner);
                jobs.wait(inner);
            },
            &outer);
    }
    jobs.wait(outer);
    CHECK(leaves.load() == 64);
}

TEST_CASE("destructor drains queued jobs") {
    std::atomic<int> done{0};
    {
        JobSystem jobs(2);
        for (int i = 0; i < 1000; ++i) jobs.submit([&] { ++done; });
    }
    CHECK(done.load() == 1000);
}

}  // TEST_SUITE

TEST_CASE("submitBatch runs every job and releases the counter") {
    JobSystem jobs(3);
    std::atomic<int> n{0};
    JobCounter c;
    std::vector<JobSystem::Job> batch;
    for (int i = 0; i < 500; ++i) batch.emplace_back([&n] { ++n; });
    jobs.submitBatch(std::move(batch), &c);
    jobs.wait(c);
    CHECK(n.load() == 500);
    jobs.submitBatch({}, &c);  // empty batch is a no-op
    CHECK(c.done());
}

TEST_CASE("more jobs than the lock-free ring holds spill over and all run, from several producers") {
    JobSystem jobs(3);
    std::atomic<int> n{0};
    JobCounter c;
    // 50k jobs submitted at once (the ring holds 16k) plus jobs that submit jobs.
    std::vector<JobSystem::Job> batch;
    for (int i = 0; i < 50000; ++i) batch.emplace_back([&n] { n.fetch_add(1, std::memory_order_relaxed); });
    jobs.submitBatch(std::move(batch), &c);
    for (int p = 0; p < 8; ++p)
        jobs.submit(
            [&] {
                for (int k = 0; k < 1000; ++k) jobs.submit([&n] { n.fetch_add(1, std::memory_order_relaxed); }, &c);
            },
            &c);
    jobs.wait(c);
    CHECK(n.load() == 58000);
    CHECK(c.done());
}

TEST_CASE("workers asleep between bursts still wake up for new work") {
    JobSystem jobs(2);
    for (int burst = 0; burst < 3; ++burst) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));  // longer than the spin window: workers park
        std::atomic<int> n{0};
        JobCounter c;
        for (int i = 0; i < 100; ++i) jobs.submit([&n] { ++n; }, &c);
        jobs.wait(c);
        CHECK(n.load() == 100);
    }
    // parallelFor with one batch runs inline; with none, nothing runs.
    int calls = 0;
    jobs.parallelFor(10, 64, [&](u32 b, u32 e) { calls += static_cast<int>(e - b); });
    CHECK(calls == 10);
    jobs.parallelFor(0, 64, [&](u32, u32) { ++calls; });
    CHECK(calls == 10);
}
