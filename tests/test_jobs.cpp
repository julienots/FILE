#include <doctest/doctest.h>

#include <atomic>
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
