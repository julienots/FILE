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

using namespace agr;

namespace {

constexpr u32 W = 1280, H = 720;

struct Bench {
    SdlPlatform platform;
    std::unique_ptr<rhi::Device> device;
    VirtualFileSystem vfs;
    JobSystem jobs{2};
    std::unique_ptr<AssetManager> assets;
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("agr_gpubench_" + Uuid::generate().toString());
};

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

void run(Bench& b, const char* name, const std::function<void(Scene&)>& build) {
    Renderer renderer(*b.device, *b.assets);
    if (!renderer.initialize()) return;
    GraphicsProfile p = graphicsProfile(GraphicsTier::Low);
    p.renderScale = 1.0f;
    renderer.setProfile(p);
    Scene scene;
    build(scene);
    scene.updateTransforms();
    for (int i = 0; i < 5; ++i) {  // warm-up + texture uploads
        b.assets->waitAll();
        renderer.render(scene, W, H);
        b.platform.present();
    }
    std::vector<double> collect, submit, wait;
    for (int i = 0; i < 60; ++i) {
        renderer.render(scene, W, H);
        b.platform.present();
        collect.push_back(renderer.stats().collectMs);
        submit.push_back(renderer.stats().submitMs - renderer.stats().gpuWaitMs);
        wait.push_back(renderer.stats().gpuWaitMs);
    }
    const auto& s = renderer.stats();
    std::printf("%-40s collect %6.3f ms  submit %7.3f ms  wait %7.3f ms  draws %5u  batches %4u  visible %6u  culled %6u\n",
                name, median(collect), median(submit), median(wait), s.drawCalls, s.spriteBatches,
                s.meshesVisible + s.spritesVisible, s.meshesCulled + s.spritesCulled);
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
    run(b, "2D: 10k sprites, 1 texture", [](Scene& s) { orthoCamera(s, 6); spriteGrid(s, 10000, 1u << 30); });
    run(b, "2D: 10k sprites, 2 textures interleaved", [](Scene& s) { orthoCamera(s, 6); spriteGrid(s, 10000, 64); });
    run(b, "2D: 50k sprites, 2 textures interleaved", [](Scene& s) { orthoCamera(s, 12); spriteGrid(s, 50000, 64); });
    run(b, "2D: 50k sprites, 75% off-screen", [](Scene& s) { orthoCamera(s, 6); spriteGrid(s, 50000, 64); });

    b.assets.reset();
    b.device.reset();
    b.platform.shutdown();
    std::filesystem::remove_all(b.root);
    return 0;
}
