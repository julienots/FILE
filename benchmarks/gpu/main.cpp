// AGR ENGINE renderer benchmarks (needs a display; hidden 1280x720 window).
// Reports the median over 60 frames of: engine-side CPU time (collect: traversal,
// culling, sorting, packing), submission time (driver), and batches / draw calls.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <vector>

#include "agr/assets/AssetManager.hpp"
#include "agr/core/Log.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/platform/SdlPlatform.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/Renderer.hpp"
#include "agr/render/SpriteSheet.hpp"
#include "agr/twod/Particles2D.hpp"
#include "agr/twod/TilemapEditing.hpp"

using namespace agr;

namespace {

constexpr u32 W = 1280, H = 720;

struct Bench {
    SdlPlatform platform;
    std::unique_ptr<rhi::Device> device;
    VirtualFileSystem vfs;
    JobSystem jobs{2};
    std::unique_ptr<AssetManager> assets;
    std::unique_ptr<SpriteSheetLibrary> sheets;
    std::unique_ptr<EnvironmentLibrary> environments;
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("agr_gpubench_" + Uuid::generate().toString());
};

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

void run(Bench& b, const char* name, const std::function<void(Scene&)>& build,
         const std::function<void(GraphicsProfile&)>& tweak = nullptr) {
    Renderer renderer(*b.device, *b.assets);
    if (!renderer.initialize()) return;
    GraphicsProfile p = graphicsProfile(GraphicsTier::Low);
    p.renderScale = 1.0f;
    p.shadowMapSize = 0;  // 3D cases enable shadows explicitly
    if (tweak) tweak(p);
    renderer.setProfile(p);
    renderer.setSpriteSheets(b.sheets.get());
    renderer.setEnvironments(b.environments.get());
    Scene scene;
    build(scene);
    scene.updateTransforms();
    simulateParticles2D(scene, 0.016f);  // spawns burst particles (no-op without emitters)
    for (int i = 0; i < 5; ++i) {  // warm-up + texture uploads
        b.assets->waitAll();
        b.environments->waitAll();
        b.environments->update();
        renderer.render(scene, W, H);
        b.platform.present();
    }
    std::vector<double> collect, submit, wait, gpuMs;
    for (int i = 0; i < 60; ++i) {
        renderer.render(scene, W, H);
        b.platform.present();
        collect.push_back(renderer.stats().collectMs);
        submit.push_back(renderer.stats().submitMs - renderer.stats().gpuWaitMs);
        wait.push_back(renderer.stats().frameWaitMs);
        gpuMs.push_back(renderer.stats().gpuTimeMs);
    }
    const auto& s = renderer.stats();
    const double gpu = median(gpuMs);
    char gpuText[32];
    if (gpu >= 0.0) std::snprintf(gpuText, sizeof gpuText, "%7.2f ms", gpu);
    else std::snprintf(gpuText, sizeof gpuText, "%10s", "-");
    std::printf("%-40s collect %6.3f ms  submit %7.3f ms  wait %7.3f ms  gpu %s  draws %5u  batches %4u  visible %6u  culled %6u\n",
                name, median(collect), median(submit), median(wait), gpuText, s.drawCalls, s.spriteBatches + s.meshBatches,
                s.meshesVisible + s.spritesVisible, s.meshesCulled + s.spritesCulled);  // sprites include tiles and particles
}

void orthoCamera(Scene& s, float halfHeight) {
    const auto c = s.createEntity("Camera");
    auto& cam = s.registry().emplace<CameraComponent>(c);
    cam.projection = Projection::Orthographic;
    cam.orthographicSize = halfHeight;
    s.registry().get<TransformComponent>(c).position = {0, 0, 10};
}

void perspectiveCamera(Scene& s) {
    const auto c = s.createEntity("Camera");
    s.registry().emplace<CameraComponent>(c);
    auto& t = s.registry().get<TransformComponent>(c);
    t.position = {0, 12, 30};
    t.rotation = math::Quat::fromAxisAngle({1, 0, 0}, math::radians(-25.0f));
    const auto l = s.createEntity("Light");
    s.registry().emplace<DirectionalLightComponent>(l);
    s.registry().get<TransformComponent>(l).rotation = math::Quat::fromEuler({math::radians(-50.0f), 0.6f, 0});
}

void spriteGrid(Scene& s, u32 count, u32 texturesEvery) {
    const u32 side = static_cast<u32>(std::ceil(std::sqrt(static_cast<float>(count))));
    for (u32 i = 0; i < count; ++i) {
        const auto e = s.createEntity("S");
        s.registry().get<TransformComponent>(e).position = {(static_cast<float>(i % side) - side * 0.5f) * 0.1f,
                                                            (static_cast<float>(i / side) - side * 0.5f) * 0.1f, 0};
        auto& sr = s.registry().emplace<SpriteRendererComponent>(e);
        sr.size = {0.09f, 0.09f};
        sr.texture = (i / texturesEvery) % 2 ? "assets://a.png" : "assets://b.png";
    }
}

}  // namespace

int main(int argc, char** argv) {
    Log::setLevel(LogLevel::Warning);
    Bench b;
    // Usage: agr_gpu_benchmarks [gl | gles | vulkan]   (default gl)
    const std::string apiName = argc > 1 ? argv[1] : "gl";
    const GraphicsApi api = apiName == "vulkan" ? GraphicsApi::Vulkan : apiName == "gles" ? GraphicsApi::OpenGLES : GraphicsApi::OpenGL;
    EngineConfig cfg;
    cfg.window.title = "bench";
    cfg.window.width = static_cast<i32>(W);
    cfg.window.height = static_cast<i32>(H);
    cfg.window.resizable = false;
    cfg.window.vsync = false;
    cfg.window.highDpi = false;
    cfg.window.hidden = true;
    cfg.window.graphics = api;
    if (Status st = b.platform.initialize(cfg); !st || b.platform.graphicsApi() != api) {
        std::fprintf(stderr, "cannot create a %s window\n", apiName.c_str());
        return 1;
    }
    Result<std::unique_ptr<rhi::Device>> dev = makeError(Error::Code::Unknown, "");
    if (api == GraphicsApi::Vulkan) {
        rhi::VulkanPlatform vp;
        vp.instanceExtensions = b.platform.vulkanInstanceExtensions();
        vp.getInstanceProcAddr = b.platform.vulkanGetInstanceProcAddr();
        vp.createSurface = [&](void* instance) { return b.platform.createVulkanSurface(instance); };
        vp.vsync = false;
        dev = rhi::createVulkanDevice(vp);
    } else {
        dev = rhi::createOpenGLDevice([&](const char* n) { return b.platform.graphicsProcAddress(n); }, api == GraphicsApi::OpenGLES);
    }
    if (!dev) {
        std::fprintf(stderr, "device creation failed: %s\n", dev.error().message.c_str());
        return 1;
    }
    b.device = std::move(dev.value());
    std::filesystem::create_directories(b.root);
    b.vfs.mount("assets", std::make_unique<NativeFileProvider>(b.root.string(), false));
    for (const char* name : {"a.png", "b.png"}) {
        std::vector<u8> px(16 * 16 * 4, 200);
        auto png = encodePng(16, 16, px);
        (void)b.vfs.writeBytes(std::string("assets://") + name, png.value().data(), png.value().size());
    }
    b.assets = std::make_unique<AssetManager>(b.vfs, b.jobs, b.device.get());
    b.sheets = std::make_unique<SpriteSheetLibrary>(b.vfs);
    b.environments = std::make_unique<EnvironmentLibrary>(b.vfs, b.jobs, b.device.get());
    {
        // 2048 x 1024 HDR sky: gradient + bright sun.
        std::vector<float> sky(2048u * 1024u * 3u);
        for (u32 y = 0; y < 1024; ++y)
            for (u32 x = 0; x < 2048; ++x) {
                const float t = 1.0f - static_cast<float>(y) / 1024.0f;
                const bool sun = (x - 1400u) * (x - 1400u) + (y - 300u) * (y - 300u) < 36u;
                for (u32 c = 0; c < 3; ++c) sky[(y * 2048u + x) * 3u + c] = sun ? 50.0f : 0.2f + 0.6f * t * (c == 2 ? 1.0f : 0.6f);
            }
        auto hdr = encodeHdr(2048, 1024, sky);
        if (hdr) (void)b.vfs.writeBytes("assets://sky.hdr", hdr.value().data(), hdr.value().size());
    }
    (void)b.vfs.writeText("assets://tiles.agrsheet", SpriteSheet::grid("assets://a.png", 16, 16, 8, 8).toJson());
    registerRenderComponents();
    std::printf("AGR ENGINE renderer benchmarks — %s on %s, %ux%u\n", rhi::enumName(b.device->info().backend),
                b.device->info().renderer.c_str(), W, H);

    run(b, "3D: 120 lit cubes", [](Scene& s) {
        perspectiveCamera(s);
        for (int i = 0; i < 120; ++i) {
            const auto e = s.createEntity("C");
            s.registry().get<TransformComponent>(e).position = {static_cast<float>(i % 12 - 6) * 2.0f, 0, static_cast<float>(i / 12) * -2.0f};
            s.registry().emplace<MeshRendererComponent>(e);
        }
    });
    run(b, "3D: 2000 lit cubes (half culled)", [](Scene& s) {
        perspectiveCamera(s);
        for (int i = 0; i < 2000; ++i) {
            const auto e = s.createEntity("C");
            s.registry().get<TransformComponent>(e).position = {static_cast<float>(i % 50 - 25) * 2.0f, 0, static_cast<float>(i / 50) * -2.0f + 20.0f};
            s.registry().emplace<MeshRendererComponent>(e);
        }
    });
    run(b, "3D: 10k cubes (instanced)", [](Scene& s) {
        perspectiveCamera(s);
        for (int i = 0; i < 10000; ++i) {
            const auto e = s.createEntity("C");
            s.registry().get<TransformComponent>(e).position = {static_cast<float>(i % 100 - 50) * 0.6f, 0,
                                                                static_cast<float>(i / 100) * -0.6f + 20.0f};
            s.registry().get<TransformComponent>(e).scale = math::Vec3{0.4f};
            s.registry().emplace<MeshRendererComponent>(e).color = i % 2 ? math::Color{1, 0.6f, 0.1f, 1} : math::Color{0.4f, 0.4f, 0.45f, 1};
        }
    });
    auto shadowScene = [](Scene& s) {
        perspectiveCamera(s);
        const auto g = s.createEntity("Ground");
        s.registry().get<TransformComponent>(g).scale = math::Vec3{80.0f};
        s.registry().emplace<MeshRendererComponent>(g).mesh = PrimitiveMesh::Plane;
        for (int i = 0; i < 2000; ++i) {
            const auto e = s.createEntity("C");
            s.registry().get<TransformComponent>(e).position = {static_cast<float>(i % 50 - 25) * 2.0f, 0.5f, static_cast<float>(i / 50) * -2.0f + 20.0f};
            s.registry().emplace<MeshRendererComponent>(e);
        }
    };
    run(b, "3D: 2000 cubes + ground, no shadows", shadowScene);
    run(b, "3D: same, shadows 2048 + PCF", shadowScene, [](GraphicsProfile& p) {
        p.shadowMapSize = 2048;
        p.softShadows = true;
    });
    run(b, "3D: same, 3 cascades x 2048 + PCF", shadowScene, [](GraphicsProfile& p) {
        p.shadowMapSize = 2048;
        p.shadowCascades = 3;
        p.softShadows = true;
    });
    auto litCubes = [](Scene& s) {
        perspectiveCamera(s);
        for (int i = 0; i < 120; ++i) {
            const auto e = s.createEntity("C");
            s.registry().get<TransformComponent>(e).position = {static_cast<float>(i % 12 - 6) * 2.0f, 0, static_cast<float>(i / 12) * -2.0f};
            s.registry().emplace<MeshRendererComponent>(e);
        }
    };
    run(b, "3D: 120 cubes, post (HDR, bloom, FXAA)", [&](Scene& s) {
        litCubes(s);
        s.registry().emplace<PostProcessComponent>(s.findByName("Camera"));
    }, [](GraphicsProfile& p) {
        p.bloom = true;
        p.fxaa = true;
    });
    run(b, "3D: 120 cubes, image sky + IBL", [&](Scene& s) {
        litCubes(s);
        auto& env = s.registry().emplace<EnvironmentComponent>(s.createEntity("Env"));
        env.sky = SkyMode::Image;
        env.skyTexture = "assets://sky.hdr";
    });
    run(b, "3D: 120 cubes, 8 point lights", [](Scene& s) {
        perspectiveCamera(s);
        for (int i = 0; i < 120; ++i) {
            const auto e = s.createEntity("C");
            s.registry().get<TransformComponent>(e).position = {static_cast<float>(i % 12 - 6) * 2.0f, 0, static_cast<float>(i / 12) * -2.0f};
            s.registry().emplace<MeshRendererComponent>(e);
        }
        for (int i = 0; i < 8; ++i) {
            const auto l = s.createEntity("L");
            s.registry().get<TransformComponent>(l).position = {static_cast<float>(i % 4) * 6.0f - 9.0f, 2.0f, static_cast<float>(i / 4) * -8.0f};
            s.registry().emplace<PointLightComponent>(l).range = 8.0f;
        }
    }, [](GraphicsProfile& p) { p.maxLights = 8; });
    run(b, "2D: 10k sprites, 1 texture", [](Scene& s) { orthoCamera(s, 6); spriteGrid(s, 10000, 1u << 30); });
    run(b, "2D: 10k sprites, 2 textures interleaved", [](Scene& s) { orthoCamera(s, 6); spriteGrid(s, 10000, 64); });
    run(b, "2D: 50k sprites, 2 textures interleaved", [](Scene& s) { orthoCamera(s, 12); spriteGrid(s, 50000, 64); });
    run(b, "2D: 50k sprites, 75% off-screen", [](Scene& s) { orthoCamera(s, 6); spriteGrid(s, 50000, 64); });
    run(b, "2D: tilemap 1000x1000, ~16k cells shown", [](Scene& s) {
        orthoCamera(s, 48);
        const auto e = s.createEntity("Map");
        s.registry().get<TransformComponent>(e).position = {-500, -500, 0};
        auto& tm = s.registry().emplace<TilemapComponent>(e);
        tm.tileset = "assets://tiles.agrsheet";
        tm.width = tm.height = 1000;
        tilemapEnsureLayout(tm);
        for (usize i = 0; i < tm.tiles.size(); ++i) tm.tiles[i] = static_cast<i32>(i % 4);
    });
    run(b, "2D: 10k particles, additive", [](Scene& s) {
        orthoCamera(s, 6);
        const auto e = s.createEntity("Emitter");
        auto& em = s.registry().emplace<ParticleEmitter2DComponent>(e);
        em.rate = 0.0f;
        em.burst = em.maxParticles = 10000;
        em.lifetimeMin = em.lifetimeMax = 1000.0f;
        em.spawnRadius = 5.0f;
        em.speedMin = em.speedMax = 0.0f;
        em.startSize = em.endSize = 0.2f;
        em.additive = true;
        em.seed = 7;
    });
    run(b, "2D: 10k lit sprites, 16 lights", [](Scene& s) {
        orthoCamera(s, 6);
        spriteGrid(s, 10000, 1u << 30);
        s.registry().view<SpriteRendererComponent>().each([](ecs::Entity, SpriteRendererComponent& sr) { sr.lit = true; });
        for (int i = 0; i < 16; ++i) {
            const auto l = s.createEntity("L");
            s.registry().get<TransformComponent>(l).position = {static_cast<float>(i % 4) * 3.0f - 4.5f, static_cast<float>(i / 4) * 3.0f - 4.5f, 0};
            s.registry().emplace<Light2DComponent>(l).radius = 3.0f;
        }
    });

    b.environments.reset();
    b.sheets.reset();
    b.assets.reset();
    b.device.reset();
    b.platform.shutdown();
    std::filesystem::remove_all(b.root);
    return 0;
}
