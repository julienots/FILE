#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "agr/scene/Reflection.hpp"
#include "agr/scene/Scene.hpp"
#include "agr/scene/SceneSerializer.hpp"

using namespace agr;
using ecs::Entity;
using math::Vec3;

namespace {

enum class Team : i32 { Red = 0, Blue = 7 };

struct FollowComponent {
    Entity target{};
    float distance = 2.0f;
    Team team = Team::Red;
    math::Vec4 tint{1, 1, 1, 1};
    std::string label = "follower";
};

void registerTestComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<FollowComponent>()) return;
    reg.add<FollowComponent>("TestFollow")
        .category("Test")
        .field("target", &FollowComponent::target)
        .field("distance", &FollowComponent::distance, {.rangeMin = 0, .rangeMax = 100})
        .field("team", &FollowComponent::team, {.enumEntries = {{"Red", 0}, {"Blue", 7}}})
        .field("tint", &FollowComponent::tint, {.flags = FieldFlag_Color})
        .field("label", &FollowComponent::label);
}

std::vector<std::string> names(const Scene& s, const std::vector<Entity>& es) {
    std::vector<std::string> out;
    for (Entity e : es) out.push_back(s.nameOf(e));
    return out;
}

}  // namespace

TEST_SUITE("scene") {

TEST_CASE("reflection describes fields") {
    registerTestComponents();
    const ComponentInfo* info = ComponentRegistry::instance().find("TestFollow");
    REQUIRE(info);
    CHECK(info->fields.size() == 5);
    CHECK(info->field("target")->type == FieldType::Entity);
    CHECK(info->field("team")->type == FieldType::Enum);
    CHECK(info->field("distance")->hasRange());
    CHECK((info->field("tint")->flags & FieldFlag_Color) != 0);
    CHECK(ComponentBuilder<FollowComponent>::prettify("linearDamping") == "Linear Damping");
    FollowComponent c;
    info->field("distance")->ref<float>(&c) = 9.0f;
    CHECK(c.distance == 9.0f);
    CHECK(ComponentRegistry::instance().find<TransformComponent>() != nullptr);
}

TEST_CASE("hierarchy keeps sibling order and supports reordering") {
    Scene s;
    const Entity root = s.createEntity("Root");
    const Entity a = s.createEntity("A", root);
    const Entity b = s.createEntity("B", root);
    const Entity c = s.createEntity("C", root);
    CHECK(names(s, s.children(root)) == std::vector<std::string>{"A", "B", "C"});
    s.setParent(c, root, a);  // move C before A
    CHECK(names(s, s.children(root)) == std::vector<std::string>{"C", "A", "B"});
    s.setParent(a, Entity{});  // A becomes a root, appended last
    CHECK(names(s, s.roots()) == std::vector<std::string>{"Root", "A"});
    CHECK(names(s, s.children(root)) == std::vector<std::string>{"C", "B"});
    CHECK(s.registry().get<HierarchyComponent>(root).childCount == 2);
    s.setParent(b, a, s.roots().front());  // 'before' from another parent -> append
    CHECK(s.parent(b) == a);
}

TEST_CASE("cycles are rejected") {
    Scene s;
    const Entity a = s.createEntity("A");
    const Entity b = s.createEntity("B", a);
    const Entity c = s.createEntity("C", b);
    CHECK_FALSE(s.setParent(a, c));
    CHECK_FALSE(s.setParent(a, a));
    CHECK(s.isAncestorOf(a, c));
    CHECK(s.parent(a).isNull());
}

TEST_CASE("destroying a parent destroys the subtree") {
    Scene s;
    const Entity a = s.createEntity("A");
    const Entity b = s.createEntity("B", a);
    s.createEntity("C", b);
    const Entity keep = s.createEntity("Keep");
    const Uuid bId = s.uuidOf(b);
    s.destroyEntity(a);
    CHECK(s.entityCount() == 1);
    CHECK(s.valid(keep));
    CHECK(s.findByUuid(bId).isNull());
    CHECK(names(s, s.roots()) == std::vector<std::string>{"Keep"});
}

TEST_CASE("deferred destruction") {
    Scene s;
    const Entity a = s.createEntity("A");
    s.createEntity("B", a);
    s.destroyDeferred(a);
    s.destroyDeferred(a);
    CHECK(s.entityCount() == 2);
    s.flushDestroyed();
    CHECK(s.entityCount() == 0);
}

TEST_CASE("world transforms follow the hierarchy") {
    Scene s;
    const Entity parent = s.createEntity("Parent");
    const Entity child = s.createEntity("Child", parent);
    auto& pt = s.registry().get<TransformComponent>(parent);
    pt.position = {10, 0, 0};
    pt.rotation = math::Quat::fromAxisAngle({0, 1, 0}, math::radians(90));
    pt.scale = Vec3{2};
    s.registry().get<TransformComponent>(child).position = {1, 0, 0};
    s.updateTransforms();
    const Vec3 world = s.registry().get<WorldTransformComponent>(child).matrix.translation();
    CHECK(math::nearlyEqual(world, Vec3{10, 0, -2}));
    CHECK(math::nearlyEqual(s.worldMatrix(child).translation(), world));
}

TEST_CASE("re-parenting keeps the world transform") {
    Scene s;
    const Entity p = s.createEntity("P");
    s.registry().get<TransformComponent>(p).position = {5, 5, 5};
    s.registry().get<TransformComponent>(p).rotation = math::Quat::fromAxisAngle({0, 0, 1}, 0.7f);
    const Entity e = s.createEntity("E");
    s.registry().get<TransformComponent>(e).position = {1, 2, 3};
    s.setParent(e, p);
    CHECK(math::nearlyEqual(s.worldMatrix(e).translation(), Vec3{1, 2, 3}));
    s.setParent(e, Entity{});
    CHECK(math::nearlyEqual(s.registry().get<TransformComponent>(e).position, Vec3{1, 2, 3}));
}

TEST_CASE("duplicate deep-copies components and children") {
    registerTestComponents();
    Scene s;
    const Entity root = s.createEntity("Root");
    const Entity a = s.createEntity("A", root);
    s.createEntity("A.child", a);
    s.createEntity("B", root);
    s.registry().emplace<FollowComponent>(a).distance = 42.0f;
    const Entity copy = s.duplicate(a);
    CHECK(names(s, s.children(root)) == std::vector<std::string>{"A", "A", "B"});
    CHECK(s.uuidOf(copy) != s.uuidOf(a));
    CHECK(s.registry().get<FollowComponent>(copy).distance == 42.0f);
    REQUIRE(s.children(copy).size() == 1);
    CHECK(s.nameOf(s.children(copy)[0]) == "A.child");
    CHECK(s.entityCount() == 6);
}

TEST_CASE("scene serialization round trip") {
    registerTestComponents();
    Scene s("Level 1");
    const Entity player = s.createEntity("Player");
    const Entity cam = s.createEntity("Camera", player);
    s.registry().get<TransformComponent>(cam).position = {0, 2, -5};
    s.registry().get<StateComponent>(cam).layer = 3;
    auto& follow = s.registry().emplace<FollowComponent>(cam);
    follow.target = player;
    follow.team = Team::Blue;
    follow.tint = {0.5f, 0.25f, 1.0f, 1.0f};
    follow.label = "main camera";
    s.createEntity("Light");

    const std::string text = SceneSerializer::toString(s);
    Scene loaded;
    REQUIRE(SceneSerializer::fromString(loaded, text));
    CHECK(loaded.name() == "Level 1");
    CHECK(loaded.entityCount() == 3);
    CHECK(names(loaded, loaded.roots()) == std::vector<std::string>{"Player", "Light"});
    const Entity lp = loaded.findByUuid(s.uuidOf(player));
    const Entity lc = loaded.findByUuid(s.uuidOf(cam));
    REQUIRE(loaded.valid(lc));
    CHECK(loaded.parent(lc) == lp);
    CHECK(loaded.registry().get<TransformComponent>(lc).position == Vec3{0, 2, -5});
    CHECK(loaded.registry().get<StateComponent>(lc).layer == 3);
    const auto& lf = loaded.registry().get<FollowComponent>(lc);
    CHECK(lf.target == lp);
    CHECK(lf.team == Team::Blue);
    CHECK(lf.tint == math::Vec4{0.5f, 0.25f, 1.0f, 1.0f});
    CHECK(lf.label == "main camera");
    // Stable output: serializing the loaded scene yields the same document.
    CHECK(SceneSerializer::toString(loaded) == text);
    // Enums are written by name.
    CHECK(text.find("\"Blue\"") != std::string::npos);
}

TEST_CASE("serializer rejects bad input and survives unknown components") {
    Scene s;
    CHECK_FALSE(SceneSerializer::fromString(s, "not json"));
    CHECK_FALSE(SceneSerializer::fromString(s, R"({"format":"other"})"));
    CHECK(SceneSerializer::fromString(s, R"({"format":"agr.scene","version":99})").error().code ==
          Error::Code::VersionMismatch);
    const char* doc = R"({"format":"agr.scene","version":1,"name":"X","entities":[
        {"id":"00000000000000aa","parent":null,"components":{
            "Name":{"name":"Thing"},"Transform":{"position":[1,2,"bad"]},"FutureComponent":{"x":1}}}]})";
    REQUIRE(SceneSerializer::fromString(s, doc));
    const Entity e = s.findByUuid(Uuid(0xaa));
    REQUIRE(s.valid(e));
    CHECK(s.nameOf(e) == "Thing");
    CHECK(s.registry().get<TransformComponent>(e).position == Vec3{0, 0, 0});  // invalid value -> default
}

TEST_CASE("entity snippets instantiate with fresh ids (prefab / paste)") {
    registerTestComponents();
    Scene s;
    const Entity enemy = s.createEntity("Enemy");
    const Entity eye = s.createEntity("Eye", enemy);
    s.registry().emplace<FollowComponent>(eye).target = enemy;  // internal reference
    const auto snippet = SceneSerializer::entityToJson(s, enemy);
    auto inst = SceneSerializer::instantiate(s, snippet);
    REQUIRE(inst);
    const Entity copy = inst.value();
    CHECK(copy != enemy);
    CHECK(s.uuidOf(copy) != s.uuidOf(enemy));
    REQUIRE(s.children(copy).size() == 1);
    // The internal reference is remapped to the new instance, not the original.
    CHECK(s.registry().get<FollowComponent>(s.children(copy)[0]).target == copy);
    CHECK(s.entityCount() == 4);
}

}  // TEST_SUITE
