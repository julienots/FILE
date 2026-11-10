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
    ctx.gles = std::getenv("AGR_TEST_GLES") != nullptr;

    EngineConfig cfg;
    cfg.window.width = static_cast<i32>(test::kWidth);
    cfg.window.height = static_cast<i32>(test::kHeight);
    cfg.window.hidden = true;
    cfg.window.highDpi = false;
    cfg.window.resizable = false;
    cfg.window.vsync = false;
    cfg.window.graphics = ctx.gles ? GraphicsApi::OpenGLES : GraphicsApi::OpenGL;
    ctx.platform = std::make_unique<SdlPlatform>();
    Status s = ctx.platform->initialize(cfg);
    if (s) {
        auto* p = ctx.platform.get();
        auto dev = rhi::createOpenGLDevice([p](const char* n) { return p->graphicsProcAddress(n); }, ctx.gles);
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
    const int result = context.run();
    ctx.device.reset();
    ctx.platform->shutdown();
    return result;
}
