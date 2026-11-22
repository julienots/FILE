#pragma once
// Visual shaders (.agrshadergraph): nodes wired together and compiled to a regular .agrshader
// (surface + vertex functions for 3D materials, or a sprite function), so a graph shader uses
// the same rendering path, parameters and error reporting as a hand-written one.
#include <map>
#include <string>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr {

/// Value types of the ports. Sampler carries a texture parameter (or the sprite texture).
enum class ShaderPortType : u8 { Float = 1, Vec2 = 2, Vec3 = 3, Vec4 = 4, Sampler = 5, Dynamic = 0 };

struct ShaderPortDef {
    std::string id, label;
    ShaderPortType type = ShaderPortType::Float;
    std::string defaultValue;  ///< Literal used when the input is not linked ("0.5", "1,0,0,1", "@uv").
};

struct ShaderNodeDef {
    std::string type, title, category, tooltip;
    std::vector<ShaderPortDef> inputs, outputs;
    std::vector<ShaderPortDef> fields;  ///< Settings that are not ports (parameter names, texture paths).
    u8 targets = 3;                     ///< 1 surface, 2 sprite, 3 both.
};
const std::vector<ShaderNodeDef>& shaderNodeDefs();
const ShaderNodeDef* findShaderNodeDef(const std::string& type);

struct ShaderGraph {
    enum class Target : u8 { Surface = 0, Sprite = 1 };
    struct Node {
        int id = 0;
        std::string type;
        float x = 0.0f, y = 0.0f;
        std::map<std::string, std::string> values;  ///< Unlinked inputs and fields.
    };
    struct Link {
        int fromNode = 0;
        std::string fromPort;
        int toNode = 0;
        std::string toPort;
    };
    Target target = Target::Surface;
    std::vector<Node> nodes;
    std::vector<Link> links;
    int nextId = 1;

    /// New graph with its output node (Surface output, or Sprite output).
    static ShaderGraph create(Target target);
    int addNode(const std::string& type, float x, float y);
    void removeNode(int id);
    Node* node(int id);
    const Node* node(int id) const;
    /// Links an output to an input (replaces the input's link). Refuses unknown ports, a texture
    /// into a value input (and the reverse) and links that would make a cycle.
    Status connect(int fromNode, const std::string& fromPort, int toNode, const std::string& toPort);
    void disconnect(int toNode, const std::string& toPort);
    const Link* linkTo(int node, const std::string& port) const;

    std::string toJson() const;
    static Result<ShaderGraph> fromJson(std::string_view json);
    static constexpr const char* kExtension = ".agrshadergraph";
};

struct ShaderGraphCompileResult {
    std::string source;               ///< .agrshader text (empty on errors).
    std::vector<std::string> errors;  ///< "Node title (n° id): problem".
    bool ok() const { return errors.empty(); }
};
ShaderGraphCompileResult compileShaderGraph(const ShaderGraph& graph);

/// The .agrshader written next to a graph ("fx/fire.agrshadergraph" -> "fx/fire.agrshader").
std::string shaderUriForGraph(const std::string& graphUri);

} // namespace agr
