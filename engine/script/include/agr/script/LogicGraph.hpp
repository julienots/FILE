#pragma once
// Visual logic (no-code scripting): a graph of nodes saved as JSON (.agrlogic) and compiled to
// Lua by the engine when a Script component points at it. Events start chains of actions (exec
// links); values flow through data links. See docs/LOGIC.md.
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr::logic {

enum class PinKind : u8 { Exec, Number, Text, Bool, Entity, Any };
const char* enumName(PinKind kind);

struct PinDef {
    std::string id;
    std::string label;
    PinKind kind = PinKind::Any;
    std::string defaultValue;          ///< Inline value of an unconnected input / field.
    std::vector<std::string> choices;  ///< Fields only: allowed values (combo box).
};

/// A node type of the catalog. Fields are settings edited on the node (key, variable name...);
/// inputs can be linked or take an inline value; outputs are exec or data.
struct NodeDef {
    std::string type;      ///< "event.start", "act.move"...
    std::string title;     ///< Shown on the node (French; the editor UI is French).
    std::string category;  ///< "Événements", "Flux", "Actions", "Effets", "Valeurs", "Maths"
    std::string tooltip;
    std::vector<PinDef> fields, inputs, outputs;
    bool event = false;  ///< Starts a chain (no exec input).
    bool pure = false;   ///< Value node: no exec pins, evaluated where used.
};

const std::vector<NodeDef>& nodeCatalog();
const NodeDef* findNodeDef(std::string_view type);

struct Node {
    int id = 0;
    std::string type;
    float x = 0.0f, y = 0.0f;
    std::map<std::string, std::string> values;  ///< Field values and inline input values.
};

struct Link {
    int fromNode = 0;
    std::string fromPin;
    int toNode = 0;
    std::string toPin;
    bool operator==(const Link&) const = default;
};

struct Variable {
    std::string name;
    PinKind type = PinKind::Number;
    std::string value;  ///< Default value (editable per entity in the inspector).
};

/// A note on the canvas (Godot's GraphFrame): a titled rectangle; dragging it carries the nodes inside.
struct Comment {
    float x = 0.0f, y = 0.0f, w = 320.0f, h = 200.0f;
    std::string text = "Commentaire";
    u32 color = 0x3C6E9Bu;  ///< 0xRRGGBB
};

struct Graph {
    std::vector<Comment> comments;
    std::vector<Node> nodes;
    std::vector<Link> links;
    std::vector<Variable> variables;
    int nextId = 1;

    Node* node(int id);
    const Node* node(int id) const;
    /// Adds a node of a catalog type at (x, y) with its default values; returns its id (0 if the
    /// type is unknown).
    int addNode(const std::string& type, float x, float y);
    void removeNode(int id);
    /// Links an output pin to an input pin. Exec outputs and data inputs keep a single link (the
    /// new one replaces the old). Fails on unknown pins, wrong directions or incompatible kinds.
    Status connect(int fromNode, const std::string& fromPin, int toNode, const std::string& toPin);
    void disconnect(int toNode, const std::string& toPin);
    /// Link arriving at an input pin, or nullptr.
    const Link* linkTo(int toNode, const std::string& toPin) const;
    const Link* linkFrom(int fromNode, const std::string& fromPin) const;  ///< First link leaving an output.

    std::string toJson() const;
    static Result<Graph> fromJson(std::string_view text);
};

/// Kinds a data output of kind `from` may feed into an input of kind `to`.
bool compatible(PinKind from, PinKind to);

struct CompileResult {
    std::string lua;
    std::vector<std::string> errors;    ///< The script does not compile / would not run.
    std::vector<std::string> warnings;  ///< Ignored nodes, unconnected chains...
    bool ok() const { return errors.empty(); }
};

/// Generates the Lua script of a graph (a class table with properties = the variables).
CompileResult compile(const Graph& graph, const std::string& sourceName = "");

} // namespace agr::logic
