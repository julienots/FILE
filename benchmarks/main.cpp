// AGR ENGINE micro-benchmarks. Run a Release build:
//   cmake --preset release && cmake --build --preset release && ./build/release/bin/agr_benchmarks
// Results are recorded in docs/BENCHMARKS.md.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "agr/core/JobSystem.hpp"
#include "agr/core/Memory.hpp"
#include "agr/core/Time.hpp"
#include "agr/ecs/Registry.hpp"
#include "agr/math/Math.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/scene/Scene.hpp"
#include "agr/scene/SceneSerializer.hpp"
#include "agr/assets/Environment.hpp"
#include "agr/assets/Model.hpp"
#include "agr/navigation/NavGrid.hpp"
#include "agr/navigation/NavMesh.hpp"
#include "agr/physics2d/Physics2DWorld.hpp"
#include "agr/physics3d/Physics3DWorld.hpp"
#include "agr/animation/AnimationSystem.hpp"
#include "agr/audio/AudioSystem.hpp"
#include "agr/audio/Wav.hpp"
#include "agr/ui/UISystem.hpp"
#include "agr/script/ScriptComponent.hpp"
#include "agr/script/ScriptEngine.hpp"
#include "agr/twod/Atlas.hpp"
#include "agr/twod/Particles2D.hpp"
#include "agr/twod/TilemapEditing.hpp"

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

    // -------------------------------------------------------------- 2D
    {
        registerRenderComponents();
        Scene scene;
        const auto e = scene.createEntity("Emitter");
        auto& em = scene.registry().emplace<ParticleEmitter2DComponent>(e);
        em.rate = 0.0f;
        em.burst = em.maxParticles = 10000;
        em.lifetimeMin = em.lifetimeMax = 1000.0f;
        em.drag = 0.5f;
        em.seed = 3;
        scene.updateTransforms();
        simulateParticles2D(scene, 0.016f);
        bench("2d: simulate 10k particles (one step)", 10000, 50, [&] { simulateParticles2D(scene, 0.016f); });

        Scene maps;
        const auto m = maps.createEntity("Map");
        auto& tm = maps.registry().emplace<TilemapComponent>(m);
        tm.width = tm.height = 1000;
        tilemapEnsureLayout(tm);
        for (u32 y = 0; y < 1000; ++y)  // ground-like data: long runs of equal tiles
            for (u32 x = 0; x < 1000; ++x) tm.tiles[y * 1000 + x] = y < 300 ? 1 : y == 300 ? 0 : (x / 40) % 7 == 0 && y < 305 ? 5 : -1;
        std::string text;
        bench("2d: serialize 1000x1000 tilemap (RLE)", 1000000, 5, [&] { text = SceneSerializer::toString(maps, -1); });
        bench("2d: load 1000x1000 tilemap (RLE)", 1000000, 5, [&] {
            Scene loaded;
            (void)SceneSerializer::fromString(loaded, text);
        });
        std::printf("    (tilemap scene JSON size: %.1f KB)\n", static_cast<double>(text.size()) / 1024.0);

        std::vector<AtlasInput> inputs(256);
        for (usize i = 0; i < inputs.size(); ++i) {
            inputs[i].name = std::to_string(i);
            inputs[i].image.width = 16 + static_cast<u32>(i % 5) * 8;
            inputs[i].image.height = 16 + static_cast<u32>(i % 3) * 8;
            inputs[i].image.pixels.assign(static_cast<usize>(inputs[i].image.width) * inputs[i].image.height * 4, static_cast<u8>(i));
        }
        bench("2d: pack atlas of 256 images", 256, 10, [&] { (void)packAtlas(inputs); });
    }

    // -------------------------------------------------------------- 3D import
    {
        // OBJ sphere-like grid (~20k triangles) parsed from text; tangents generated.
        std::string obj;
        const int n = 100;
        for (int y = 0; y <= n; ++y)
            for (int x = 0; x <= n; ++x)
                obj += "v " + std::to_string(x * 0.1f) + " " + std::to_string(std::sin(x * 0.3f) * std::cos(y * 0.2f)) + " " + std::to_string(y * 0.1f) +
                       "\nvt " + std::to_string(x / float(n)) + " " + std::to_string(y / float(n)) + "\n";
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                const int a = y * (n + 1) + x + 1, b = a + 1, c = a + n + 1, d = c + 1;
                obj += "f " + std::to_string(a) + "/" + std::to_string(a) + " " + std::to_string(c) + "/" + std::to_string(c) + " " +
                       std::to_string(d) + "/" + std::to_string(d) + " " + std::to_string(b) + "/" + std::to_string(b) + "\n";
            }
        bench("3d: import OBJ 20k triangles (+normals, tangents)", 20000, 5, [&] { (void)importObj(obj, "assets://grid.obj", nullptr); });
    }
    {
        // Image sky: 2048 x 1024 HDR -> prefiltered mip chain + SH9 (worker thread at load time).
        ImageF sky;
        sky.width = 2048;
        sky.height = 1024;
        sky.rgb.resize(2048u * 1024u * 3u);
        for (usize i = 0; i < sky.rgb.size(); ++i) sky.rgb[i] = 0.2f + static_cast<float>((i * 2654435761u) % 1000u) * 0.001f;
        bench("3d: environment map 2048x1024 (mips + SH9)", 2048u * 1024u, 3, [&] {
            auto env = buildEnvironmentMap(sky, 2048);
            g_sink = env ? env.value().average.x : 0.0f;
        });
    }
    {
        // Navigation: A* on a 512 x 512 grid with random obstacles; navmesh of a 100 x 100 m floor with 200 boxes.
        NavGrid grid;
        grid.reset({0, 0}, 1.0f, 512, 512);
        u32 seed = 12345;
        for (int i = 0; i < 40000; ++i) {
            seed = seed * 1664525u + 1013904223u;
            grid.setBlocked(static_cast<i32>(seed % 512), static_cast<i32>((seed >> 9) % 512), true);
        }
        std::vector<math::Vec2> path;
        bench("nav: A* 512x512 grid, corner to corner (+ smoothing)", 1, 5, [&] { g_sink = grid.findPath({1.5f, 1.5f}, {510.5f, 510.5f}, path) ? 1.0f : 0.0f; });
        std::printf("    (%zu points, %u cells expanded)\n", path.size(), grid.lastExpanded());
        std::vector<math::Vec3> verts{{-50, 0, -50}, {50, 0, -50}, {50, 0, 50}, {-50, 0, 50}};
        std::vector<u32> idx{0, 2, 1, 0, 3, 2};
        for (int b = 0; b < 200; ++b) {
            seed = seed * 1664525u + 1013904223u;
            const float x = static_cast<float>(seed % 90) - 45.0f, z = static_cast<float>((seed >> 8) % 90) - 45.0f;
            const u32 base = static_cast<u32>(verts.size());
            for (int i = 0; i < 8; ++i) verts.push_back({x + ((i & 1) ? 1.0f : -1.0f), (i & 2) ? 2.0f : 0.0f, z + ((i & 4) ? 1.0f : -1.0f)});
            const u32 f[12][3] = {{2, 6, 7}, {2, 7, 3}, {0, 1, 5}, {0, 5, 4}, {0, 2, 3}, {0, 3, 1}, {4, 5, 7}, {4, 7, 6}, {0, 4, 6}, {0, 6, 2}, {1, 3, 7}, {1, 7, 5}};
            for (const auto& t : f)
                for (u32 k2 : t) idx.push_back(base + k2);
        }
        NavMesh mesh;
        bench("nav: build navmesh 100x100 m, 200 boxes (Recast)", 1, 3, [&] { g_sink = mesh.build(verts, idx, {}) ? 1.0f : 0.0f; });
        std::printf("    (%u polygons)\n", mesh.stats().polygons);
        std::vector<math::Vec3> route;
        bench("nav: navmesh path across (Detour)", 1, 20, [&] { g_sink = mesh.findPath({-48, 0, -48}, {48, 0, 48}, route) ? 1.0f : 0.0f; });
    }

    // -------------------------------------------------------------- physics & scripts
    {
        registerPhysics2DComponents();
        Scene scene;
        const auto ground = scene.createEntity("Ground");
        scene.registry().emplace<Collider2DComponent>(ground).size = {200, 1};
        for (int i = 0; i < 1000; ++i) {
            const auto e = scene.createEntity("Box");
            scene.registry().get<TransformComponent>(e).position = {static_cast<float>(i % 50) * 1.1f - 27.0f, 1.0f + static_cast<float>(i / 50) * 1.1f, 0};
            scene.registry().emplace<RigidBody2DComponent>(e);
            scene.registry().emplace<Collider2DComponent>(e);
        }
        Physics2DWorld world;
        for (int i = 0; i < 60; ++i) world.step(scene, 1.0f / 60.0f);  // bodies created, pile settling
        bench("physics2d: step, 1000 boxes piling up", 1000, 60, [&] { world.step(scene, 1.0f / 60.0f); });
    }
    {
        registerPhysics3DComponents();
        Scene scene;
        const auto ground = scene.createEntity("Ground");
        scene.registry().emplace<ColliderComponent>(ground).size = {200, 1, 200};
        for (int i = 0; i < 1000; ++i) {
            const auto e = scene.createEntity("Box");
            scene.registry().get<TransformComponent>(e).position = {static_cast<float>(i % 10) * 1.1f - 5.0f, 1.0f + static_cast<float>(i / 100) * 1.1f,
                                                                    static_cast<float>((i / 10) % 10) * 1.1f - 5.0f};
            scene.registry().emplace<RigidBodyComponent>(e);
            scene.registry().emplace<ColliderComponent>(e);
        }
        Physics3DWorld world;
        for (int i = 0; i < 60; ++i) world.step(scene, 1.0f / 60.0f);
        bench("physics3d: step, 1000 boxes piling up", 1000, 60, [&] { world.step(scene, 1.0f / 60.0f); });
    }
    {
        registerScriptComponent();
        const auto root = std::filesystem::temp_directory_path() / "agr_bench_scripts";
        std::filesystem::create_directories(root);
        VirtualFileSystem vfs;
        vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
        (void)vfs.writeText("assets://mover.lua", "local M = { properties = { speed = 1 } }\n"
                                                  "function M:onUpdate(dt) self.entity:translate(self.speed * dt, 0, 0) end\nreturn M\n");
        Scene scene;
        for (int i = 0; i < 1000; ++i) scene.registry().emplace<ScriptComponent>(scene.createEntity("S")).script = "assets://mover.lua";
        ScriptEngine scripts(vfs);
        FrameTime time;
        time.deltaSeconds = 1.0 / 60.0;
        scripts.update(scene, time);  // instances created
        bench("script: onUpdate of 1000 entities (translate)", 1000, 30, [&] { scripts.update(scene, time); });
        std::printf("    (Lua heap: %zu KB)\n", scripts.stats().memoryKb);
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    // -------------------------------------------------------------- audio, UI, animation
    const auto root = std::filesystem::temp_directory_path() / "agr_bench_p7";
    std::filesystem::create_directories(root / "fonts");
    std::filesystem::copy_file(AGR_BENCH_FONT, root / "fonts" / "Inter-Regular.ttf", std::filesystem::copy_options::overwrite_existing);
    VirtualFileSystem vfs;
    vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
    {
        std::vector<float> tone(48000);
        for (usize i = 0; i < tone.size(); ++i) tone[i] = 0.3f * std::sin(static_cast<float>(i) * 0.05f);
        const ByteBuffer wav = encodeWav16(tone.data(), 48000, 1, 44100);  // resampled to 48 kHz
        (void)vfs.writeBytes("assets://tone.wav", wav.data(), wav.size());
        AudioSystem audio(vfs);
        (void)audio.initialize({.output = AudioOutput::Manual});
        for (int i = 0; i < 32; ++i) {
            PlayParams p;
            p.loop = true;
            p.pitch = 0.8f + 0.01f * static_cast<float>(i);
            p.spatial = i % 2 == 0;
            p.position = {static_cast<float>(i % 8) - 4.0f, 0.0f, -static_cast<float>(i / 8)};
            (void)audio.play("assets://tone.wav", p);
        }
        std::vector<float> out(512 * 2);
        bench("audio: mix 32 voices (16 spatial, resampled), 512 frames", 512, 400, [&] { audio.mix(out.data(), 512); });
        std::printf("    (512 frames = 10.7 ms of sound at 48 kHz)\n");
    }
    {
        registerUIComponents();
        Scene scene;
        const auto canvas = scene.createEntity("Canvas");
        scene.registry().emplace<UICanvasComponent>(canvas);
        const auto column = scene.createEntity("Column", canvas);
        auto& cr = scene.registry().emplace<UIRectComponent>(column);
        cr.anchorMin = {0, 0};
        cr.anchorMax = {1, 1};
        cr.size = {0, 0};
        scene.registry().emplace<UILayoutComponent>(column).type = UILayoutType::Grid;
        for (int i = 0; i < 100; ++i) {
            const auto b = scene.createEntity("Button", column);
            scene.registry().emplace<UIRectComponent>(b);
            scene.registry().emplace<UIImageComponent>(b);
            scene.registry().emplace<UIButtonComponent>(b);
            const auto t = scene.createEntity("Label", b);
            auto& tr = scene.registry().emplace<UIRectComponent>(t);
            tr.anchorMin = {0, 0};
            tr.anchorMax = {1, 1};
            tr.size = {0, 0};
            scene.registry().emplace<UITextComponent>(t).text = "Item " + std::to_string(i);
            for (int k = 0; k < 3; ++k) {
                const auto img = scene.createEntity("Icon", b);
                scene.registry().emplace<UIRectComponent>(img).size = {20, 20};
                scene.registry().emplace<UIImageComponent>(img);
            }
        }
        UISystem ui(vfs);
        OverlayDrawList list;
        ui.buildOverlay(scene, 1920, 1080, list);  // glyphs rasterized once
        bench("ui: layout + overlay of 501 elements (100 buttons with text)", 501, 200, [&] {
            list.clear();
            ui.buildOverlay(scene, 1920, 1080, list);
        });
        std::printf("    (%u quads)\n", ui.stats().quads);
        std::vector<UIPointer> pointers{{0, {500, 300}, true}};
        bench("ui: pointer input (hit test, top element first) over 501 elements", 1, 2000, [&] { ui.processInput(scene, pointers); });
    }
    {
        registerAnimationComponents();
        AnimationClip clip;
        for (int ch = 0; ch < 3; ++ch) {
            Curve& c = clip.track("", "Transform", "position", ch).curve;
            for (int k = 0; k <= 8; ++k) c.setKey(static_cast<float>(k) * 0.25f, static_cast<float>((k * 7 + ch) % 5));
        }
        clip.duration = 2.0f;
        (void)vfs.writeText("assets://move.agranim", clip.toJson());
        AnimatorController ctrl;
        ctrl.parameters = {{"speed", ParameterType::Float, 0.0f}};
        ctrl.states = {{"A", "assets://move.agranim", "", 1.0f, {}}, {"B", "assets://move.agranim", "", 1.5f, {}}};
        ctrl.transitions = {{"A", "B", {{"speed", ConditionOp::Greater, 0.5f}}, -1.0f, 0.2f}, {"B", "A", {}, 1.0f, 0.2f}};
        (void)vfs.writeText("assets://sm.agrctrl", ctrl.toJson());
        Scene scene;
        for (int i = 0; i < 1000; ++i) {
            const auto e = scene.createEntity("A");
            auto& a = scene.registry().emplace<AnimatorComponent>(e);
            if (i % 2) a.controller = "assets://sm.agrctrl";
            else a.clip = "assets://move.agranim";
        }
        AnimationSystem anim(vfs);
        anim.update(scene, 0.016f);
        bench("animation: 1000 animators (500 state machines), 3 curves each", 1000, 100, [&] { anim.update(scene, 0.016f); });
    }
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    return 0;
}
