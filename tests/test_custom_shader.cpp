// .agrshader parsing: parameters, entry points, errors with line numbers, stage splitting,
// compiler log parsing; custom shader references in .agrmat files.
#include <doctest/doctest.h>

#include <algorithm>

#include "agr/assets/CustomShader.hpp"
#include "agr/assets/Material.hpp"

using namespace agr;

TEST_SUITE("assets.custom_shader") {

TEST_CASE("parameters: types, defaults, ranges, textures, slots") {
    const CustomShader s = parseCustomShader(
        "// comment\n"
        "param color tint = (1.0, 0.5, 0.25);   // alpha defaults to 1\n"
        "param float speed = 2 range(0, 10);\n"
        "param vec2 scroll = vec2(0.5, -1.0);\n"
        "  param vec3 axis = 1.0;\n"
        "param vec4 raw;\n"
        "param texture noise = \"assets://fx/noise.png\";\n"
        "param texture albedo srgb;\n"
        "void surface(inout Surface s) { s.baseColor *= tint.rgb; }\n");
    REQUIRE_MESSAGE(s.valid(), (s.errors.empty() ? std::string() : s.errors[0].message));
    REQUIRE(s.params.size() == 7);
    CHECK(s.hasSurface);
    CHECK_FALSE(s.hasVertex);
    CHECK(s.vectorCount == 5);
    CHECK(s.textureCount == 2);
    const ShaderParam* tint = s.find("tint");
    REQUIRE(tint);
    CHECK(tint->type == ShaderParamType::Color);
    CHECK(tint->defaultValue == math::Vec4(1.0f, 0.5f, 0.25f, 1.0f));
    CHECK(tint->line == 2);
    CHECK(tint->slot == 0);
    const ShaderParam* speed = s.find("speed");
    CHECK(speed->rangeMin == 0.0f);
    CHECK(speed->rangeMax == 10.0f);
    CHECK(speed->defaultValue.x == 2.0f);
    CHECK(s.find("scroll")->defaultValue == math::Vec4(0.5f, -1.0f, 0.0f, 0.0f));
    CHECK(s.find("axis")->defaultValue == math::Vec4(1.0f, 1.0f, 1.0f, 1.0f));  // broadcast
    CHECK(s.find("noise")->defaultTexture == "assets://fx/noise.png");             // "//" inside quotes kept
    CHECK_FALSE(s.find("noise")->srgb);
    CHECK(s.find("albedo")->srgb);
    CHECK(s.find("albedo")->slot == 1);
    // Param lines are blanked: the GLSL keeps the file's line numbers.
    CHECK(s.code.find("param") == std::string::npos);
    CHECK(std::count(s.code.begin(), s.code.end(), '\n') == 9);
    const std::string defines = shaderParamDefines(s, true);
    CHECK(defines.find("#define speed (u_custom[1].x)") != std::string::npos);
    CHECK(defines.find("#define scroll (u_custom[2].xy)") != std::string::npos);
    CHECK(defines.find("#define albedo u_customMap1") != std::string::npos);
    CHECK(shaderParamDefines(s, false).find("u_customMap") == std::string::npos);
}

TEST_CASE("errors carry the line of the problem") {
    auto firstError = [](std::string_view src) {
        const CustomShader s = parseCustomShader(src);
        return s.errors.empty() ? ShaderDiagnostic{~0u, ""} : s.errors[0];
    };
    CHECK(firstError("param float x = (1, 2);\nvoid surface(inout Surface s) {}\n").line == 1);
    CHECK(firstError("void surface(inout Surface s) {}\nparam matrix m;\n").line == 2);
    CHECK(firstError("void surface(inout Surface s) {}\n\nparam float texture;\n").line == 3);  // would break texture()
    CHECK(firstError("void surface(inout Surface s) {}\nparam float u_time;\n").line == 2);
    CHECK(firstError("param float a;\nparam vec2 a;\nvoid surface(inout Surface s) {}\n").line == 2);
    CHECK(firstError("param float a range(1, 0);\nvoid surface(inout Surface s) {}\n").line == 1);
    CHECK(firstError("param vec2 a range(0, 1);\nvoid surface(inout Surface s) {}\n").line == 1);
    CHECK(firstError("param float a = 1 extra;\nvoid surface(inout Surface s) {}\n").line == 1);
    CHECK(firstError("void surface(inout Surface s) {}\nuniform float hack;\n").line == 2);
    CHECK(firstError("void surface(inout Surface s) {}\n\nout vec4 color;\n").line == 3);
    // No entry point (a prototype or a commented-out function does not count).
    CHECK(firstError("void surface(inout Surface s);\n// void surface(inout Surface s) {}\n").line == 0);
    std::string many;
    for (int i = 0; i < 9; ++i) many += "param float p" + std::to_string(i) + ";\n";
    CHECK(firstError(many + "void surface(inout Surface s) {}\n").line == 9);
    // Valid: helpers, nested braces, a local variable named like a keyword prefix.
    const CustomShader ok = parseCustomShader(
        "float wave(float x) { if (x > 0.0) { return sin(x); } return 0.0; }\n"
        "void vertex(inout Vertex v) { float input_ = wave(v.position.x); v.position.y += input_; }\n");
    CHECK(ok.valid());
    CHECK(ok.hasVertex);
    // Numbers: sign, exponent, "f" suffix; no "inf" / "nan".
    const CustomShader nums = parseCustomShader("param vec4 n = (2.5e1, -0.5, .25, 1.0f);\nvoid surface(inout Surface s) {}\n");
    REQUIRE(nums.valid());
    CHECK(nums.params[0].defaultValue == math::Vec4(25.0f, -0.5f, 0.25f, 1.0f));
    CHECK(firstError("param float n = inf;\nvoid surface(inout Surface s) {}\n").line == 1);
    CHECK_FALSE(ok.hasSurface);
}

TEST_CASE("each stage is compiled without the other stage's entry point") {
    const std::string code =
        "float helper() { return 1.0; }\n"
        "void vertex(inout Vertex v) {\n"
        "    v.position.y += helper();\n"
        "}\n"
        "/* void surface(inout Surface s) { } */\n"
        "void surface(inout Surface s) {\n"
        "    if (s.alpha < 0.5) { discard; }\n"
        "}\n";
    const std::string vertexOnly = blankFunction(code, "surface");
    CHECK(vertexOnly.find("discard") == std::string::npos);
    CHECK(vertexOnly.find("v.position.y") != std::string::npos);
    CHECK(vertexOnly.find("/* void surface") != std::string::npos);  // the comment is not the function
    CHECK(std::count(vertexOnly.begin(), vertexOnly.end(), '\n') == std::count(code.begin(), code.end(), '\n'));
    const std::string fragmentOnly = blankFunction(code, "vertex");
    CHECK(fragmentOnly.find("v.position.y") == std::string::npos);
    CHECK(fragmentOnly.find("discard") != std::string::npos);
    CHECK(fragmentOnly.find("helper()") != std::string::npos);  // helpers stay in both stages
    CHECK(blankFunction(code, "missing") == code);
}

TEST_CASE("compiler logs: user lines from Mesa, glslang and NVIDIA formats") {
    const auto mesa = parseShaderLog("1:3(19): error: `notDeclared' undeclared\n0:120(4): error: something internal\n");
    REQUIRE(mesa.size() == 2);
    CHECK(mesa[0].line == 3);
    CHECK(mesa[0].message == "`notDeclared' undeclared");
    CHECK(mesa[1].line == 0);
    const auto glslang = parseShaderLog(
        "pipeline 'agr.custom': fragment shader:\nERROR: 1:7: 'foo' : undeclared identifier\nERROR: 1 compilation errors.  No code generated.\n");
    REQUIRE(glslang.size() == 1);
    CHECK(glslang[0].line == 7);
    CHECK(glslang[0].message.find("undeclared identifier") != std::string::npos);
    const auto nvidia = parseShaderLog("1(12) : error C1008: undefined variable \"x\"\n");
    REQUIRE(nvidia.size() == 1);
    CHECK(nvidia[0].line == 12);
    const auto duplicate = parseShaderLog("1:3(1): error: a\n1:3(1): error: a\n");
    CHECK(duplicate.size() == 1);
}

TEST_CASE("the default shader is valid and uses both hooks") {
    const CustomShader s = parseCustomShader(defaultCustomShaderSource());
    CHECK(s.valid());
    CHECK(s.hasSurface);
    CHECK(s.hasVertex);
    CHECK(s.params.size() == 3);
}

TEST_CASE("materials reference a custom shader and override its parameters") {
    MaterialData m;
    m.shader = "assets://fx/glow.agrshader";
    m.shaderParams["tint"] = {0.1f, 0.2f, 0.3f, 1.0f};
    m.shaderParams["speed"] = {4.0f, 0.0f, 0.0f, 0.0f};
    m.shaderTextures["noise"] = "assets://fx/noise.png";
    auto back = parseMaterial(materialToJson(m));
    REQUIRE(back);
    CHECK(back.value() == m);
    // Materials without a shader keep the old file format.
    CHECK(materialToJson(MaterialData{}).find("shader") == std::string::npos);
    auto old = parseMaterial(R"({"format":"agr.material","version":1,"metallic":0.5})");
    REQUIRE(old);
    CHECK(old.value().shader.empty());
    CHECK(old.value().shaderParams.empty());
}

}
