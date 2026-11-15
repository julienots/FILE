// Visual logic: catalog, editing rules, JSON format, compiler and execution by the script engine.
#include <doctest/doctest.h>

#include <filesystem>
#include <set>

#include "agr/core/Uuid.hpp"
#include "agr/script/LogicGraph.hpp"
#include "agr/script/ScriptComponent.hpp"
#include "agr/script/ScriptEngine.hpp"
#include "agr/runtime/Input.hpp"
#include "agr/core/FileSystem.hpp"
#include "agr/scene/Scene.hpp"
#include "agr/render/RenderComponents.hpp"

using namespace agr;
using namespace agr::logic;
namespace fs = std::filesystem;

namespace {

struct LogicFixture {
    fs::path root = fs::temp_directory_path() / ("agr_logic_" + Uuid::generate().toString());
    VirtualFileSystem vfs;
    Scene scene;
    Input input;
    ScriptEngine scripts{vfs};
    FrameTime time;
    LogicFixture() {
        fs::create_directories(root);
        vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
        registerRenderComponents();
        registerScriptComponent();
        scripts.setInput(&input);
        time.deltaSeconds = time.unscaledDeltaSeconds = 0.1;
    }
    ~LogicFixture() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    ecs::Entity scripted(const char* name, const std::string& uri) {
        const auto e = scene.createEntity(name);
        scene.registry().emplace<ScriptComponent>(e).script = uri;
        return e;
    }
    void update(int frames = 1) {
        for (int i = 0; i < frames; ++i) {
            input.beginFrame();
            scripts.update(scene, time);
            scene.flushDestroyed();
            scene.updateTransforms();
        }
    }
};

/// Finds the id of the n-th node of a type.
int add(Graph& g, const char* type, std::map<std::string, std::string> values = {}) {
    const int id = g.addNode(type, 0, 0);
    REQUIRE_MESSAGE(id != 0, type);
    for (auto& [k, v] : values) g.node(id)->values[k] = v;
    return id;
}

void link(Graph& g, int a, const char* pa, int b, const char* pb) {
    const Status st = g.connect(a, pa, b, pb);
    REQUIRE_MESSAGE(st.ok(), (st.ok() ? "" : st.error().message));
}

} // namespace

TEST_SUITE("logic") {

TEST_CASE("catalog: unique types, French titles, every node compiles inside a chain") {
    std::set<std::string> types;
    for (const NodeDef& d : nodeCatalog()) {
        CHECK_MESSAGE(types.insert(d.type).second, d.type);
        CHECK(!d.title.empty());
        CHECK(!d.category.empty());
        CHECK_FALSE((d.event && d.pure));
    }
    CHECK(nodeCatalog().size() >= 50);
    // Each action / flow node after a start event; each value node into a log action.
    for (const NodeDef& d : nodeCatalog()) {
        Graph g;
        g.variables.push_back({"score", PinKind::Number, "0"});
        const int start = add(g, "event.start");
        int target = 0;
        if (d.event) {
            target = add(g, d.type.c_str());
            link(g, target, "next", add(g, "act.log"), "in");
        } else if (d.pure) {
            const int log = add(g, "act.log");
            link(g, start, "next", log, "in");
            target = add(g, d.type.c_str());
            for (const PinDef& p : d.outputs) {
                if (compatible(p.kind, PinKind::Any)) {
                    link(g, target, p.id.c_str(), log, "text");
                    break;
                }
            }
        } else {
            target = add(g, d.type.c_str());
            link(g, start, "next", target, "in");
        }
        const CompileResult r = compile(g, d.type);
        CHECK_MESSAGE(r.ok(), d.type << ": " << (r.errors.empty() ? "" : r.errors.front()));
    }
}

TEST_CASE("editing: directions, kinds, single links, removal, JSON round trip") {
    Graph g;
    const int start = add(g, "event.start");
    const int move = add(g, "act.move");
    const int pos = add(g, "val.position");
    const int self = add(g, "val.self");
    CHECK_FALSE(g.connect(move, "in", start, "next").ok());       // input to output
    CHECK_FALSE(g.connect(start, "next", move, "x").ok());         // exec into a number
    CHECK_FALSE(g.connect(self, "entity", move, "x").ok());        // entity into a number
    CHECK(g.connect(pos, "x", move, "x").ok());
    CHECK(g.connect(pos, "y", move, "x").ok());                    // replaces the previous source
    CHECK(g.linkTo(move, "x")->fromPin == "y");
    CHECK(g.connect(start, "next", move, "in").ok());
    const int log = add(g, "act.log");
    CHECK(g.connect(start, "next", log, "in").ok());               // an exec output has one next step
    CHECK(g.linkFrom(start, "next")->toNode == log);
    CHECK(g.connect(pos, "x", log, "text").ok());                  // number into text: allowed
    g.variables.push_back({"lives", PinKind::Number, "3"});
    g.node(move)->values["x"] = "2.5";
    auto back = Graph::fromJson(g.toJson());
    REQUIRE(back);
    CHECK(back.value().nodes.size() == g.nodes.size());
    CHECK(back.value().links == g.links);
    CHECK(back.value().variables.front().name == "lives");
    CHECK(back.value().node(move)->values.at("x") == "2.5");
    CHECK(back.value().nextId == g.nextId);
    g.removeNode(pos);
    CHECK(g.linkTo(move, "x") == nullptr);
    CHECK_FALSE(Graph::fromJson("{\"format\": \"other\"}").ok());
    CHECK_FALSE(Graph::fromJson("not json").ok());
}

TEST_CASE("compiler: errors and warnings are reported, not hidden") {
    Graph g;
    const int start = add(g, "event.start");
    const int set = add(g, "act.setVar", {{"name", "bad name"}});
    link(g, start, "next", set, "in");
    add(g, "act.log");  // unreachable
    CompileResult r = compile(g);
    CHECK_FALSE(r.ok());
    CHECK(r.errors.front().find("bad name") != std::string::npos);
    CHECK_FALSE(r.warnings.empty());
    // A number field with letters.
    Graph h;
    const int s2 = add(h, "event.start");
    const int mv = add(h, "act.move", {{"x", "abc"}});
    link(h, s2, "next", mv, "in");
    CHECK_FALSE(compile(h).ok());
    // Unknown node types (a newer engine's graph).
    Graph u;
    u.nodes.push_back({1, "future.node", 0, 0, {}});
    CHECK_FALSE(compile(u).ok());
}

TEST_CASE("execution: events, variables, branches, loops, timers, messages, keys run in the engine") {
    LogicFixture f;
    Graph g;
    g.variables.push_back({"score", PinKind::Number, "0"});
    g.variables.push_back({"ticks", PinKind::Number, "0"});
    g.variables.push_back({"title", PinKind::Text, "start"});
    // Au démarrage : score = 2 ; répéter 3 fois : score += index (2 + 1 + 2 + 3 = 8) ; puis title.
    const int start = add(g, "event.start");
    const int setScore = add(g, "act.setVar", {{"name", "score"}, {"value", "2"}});
    const int rep = add(g, "flow.repeat", {{"count", "3"}});
    const int addIdx = add(g, "act.addVar", {{"name", "score"}});
    const int setTitle = add(g, "act.setVar", {{"name", "title"}});
    const int concat = add(g, "math.concat", {{"a", "score="}});
    const int getScore = add(g, "val.var", {{"name", "score"}});
    link(g, start, "next", setScore, "in");
    link(g, setScore, "next", rep, "in");
    link(g, rep, "body", addIdx, "in");
    link(g, rep, "index", addIdx, "amount");
    link(g, rep, "done", setTitle, "in");
    link(g, getScore, "out", concat, "b");
    link(g, concat, "out", setTitle, "value");
    // À chaque image : déplacer de x = 2 par seconde.
    const int update = add(g, "event.update");
    const int move = add(g, "act.move", {{"x", "2"}});
    link(g, update, "next", move, "in");
    // Toutes les 0,5 s : ticks += 1.
    const int every = add(g, "event.every", {{"seconds", "0.5"}});
    const int tick = add(g, "act.addVar", {{"name", "ticks"}});
    link(g, every, "next", tick, "in");
    // Touche Espace : si score > 5, cacher « Target », sinon rien ; puis après 0,3 s, placer Target.
    const int key = add(g, "event.keyPressed", {{"key", "Space"}});
    const int branch = add(g, "flow.branch");
    const int cmp = add(g, "math.compare", {{"op", ">"}, {"b", "5"}});
    const int score2 = add(g, "val.var", {{"name", "score"}});
    const int hide = add(g, "act.setActive", {{"entity", "Target"}, {"active", "false"}});
    const int delay = add(g, "flow.delay", {{"seconds", "0.3"}});
    const int place = add(g, "act.setPosition", {{"entity", "Target"}, {"y", "7"}});
    link(g, key, "next", branch, "in");
    link(g, score2, "out", cmp, "a");
    link(g, cmp, "out", branch, "condition");
    link(g, branch, "true", hide, "in");
    link(g, hide, "next", delay, "in");
    link(g, delay, "next", place, "in");
    // Message « hit » : score += valeur reçue.
    const int msg = add(g, "event.message", {{"name", "hit"}});
    const int addValue = add(g, "act.addVar", {{"name", "score"}});
    link(g, msg, "next", addValue, "in");
    link(g, msg, "value", addValue, "amount");

    const CompileResult r = compile(g, "assets://logic/test.agrlogic");
    REQUIRE_MESSAGE(r.ok(), (r.errors.empty() ? "" : r.errors.front()));
    REQUIRE(f.vfs.writeText("assets://logic/test.agrlogic", g.toJson()));
    const auto target = f.scene.createEntity("Target");
    const auto e = f.scripted("Logic", "assets://logic/test.agrlogic");
    f.update();  // onStart then onUpdate
    CHECK(f.scripts.field(e, "score").value() == "8");
    CHECK(f.scripts.field(e, "title").value() == "score=8");
    f.update(9);  // 1 s in total
    CHECK(f.scene.registry().get<TransformComponent>(e).position.x == doctest::Approx(2.0f).epsilon(0.01));
    CHECK(f.scripts.field(e, "ticks").value() == "2");
    // Key: score 8 > 5 -> Target hidden, then moved 0.3 s later.
    f.input.beginFrame();
    f.input.setKey(Key::Space, true);
    f.scripts.update(f.scene, f.time);
    CHECK_FALSE(f.scene.registry().get<StateComponent>(target).active);
    CHECK(f.scene.registry().get<TransformComponent>(target).position.y == doctest::Approx(0.0f));
    f.input.setKey(Key::Space, false);
    f.update(4);
    CHECK(f.scene.registry().get<TransformComponent>(target).position.y == doctest::Approx(7.0f));
    // Message from another script.
    REQUIRE(f.scripts.run("agr.find('Logic'):script():hit(nil, 5)", &f.scene));
    CHECK(f.scripts.field(e, "score").value() == "13");
    // A broken graph is reported as a script error, the scene keeps running.
    Graph bad;
    const int s = add(bad, "event.start");
    link(bad, s, "next", add(bad, "act.addVar", {{"name", "1x"}}), "in");
    REQUIRE(f.vfs.writeText("assets://logic/bad.agrlogic", bad.toJson()));
    f.scripted("Bad", "assets://logic/bad.agrlogic");
    const u32 errors = f.scripts.stats().errors;
    f.update();
    CHECK(f.scripts.stats().errors > errors);
}

}
