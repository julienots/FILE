#include <doctest/doctest.h>

#include "agr/assets/CustomShader.hpp"
#include "agr/assets/ShaderGraph.hpp"

using namespace agr;

TEST_SUITE("shader graph") {
TEST_CASE("node library: every node has a title, ports and a target") {
    CHECK(shaderNodeDefs().size() >= 40);
    for (const ShaderNodeDef& d : shaderNodeDefs()) {
        CHECK_FALSE(d.title.empty());
        CHECK(d.targets != 0);
        // Every node compiles alone in a graph where it can live (inputs at their defaults).
        for (ShaderGraph::Target t : {ShaderGraph::Target::Surface, ShaderGraph::Target::Sprite}) {
            if (!(d.targets & (t == ShaderGraph::Target::Sprite ? 2 : 1)) || d.outputs.empty()) continue;
            ShaderGraph g = ShaderGraph::create(t);
            const int out = g.nodes[0].id;
            const int n = g.addNode(d.type, 0, 0);
            if (d.type == "SampleTexture" && t == ShaderGraph::Target::Surface) {
                const int tex = g.addNode("ParamTexture", 0, 0);
                REQUIRE(g.connect(tex, "tex", n, "tex"));
            }
            std::string port;
            for (const ShaderPortDef& o : d.outputs)
                if (o.type != ShaderPortType::Sampler) port = port.empty() ? o.id : port;
            if (port.empty()) continue;  // texture-only outputs
            REQUIRE(g.connect(n, port, out, t == ShaderGraph::Target::Sprite ? "color" : "emissive"));
            const auto r = compileShaderGraph(g);
            INFO(d.type, " ", (r.errors.empty() ? "" : r.errors[0]));
            CHECK(r.ok());
            const CustomShader parsed = parseCustomShader(r.source);
            INFO(r.source);
            CHECK(parsed.valid());
            CHECK((t == ShaderGraph::Target::Sprite ? parsed.hasSprite : parsed.hasSurface));
        }
    }
}

TEST_CASE("compile: types are converted, parameters declared, vertex and surface separated") {
    ShaderGraph g = ShaderGraph::create(ShaderGraph::Target::Surface);
    const int out = g.nodes[0].id;
    const int tint = g.addNode("ParamColor", 0, 0);
    g.node(tint)->values["name"] = "tint";
    g.node(tint)->values["value"] = "1,0.5,0,1";
    const int noise = g.addNode("Noise", 0, 0);
    const int mul = g.addNode("Multiply", 0, 0);
    REQUIRE(g.connect(tint, "rgb", mul, "a"));     // vec3
    REQUIRE(g.connect(noise, "v", mul, "b"));      // float: broadcast
    REQUIRE(g.connect(mul, "out", out, "baseColor"));
    const int fres = g.addNode("Fresnel", 0, 0);
    REQUIRE(g.connect(fres, "out", out, "emissive"));  // float -> vec3
    const int vo = g.addNode("VertexOffset", 0, 0);
    const int time = g.addNode("Time", 0, 0);
    const int sine = g.addNode("Sin", 0, 0);
    const int nrm = g.addNode("Normal", 0, 0);
    const int scale = g.addNode("Multiply", 0, 0);
    REQUIRE(g.connect(time, "t", sine, "x"));
    REQUIRE(g.connect(nrm, "n", scale, "a"));
    REQUIRE(g.connect(sine, "out", scale, "b"));
    REQUIRE(g.connect(scale, "out", vo, "offset"));
    const auto r = compileShaderGraph(g);
    REQUIRE_MESSAGE(r.ok(), (r.errors.empty() ? "" : r.errors[0]));
    INFO(r.source);
    CHECK(r.source.find("param color tint = (1.0, 0.5, 0.0, 1.0);") != std::string::npos);
    CHECK(r.source.find("sgNoise") != std::string::npos);
    CHECK(r.source.find("vec3(n_" + std::to_string(fres) + "_out)") != std::string::npos);  // float -> vec3
    CHECK(r.source.find("v.position += ") != std::string::npos);
    CHECK(r.source.find("v.normal") != std::string::npos);  // the vertex stage reads v., not s.
    const CustomShader parsed = parseCustomShader(r.source);
    CHECK(parsed.valid());
    CHECK(parsed.hasSurface);
    CHECK(parsed.hasVertex);
    REQUIRE(parsed.find("tint"));
    // Only linked outputs are written: the material keeps its roughness.
    CHECK(r.source.find("s.roughness") == std::string::npos);

    // JSON round trip keeps everything.
    auto back = ShaderGraph::fromJson(g.toJson());
    REQUIRE(back);
    CHECK(back.value().nodes.size() == g.nodes.size());
    CHECK(back.value().links.size() == g.links.size());
    CHECK(compileShaderGraph(back.value()).source == r.source);
    CHECK(shaderUriForGraph("assets://fx/fire.agrshadergraph") == "assets://fx/fire.agrshader");
}

TEST_CASE("compile errors: cycles refused, fragment-only nodes in the vertex stage, bad names, texture types") {
    ShaderGraph g = ShaderGraph::create(ShaderGraph::Target::Surface);
    const int a = g.addNode("Add", 0, 0), b = g.addNode("Add", 0, 0);
    REQUIRE(g.connect(a, "out", b, "a"));
    CHECK_FALSE(g.connect(b, "out", a, "a"));  // would loop
    const int tex = g.addNode("ParamTexture", 0, 0);
    CHECK_FALSE(g.connect(tex, "tex", a, "b"));  // a texture into a number
    const int vo = g.addNode("VertexOffset", 0, 0);
    const int fres = g.addNode("Fresnel", 0, 0);
    REQUIRE(g.connect(fres, "out", vo, "offset"));
    auto r = compileShaderGraph(g);
    CHECK_FALSE(r.ok());
    CHECK(r.errors[0].find("Fresnel") != std::string::npos);
    g.removeNode(vo);
    const int p = g.addNode("ParamFloat", 0, 0);
    g.node(p)->values["name"] = "texture";  // reserved
    REQUIRE(g.connect(p, "v", g.nodes[0].id, "metallic"));
    r = compileShaderGraph(g);
    CHECK_FALSE(r.ok());
    g.node(p)->values["name"] = "shine";
    CHECK(compileShaderGraph(g).ok());
    // Two parameters with the same name.
    const int p2 = g.addNode("ParamFloat", 0, 0);
    g.node(p2)->values["name"] = "shine";
    REQUIRE(g.connect(p2, "v", g.nodes[0].id, "roughness"));
    CHECK_FALSE(compileShaderGraph(g).ok());
    // Sprite graphs: SampleTexture without a texture reads the sprite's own texture.
    ShaderGraph sg = ShaderGraph::create(ShaderGraph::Target::Sprite);
    const int st = sg.addNode("SampleTexture", 0, 0);
    REQUIRE(sg.connect(st, "rgba", sg.nodes[0].id, "color"));
    auto sr = compileShaderGraph(sg);
    REQUIRE(sr.ok());
    CHECK(sr.source.find("texture(TEXTURE, s.uv)") != std::string::npos);
    CHECK(sg.addNode("Fresnel", 0, 0) > 0);
    const int fr = sg.nodes.back().id;
    REQUIRE(sg.connect(fr, "out", sg.nodes[0].id, "color"));
    CHECK_FALSE(compileShaderGraph(sg).ok());  // surface-only node in a sprite graph
}
}  // TEST_SUITE
