#define DOCTEST_CONFIG_IMPLEMENT
#include <cstdio>
#include <cstdlib>

#include "GpuFixture.hpp"
#include "agr/core/Log.hpp"

namespace agr::test {
static GpuContext g_context;
GpuContext& gpu() { return g_context; }
} // namespace agr::test

int main(int argc, char** argv) {
    using namespace agr;
    Log::setLevel(LogLevel::Warning);
    auto& ctx = test::gpu();
    const char* api = std::getenv("AGR_TEST_API");
    const std::string apiName = api ? api : (std::getenv("AGR_TEST_GLES") ? "gles" : "gl");
    ctx.api = apiName == "vulkan" ? GraphicsApi::Vulkan : apiName == "gles" ? GraphicsApi::OpenGLES : GraphicsApi::OpenGL;

    EngineConfig cfg;
    cfg.window.width = static_cast<i32>(test::kWidth);
    cfg.window.height = static_cast<i32>(test::kHeight);
    cfg.window.hidden = true;
    cfg.window.highDpi = false;
    cfg.window.resizable = false;
    cfg.window.vsync = false;
    cfg.window.graphics = ctx.api;
    cfg.window.graphicsValidation = true;  // Vulkan: every validation error fails the run
    ctx.platform = std::make_unique<SdlPlatform>();
    Status s = ctx.platform->initialize(cfg);
    if (s && ctx.platform->graphicsApi() != ctx.api)
        s = makeError(Error::Code::Unsupported, std::string("requested graphics API unavailable: ") + enumName(ctx.api));
    if (s) {
        auto* p = ctx.platform.get();
        Result<std::unique_ptr<rhi::Device>> dev = makeError(Error::Code::Unknown, "");
        if (ctx.api == GraphicsApi::Vulkan) {
            rhi::VulkanPlatform vp;
            vp.appName = "agr_gpu_tests";
            vp.instanceExtensions = p->vulkanInstanceExtensions();
            vp.getInstanceProcAddr = p->vulkanGetInstanceProcAddr();
            vp.createSurface = [p](void* instance) { return p->createVulkanSurface(instance); };
            vp.vsync = false;
            vp.validation = true;
            dev = rhi::createVulkanDevice(vp);
        } else {
            dev = rhi::createOpenGLDevice([p](const char* n) { return p->graphicsProcAddress(n); }, ctx.api == GraphicsApi::OpenGLES);
        }
        if (dev) ctx.device = std::move(dev.value());
        else s = dev.error();
    }
    if (!s) {
        std::fprintf(stderr, "GPU tests skipped: %s\n", s.error().message.c_str());
        // CI sets AGR_REQUIRE_GPU_TESTS so that a missing display is a failure there.
        return std::getenv("AGR_REQUIRE_GPU_TESTS") ? 1 : 0;
    }
    const auto fb = ctx.platform->framebufferSize();
    if (static_cast<u32>(fb.x) != test::kWidth || static_cast<u32>(fb.y) != test::kHeight) {
        std::fprintf(stderr, "unexpected framebuffer size %gx%g\n", fb.x, fb.y);
        return 1;
    }
    std::printf("GPU tests on %s: %s\n", rhi::enumName(ctx.device->info().backend), ctx.device->info().renderer.c_str());
    doctest::Context context(argc, argv);
    int result = context.run();
    if (const u64 errors = ctx.device->validationErrorCount(); errors > 0) {
        std::fprintf(stderr, "FAILED: %llu graphics API validation error(s)\n", static_cast<unsigned long long>(errors));
        result = 1;
    }
    ctx.device.reset();
    ctx.platform->shutdown();
    return result;
}
