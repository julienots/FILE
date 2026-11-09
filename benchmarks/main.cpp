// AGR ENGINE micro-benchmarks. Run a Release build:
//   cmake --preset release && cmake --build --preset release && ./build/release/bin/agr_benchmarks
// Results are recorded in docs/BENCHMARKS.md.
#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "agr/core/JobSystem.hpp"
#include "agr/core/Memory.hpp"
#include "agr/core/Time.hpp"
#include "agr/ecs/Registry.hpp"
#include "agr/math/Math.hpp"
#include "agr/scene/Scene.hpp"
#include "agr/scene/SceneSerializer.hpp"

using namespace agr;

namespace {

volatile float g_sink = 0.0f;  // defeats dead-code elimination

/// Runs `fn` `reps` times and reports the best run (least noisy on shared machines).
void bench(const char* name, u64 itemsPerRun, int reps, const std::function<void()>& fn) {
    fn();  // warm-up
    f64 best = 1e30;
    for (int i = 0; i < reps; ++i) {
        Stopwatch sw;
        fn();
        best = std::min(best, sw.elapsedSeconds());
    }
    std::printf("%-44s %10.3f ms   %8.2f ns/item\n", name, best * 1e3, best * 1e9 / static_cast<f64>(itemsPerRun));
}

struct Position { math::Vec3 v; };
struct Velocity { math::Vec3 v; };
struct Health { float hp = 100.0f; };

}  // namespace

int main() {
    std::printf("AGR ENGINE benchmarks\n%-44s %13s   %14s\n", "case", "best", "per item");
    constexpr u32 N = 1'000'000;

    // ------------------------------------------------------------ ECS
    bench("ecs: create 1M entities + 2 components", N, 5, [] {
        ecs::Registry r;
        for (u32 i = 0; i < N; ++i) {
            const auto e = r.create();
            r.emplace<Position>(e);
            r.emplace<Velocity>(e, Velocity{{1, 0, 0}});
        }
    });
    {
        ecs::Registry r;
        for (u32 i = 0; i < N; ++i) {
            const auto e = r.create();
            r.emplace<Position>(e);
            r.emplace<Velocity>(e, Velocity{{1, 0, 0}});
            if (i % 4 == 0) r.emplace<Health>(e);
        }
        bench("ecs: view<Position,Velocity> 1M", N, 10, [&] {
            r.view<Position, Velocity>().each([](ecs::Entity, Position& p, Velocity& v) { p.v += v.v * 0.016f; });
        });
        bench("ecs: view<Health,Position> (250k matches)", N / 4, 10, [&] {
            r.view<Health, Position>().each([](ecs::Entity, Health& h, Position& p) { h.hp -= p.v.x * 0.0001f; });
        });
        bench("ecs: raw pool iteration Position 1M", N, 10, [&] {
            for (auto& p : r.pool<Position>().data()) p.v.y += 1.0f;
        });
        std::vector<ecs::Entity> ents = r.view<Position>().entities();
        bench("ecs: random get<Velocity> 1M", N, 5, [&] {
            float acc = 0;
            for (u32 i = 0; i < N; ++i) acc += r.get<Velocity>(ents[(i * 2654435761u) % N]).v.x;
            g_sink = acc;
        });
    }

    // ---------------------------------------------------------- memory
    bench("memory: LinearAllocator 1M x 32B", N, 10, [] {
        static LinearAllocator a(N * 48);
        a.reset();
        for (u32 i = 0; i < N; ++i) g_sink = static_cast<float>(reinterpret_cast<std::uintptr_t>(a.allocate(32, 16)) & 1);
    });
    bench("memory: PoolAllocator 1M alloc+free 64B", N, 10, [] {
        static PoolAllocator pool(64, 4096);
        static std::vector<void*> ptrs(N);
        for (u32 i = 0; i < N; ++i) ptrs[i] = pool.allocate(64);
        for (u32 i = 0; i < N; ++i) pool.deallocate(ptrs[i]);
    });
    bench("memory: malloc/free 1M x 64B (reference)", N, 10, [] {
        static std::vector<void*> ptrs(N);
        for (u32 i = 0; i < N; ++i) ptrs[i] = std::malloc(64);
        for (u32 i = 0; i < N; ++i) std::free(ptrs[i]);
    });

    // ------------------------------------------------------------ jobs
    {
        JobSystem jobs;
        std::vector<math::Mat4> mats(N);
        auto work = [&](u32 b, u32 e) {
            for (u32 i = b; i < e; ++i)
                mats[i] = math::Mat4::trs({float(i), 0, 0}, math::Quat::fromAxisAngle({0, 1, 0}, float(i) * 0.001f), math::Vec3{1});
        };
        bench("jobs: 1M TRS matrices, single thread", N, 5, [&] { work(0, N); });
        char label[64];
        std::snprintf(label, sizeof label, "jobs: 1M TRS matrices, parallelFor (%u w)", jobs.workerCount());
        bench(label, N, 5, [&] { jobs.parallelFor(N, 4096, work); });
        bench("jobs: 10k empty jobs submit+wait", 10000, 10, [&] {
            JobCounter c;
            for (int i = 0; i < 10000; ++i) jobs.submit([] {}, &c);
            jobs.wait(c);
        });
        bench("jobs: 10k empty jobs submitBatch+wait", 10000, 10, [&] {
            JobCounter c;
            std::vector<JobSystem::Job> batch(10000, [] {});
            jobs.submitBatch(std::move(batch), &c);
            jobs.wait(c);
        });
    }

    // ----------------------------------------------------------- scene
    {
        Scene scene;
        // 10k roots with 9 children each = 100k entities.
        for (int i = 0; i < 10000; ++i) {
            const auto root = scene.createEntity("Root");
            scene.registry().get<TransformComponent>(root).position = {float(i), 0, 0};
            for (int k = 0; k < 9; ++k) scene.createEntity("Child", root);
        }
        bench("scene: updateTransforms 100k entities", 100000, 10, [&] { scene.updateTransforms(); });
        std::string text;
        bench("scene: serialize 100k entities to JSON", 100000, 3, [&] { text = SceneSerializer::toString(scene, -1); });
        bench("scene: load 100k entities from JSON", 100000, 3, [&] {
            Scene loaded;
            (void)SceneSerializer::fromString(loaded, text);
        });
        std::printf("    (scene JSON size: %.1f MB)\n", static_cast<double>(text.size()) / (1024.0 * 1024.0));
    }
    return 0;
}
