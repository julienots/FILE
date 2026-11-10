#include <doctest/doctest.h>

#include "agr/rhi/ShaderDialect.hpp"

using namespace agr;
using namespace agr::rhi;

TEST_SUITE("rhi.dialect") {

TEST_CASE("uniform blocks, samplers and varyings get explicit Vulkan bindings") {
    PipelineDesc d;
    d.uniformBlocks = {"FrameData", "ObjectData"};
    d.samplers = {"u_albedo", "u_normal"};
    const std::string vs = R"(layout(std140) uniform ObjectData { mat4 u_model; };
layout(location = 0) in vec3 a_position;
out mat4 v_matrix;
flat out int v_id;
out vec2 v_uv;
void main() {})";
    const std::string fs = R"(uniform sampler2D u_normal;
in vec2 v_uv;
flat in int v_id;
layout(location = 0) out vec4 o_color;
void main() {})";
    std::unordered_map<std::string, u32> varyings;
    auto v = translateForVulkan(vs, ShaderStage::Vertex, d, varyings);
    REQUIRE(v);
    CHECK(v.value().rfind("#version 450\n", 0) == 0);
    CHECK(v.value().find("layout(std140, set = 0, binding = 1) uniform ObjectData {") != std::string::npos);
    CHECK(v.value().find("layout(location = 0) in vec3 a_position;") != std::string::npos);  // untouched
    CHECK(v.value().find("layout(location = 0) out mat4 v_matrix;") != std::string::npos);
    CHECK(v.value().find("layout(location = 4) flat out int v_id;") != std::string::npos);    // mat4 uses 4 slots
    CHECK(v.value().find("layout(location = 5) out vec2 v_uv;") != std::string::npos);
    auto f = translateForVulkan(fs, ShaderStage::Fragment, d, varyings);
    REQUIRE(f);
    CHECK(f.value().find("layout(set = 0, binding = 9) uniform sampler2D u_normal;") != std::string::npos);
    CHECK(f.value().find("layout(location = 5) in vec2 v_uv;") != std::string::npos);  // matched by name
    CHECK(f.value().find("layout(location = 4) flat in int v_id;") != std::string::npos);
    CHECK(f.value().find("layout(location = 0) out vec4 o_color;") != std::string::npos);
}

TEST_CASE("dialect errors name the offending symbol") {
    PipelineDesc d;
    std::unordered_map<std::string, u32> varyings;
    auto block = translateForVulkan("layout(std140) uniform Missing { vec4 x; };", ShaderStage::Vertex, d, varyings);
    REQUIRE_FALSE(block);
    CHECK(block.error().message.find("'Missing'") != std::string::npos);
    auto sampler = translateForVulkan("uniform sampler2D u_tex;", ShaderStage::Fragment, d, varyings);
    REQUIRE_FALSE(sampler);
    CHECK(sampler.error().message.find("'u_tex'") != std::string::npos);
    auto loose = translateForVulkan("uniform vec4 u_color;", ShaderStage::Fragment, d, varyings);
    REQUIRE_FALSE(loose);
    CHECK(loose.error().message.find("uniform block") != std::string::npos);
    auto orphan = translateForVulkan("in vec2 v_unknown;", ShaderStage::Fragment, d, varyings);
    REQUIRE_FALSE(orphan);
    CHECK(orphan.error().message.find("'v_unknown'") != std::string::npos);
}

}  // TEST_SUITE
