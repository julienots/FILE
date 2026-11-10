#include <cstring>

#include "GpuFixture.hpp"

using namespace agr;
using namespace agr::test;

namespace {

// Full-screen triangle helpers (AGR GLSL dialect).
const char* kPosColorVS = R"(
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
void main() { v_uv = a_uv; gl_Position = vec4(a_pos, 0.0, 1.0); }
)";

struct V2 {
    float x, y, u, v;
};

rhi::VertexLayout layoutV2() {
    rhi::VertexLayout l;
    l.stride = sizeof(V2);
    l.attributes = {{0, rhi::VertexFormat::Float2, 0}, {1, rhi::VertexFormat::Float2, 8}};
    return l;
}

rhi::PipelineHandle makePipeline(rhi::Device& d, const char* fs, std::vector<std::string> blocks = {},
                                 std::vector<std::string> samplers = {}) {
    rhi::PipelineDesc p;
    p.shader = {kPosColorVS, fs};
    p.layout = layoutV2();
    p.cull = rhi::CullMode::None;
    p.depthTest = false;
    p.uniformBlocks = std::move(blocks);
    p.samplers = std::move(samplers);
    p.debugName = "test";
    auto r = d.createPipeline(p);
    REQUIRE_MESSAGE(r, (r ? "" : r.error().message));
    return r.value();
}

// Quad covering the screen; uv (0,0) at the top-left corner of the screen.
const V2 kQuad[6] = {{-1, -1, 0, 1}, {1, -1, 1, 1}, {1, 1, 1, 0}, {-1, -1, 0, 1}, {1, 1, 1, 0}, {-1, 1, 0, 0}};

}  // namespace

TEST_SUITE("gpu.rhi") {

TEST_CASE("device reports sane capabilities") {
    const auto& info = gpu().device->info();
    CHECK(info.backend == (gpu().gles ? rhi::Backend::OpenGLES : rhi::Backend::OpenGL));
    CHECK(info.maxTextureSize >= 2048);
    CHECK(info.uniformBufferAlignment >= 1);
    CHECK_FALSE(info.renderer.empty());
}

TEST_CASE("clear and read back") {
    auto& d = *gpu().device;
    d.beginFrame(kWidth, kHeight);
    d.beginPass({}, {true, {1.0f, 0.5f, 0.0f, 1.0f}, true, 1.0f});
    d.endPass();
    d.endFrame();
    const Rgba p = pixel(d, 10, 10);
    CHECK_MESSAGE(near(p, 255, 128, 0), p);
    std::vector<u8> out;
    CHECK_FALSE(d.readBackbuffer(60, 60, 10, 10, out));  // out of bounds
}

TEST_CASE("shader errors are reported with the compiler log") {
    rhi::PipelineDesc p;
    p.shader = {kPosColorVS, "this is not glsl"};
    p.layout = layoutV2();
    p.debugName = "broken";
    auto r = gpu().device->createPipeline(p);
    REQUIRE_FALSE(r);
    CHECK(r.error().message.find("fragment shader of 'broken' failed to compile") != std::string::npos);
}

TEST_CASE("draw, textures keep top-left orientation") {
    auto& d = *gpu().device;
    // 2x2 texture: TL red, TR green, BL blue, BR white (top row first).
    const u8 px[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    rhi::TextureDesc td;
    td.width = td.height = 2;
    td.format = rhi::TextureFormat::RGBA8;
    td.generateMips = false;
    td.sampler = {rhi::Filter::Nearest, rhi::Filter::Nearest, false, rhi::Wrap::Clamp, rhi::Wrap::Clamp};
    td.data = px;
    auto tex = d.createTexture(td);
    REQUIRE(tex);
    auto vb = d.createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Static, sizeof kQuad, kQuad, "quad"});
    REQUIRE(vb);
    const auto pipe = makePipeline(d, R"(
uniform sampler2D u_tex;
in vec2 v_uv;
layout(location = 0) out vec4 o_color;
void main() { o_color = texture(u_tex, v_uv); }
)", {}, {"u_tex"});

    d.beginFrame(kWidth, kHeight);
    d.beginPass({}, {true, {0, 0, 0, 1}, true, 1.0f});
    d.bindPipeline(pipe);
    d.bindVertexBuffer(vb.value());
    d.bindTexture(0, tex.value());
    d.draw(6);
    d.endPass();
    d.endFrame();
    CHECK(d.frameStats().drawCalls == 1);
    CHECK(d.frameStats().triangles == 2);
    CHECK_MESSAGE(near(pixel(d, 8, 8), 255, 0, 0), pixel(d, 8, 8));      // top-left
    CHECK_MESSAGE(near(pixel(d, 56, 8), 0, 255, 0), pixel(d, 56, 8));    // top-right
    CHECK_MESSAGE(near(pixel(d, 8, 56), 0, 0, 255), pixel(d, 8, 56));    // bottom-left
    CHECK_MESSAGE(near(pixel(d, 56, 56), 255, 255, 255), pixel(d, 56, 56));

    d.destroyPipeline(pipe);
    d.destroyBuffer(vb.value());
    d.destroyTexture(tex.value());
}

TEST_CASE("uniform buffer ranges select per-draw data") {
    auto& d = *gpu().device;
    const u32 align = d.info().uniformBufferAlignment;
    std::vector<u8> data(align * 2, 0);
    const float red[4] = {1, 0, 0, 1}, blue[4] = {0, 0, 1, 1};
    std::memcpy(data.data(), red, 16);
    std::memcpy(data.data() + align, blue, 16);
    auto ub = d.createBuffer({rhi::BufferUsage::Uniform, rhi::BufferMemory::Dynamic, data.size(), nullptr, "ubo"});
    REQUIRE(ub);
    REQUIRE(d.updateBuffer(ub.value(), 0, data.data(), data.size()));
    // Left half and right half quads.
    const V2 halves[12] = {{-1, -1, 0, 0}, {0, -1, 0, 0}, {0, 1, 0, 0},  {-1, -1, 0, 0}, {0, 1, 0, 0}, {-1, 1, 0, 0},
                           {0, -1, 0, 0},  {1, -1, 0, 0}, {1, 1, 0, 0},  {0, -1, 0, 0},  {1, 1, 0, 0}, {0, 1, 0, 0}};
    auto vb = d.createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Static, sizeof halves, halves, "halves"});
    REQUIRE(vb);
    const auto pipe = makePipeline(d, R"(
layout(std140) uniform Tint { vec4 u_tint; };
layout(location = 0) out vec4 o_color;
void main() { o_color = u_tint; }
)", {"Tint"});
    d.beginFrame(kWidth, kHeight);
    d.beginPass({}, {true, {0, 0, 0, 1}, true, 1.0f});
    d.bindPipeline(pipe);
    d.bindVertexBuffer(vb.value());
    d.bindUniformBuffer(0, ub.value(), 0, 16);
    d.draw(6, 0);
    d.bindUniformBuffer(0, ub.value(), align, 16);
    d.draw(6, 6);
    d.endPass();
    d.endFrame();
    CHECK(near(pixel(d, 10, 32), 255, 0, 0));
    CHECK(near(pixel(d, 54, 32), 0, 0, 255));
    d.destroyPipeline(pipe);
    d.destroyBuffer(vb.value());
    d.destroyBuffer(ub.value());
}

TEST_CASE("indexed draws with base vertex (emulated on GLES 3.0)") {
    auto& d = *gpu().device;
    // Two quads in one buffer; the second one is drawn through baseVertex = 4.
    const V2 verts[8] = {{-1, -1, 0, 0}, {0, -1, 0, 0}, {0, 1, 0, 0}, {-1, 1, 0, 0},
                         {0, -1, 0, 0},  {1, -1, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}};
    const u16 idx[6] = {0, 1, 2, 0, 2, 3};
    auto vb = d.createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Static, sizeof verts, verts, "v"});
    auto ib = d.createBuffer({rhi::BufferUsage::Index, rhi::BufferMemory::Static, sizeof idx, idx, "i"});
    REQUIRE(vb);
    REQUIRE(ib);
    const auto pipe = makePipeline(d, R"(
layout(location = 0) out vec4 o_color;
void main() { o_color = vec4(0.0, 1.0, 0.0, 1.0); }
)");
    d.beginFrame(kWidth, kHeight);
    d.beginPass({}, {true, {0, 0, 0, 1}, true, 1.0f});
    d.bindPipeline(pipe);
    d.bindVertexBuffer(vb.value());
    d.bindIndexBuffer(ib.value(), rhi::IndexType::U16);
    d.drawIndexed(6, 0, 4);  // right quad only
    d.endPass();
    d.endFrame();
    CHECK(near(pixel(d, 10, 32), 0, 0, 0));
    CHECK(near(pixel(d, 54, 32), 0, 255, 0));
    d.destroyPipeline(pipe);
    d.destroyBuffer(vb.value());
    d.destroyBuffer(ib.value());
}

TEST_CASE("render targets, MSAA resolve and scaled blit") {
    auto& d = *gpu().device;
    for (u32 samples : {1u, 4u}) {
        CAPTURE(samples);
        auto rt = d.createRenderTarget({32, 32, rhi::TextureFormat::RGBA8, true, samples, "rt"});
        REQUIRE_MESSAGE(rt, (rt ? "" : rt.error().message));
        CHECK(static_cast<bool>(d.renderTargetTexture(rt.value())) == (samples == 1 || d.info().maxSamples == 1));
        d.beginFrame(kWidth, kHeight);
        d.beginPass({}, {true, {0, 0, 0, 1}, true, 1.0f});
        d.endPass();
        d.beginPass(rt.value(), {true, {0.0f, 0.0f, 1.0f, 1.0f}, true, 1.0f});
        d.endPass();
        if (samples > 1) {
            auto resolve = d.createRenderTarget({32, 32, rhi::TextureFormat::RGBA8, false, 1, "resolve"});
            REQUIRE(resolve);
            d.blit(rt.value(), resolve.value(), rhi::Filter::Nearest);
            d.blit(resolve.value(), {}, rhi::Filter::Linear);
            d.destroyRenderTarget(resolve.value());
        } else {
            d.blit(rt.value(), {}, rhi::Filter::Linear);  // 32x32 -> 64x64 upscale
        }
        d.endFrame();
        CHECK(near(pixel(d, 5, 5), 0, 0, 255));
        CHECK(near(pixel(d, 60, 60), 0, 0, 255));
        d.destroyRenderTarget(rt.value());
    }
    CHECK(d.memoryStats().renderTargets == 0);
}

TEST_CASE("stale handles are rejected") {
    auto& d = *gpu().device;
    auto b = d.createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Dynamic, 64, nullptr, "tmp"});
    REQUIRE(b);
    const u8 bytes[16] = {};
    CHECK(d.updateBuffer(b.value(), 0, bytes, 16));
    CHECK_FALSE(d.updateBuffer(b.value(), 60, bytes, 16));  // out of range
    d.destroyBuffer(b.value());
    CHECK_FALSE(d.updateBuffer(b.value(), 0, bytes, 16));   // destroyed
    CHECK_FALSE(d.createTexture({0, 0}));
}

}  // TEST_SUITE
