#include <doctest/doctest.h>

#include <filesystem>

#include <nlohmann/json.hpp>

#include "agr/core/FileSystem.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/scene/Prefab.hpp"
#include "agr/scene/Reflection.hpp"
#include "agr/scene/Scene.hpp"
#include "agr/scene/SceneSerializer.hpp"

using namespace agr;
using ecs::Entity;
using json = nlohmann::ordered_json;
using math::Vec3;

namespace {

struct AimComponent {
    Entity target{};
    float damage = 1.0f;
    std::string tag = "enemy";
};
struct ArmorComponent {
    float value = 5.0f;
};

void registerPrefabTestComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<AimComponent>()) return;
    reg.add<AimComponent>("TestAim")
        .category("Test")
        .field("target", &AimComponent::target)
        .field("damage", &AimComponent::damage)
        .field("tag", &AimComponent::tag);
    reg.add<ArmorComponent>("TestArmor").category("Test").field("value", &ArmorComponent::value);
}

struct Fixture {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("agr_prefab_" + Uuid::generate().toString());
    VirtualFileSystem vfs;
    PrefabLibrary library{vfs};
    Scene scene;

    Fixture() {
        registerPrefabTestComponents();
        std::filesystem::create_directories(root);
        vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
        library.install();
    }
    ~Fixture() {
        setPrefabResolver(nullptr);
        std::filesystem::remove_all(root);
    }
    /// Enemy (TestArmor) > Body, Gun (TestAim -> Body).
    Entity enemy(Scene& s) {
        const Entity e = s.createEntity("Enemy");
        s.registry().emplace<ArmorComponent>(e);
        const Entity body = s.createEntity("Body", e);
        s.registry().get<TransformComponent>(body).position = {0, 1, 0};
        const Entity gun = s.createEntity("Gun", e);
        s.registry().emplace<AimComponent>(gun).target = body;
        return e;
    }
    /// Makes a prefab of `e` and links it.
    void makePrefab(Scene& s, Entity e, const std::string& uri) {
        REQUIRE(library.save(uri, prefab::fromEntity(s, e)));
        REQUIRE(prefab::link(s, e, uri));
    }
    Entity child(Scene& s, Entity e, std::string_view name) {
        for (Entity c : s.children(e))
            if (s.nameOf(c) == name) return c;
        return {};
    }
    void reload(Scene& s, Scene& out) { REQUIRE(SceneSerializer::fromString(out, SceneSerializer::toString(s))); }
};

}  // namespace

TEST_SUITE("prefab") {

TEST_CASE("an instance is saved as a reference: ids derive from the root, references stay inside") {
    Fixture f;
    const Entity e = f.enemy(f.scene);
    const Entity gunBefore = f.child(f.scene, e, "Gun");
    f.makePrefab(f.scene, e, "assets://enemy.agrprefab");
    CHECK(prefab::isInstanceRoot(f.scene, e));
    CHECK(prefab::instanceRootOf(f.scene, gunBefore) == e);
    CHECK(f.scene.registry().get<AimComponent>(gunBefore).target == f.child(f.scene, e, "Body"));  // handles kept by link
    CHECK(prefab::overrideCount(f.scene, e) == 0);

    const json saved = SceneSerializer::toJson(f.scene);
    REQUIRE(saved["entities"].size() == 1);  // children come from the prefab
    CHECK(saved["entities"][0]["prefab"] == "assets://enemy.agrprefab");

    Scene loaded;
    f.reload(f.scene, loaded);
    const Entity e2 = loaded.findByUuid(f.scene.uuidOf(e));
    REQUIRE(loaded.valid(e2));
    const Entity gun = f.child(loaded, e2, "Gun");
    REQUIRE(loaded.valid(gun));
    CHECK(loaded.uuidOf(gun) == f.scene.uuidOf(gunBefore));  // same derived id after a reload
    CHECK(loaded.registry().get<AimComponent>(gun).target == f.child(loaded, e2, "Body"));
    CHECK(loaded.registry().get<TransformComponent>(f.child(loaded, e2, "Body")).position.y == doctest::Approx(1.0f));
}

TEST_CASE("per-instance changes survive a reload; other instances are untouched") {
    Fixture f;
    f.makePrefab(f.scene, f.enemy(f.scene), "assets://enemy.agrprefab");
    auto a = prefab::instantiate(f.scene, "assets://enemy.agrprefab");
    auto b = prefab::instantiate(f.scene, "assets://enemy.agrprefab");
    REQUIRE(a);
    REQUIRE(b);
    CHECK(f.scene.uuidOf(f.child(f.scene, a.value(), "Gun")) != f.scene.uuidOf(f.child(f.scene, b.value(), "Gun")));
    // Instance a: moved, renamed, a changed field, a removed component, a removed child, an added child, an added component.
    auto& reg = f.scene.registry();
    reg.get<TransformComponent>(a.value()).position = {5, 0, 0};
    reg.get<NameComponent>(a.value()).name = "Boss";
    reg.get<AimComponent>(f.child(f.scene, a.value(), "Gun")).damage = 9.0f;
    reg.remove<ArmorComponent>(a.value());
    f.scene.destroyEntity(f.child(f.scene, a.value(), "Body"));
    const Entity shield = f.scene.createEntity("Shield", a.value());
    reg.emplace<ArmorComponent>(shield).value = 50.0f;
    reg.emplace<ArmorComponent>(f.child(f.scene, a.value(), "Gun")).value = 2.0f;
    // damage, the gun's target (Body is gone), removed Armor, removed Body, Shield, Armor on Gun
    CHECK(prefab::overrideCount(f.scene, a.value()) == 6);

    Scene loaded;
    f.reload(f.scene, loaded);
    const Entity la = loaded.findByUuid(f.scene.uuidOf(a.value()));
    const Entity lb = loaded.findByUuid(f.scene.uuidOf(b.value()));
    REQUIRE(loaded.valid(la));
    REQUIRE(loaded.valid(lb));
    auto& lr = loaded.registry();
    CHECK(loaded.nameOf(la) == "Boss");
    CHECK(lr.get<TransformComponent>(la).position.x == doctest::Approx(5.0f));
    CHECK_FALSE(lr.tryGet<ArmorComponent>(la));
    CHECK(loaded.findByName("Body") != Entity{});  // b still has its Body
    CHECK(f.child(loaded, la, "Body").isNull());
    const Entity lshield = f.child(loaded, la, "Shield");
    REQUIRE(loaded.valid(lshield));
    CHECK(lr.get<ArmorComponent>(lshield).value == doctest::Approx(50.0f));
    CHECK(lr.get<AimComponent>(f.child(loaded, la, "Gun")).damage == doctest::Approx(9.0f));
    CHECK(lr.get<ArmorComponent>(f.child(loaded, la, "Gun")).value == doctest::Approx(2.0f));
    CHECK(lr.get<AimComponent>(f.child(loaded, lb, "Gun")).damage == doctest::Approx(1.0f));
    CHECK(lr.tryGet<ArmorComponent>(lb));
    CHECK(prefab::overrideCount(loaded, la) == 6);
    CHECK(prefab::overrideCount(loaded, lb) == 0);
}

TEST_CASE("applying an instance updates the prefab and every other instance, keeping their own changes") {
    Fixture f;
    const Entity source = f.enemy(f.scene);
    f.makePrefab(f.scene, source, "assets://enemy.agrprefab");
    Entity other = prefab::instantiate(f.scene, "assets://enemy.agrprefab").value();
    auto& reg = f.scene.registry();
    reg.get<AimComponent>(f.child(f.scene, other, "Gun")).tag = "elite";  // its own change

    // Outside reference to the other instance's gun.
    const Entity turret = f.scene.createEntity("Turret");
    reg.emplace<AimComponent>(turret).target = f.child(f.scene, other, "Gun");

    // Edit the source instance and apply: new damage, a new child.
    reg.get<AimComponent>(f.child(f.scene, source, "Gun")).damage = 3.0f;
    f.scene.createEntity("Helmet", source);
    auto content = prefab::applyContent(f.scene, source);
    REQUIRE(content);
    auto previous = f.library.save("assets://enemy.agrprefab", content.value());
    REQUIRE(previous);
    REQUIRE(prefab::link(f.scene, source, "assets://enemy.agrprefab"));
    CHECK(prefab::overrideCount(f.scene, source) == 0);
    const Uuid otherId = f.scene.uuidOf(other);
    CHECK(prefab::refreshAll(f.scene, "assets://enemy.agrprefab", previous.value(), source) == 1);
    other = f.scene.findByUuid(otherId);  // rebuilt: same id, new handle

    const Entity gun = f.child(f.scene, other, "Gun");
    REQUIRE(f.scene.valid(other));
    CHECK(f.scene.valid(f.child(f.scene, other, "Helmet")));
    CHECK(reg.get<AimComponent>(gun).damage == doctest::Approx(3.0f));  // from the prefab
    CHECK(reg.get<AimComponent>(gun).tag == "elite");                     // kept
    CHECK(reg.get<AimComponent>(turret).target == gun);                    // outside reference repaired
    CHECK(reg.get<AimComponent>(gun).target == f.child(f.scene, other, "Body"));
    // The prefab kept its entity ids: the saved scene still matches after a reload.
    Scene loaded;
    f.reload(f.scene, loaded);
    CHECK(loaded.registry().get<AimComponent>(loaded.findByName("Turret")).target == loaded.findByUuid(f.scene.uuidOf(gun)));
}

TEST_CASE("a prefab edited on disk updates instances on the next load") {
    Fixture f;
    f.makePrefab(f.scene, f.enemy(f.scene), "assets://enemy.agrprefab");
    const std::string text = SceneSerializer::toString(f.scene);
    // Another tool changes the prefab: armor 5 -> 8.
    json content = *f.library.get("assets://enemy.agrprefab");
    content["entities"][0]["components"]["TestArmor"]["value"] = 8.0f;
    REQUIRE(f.vfs.writeText("assets://enemy.agrprefab", content.dump()));
    f.library.clear();
    Scene loaded;
    REQUIRE(SceneSerializer::fromString(loaded, text));
    CHECK(loaded.registry().get<ArmorComponent>(loaded.findByName("Enemy")).value == doctest::Approx(8.0f));
}

TEST_CASE("nested prefabs: a car of wheels; editing the wheel prefab reaches the cars") {
    Fixture f;
    const Entity wheel = f.scene.createEntity("Wheel");
    f.scene.registry().emplace<ArmorComponent>(wheel).value = 1.0f;
    f.makePrefab(f.scene, wheel, "assets://wheel.agrprefab");
    const Entity car = f.scene.createEntity("Car");
    for (const char* name : {"FrontWheel", "RearWheel"}) {
        const Entity w = prefab::instantiate(f.scene, "assets://wheel.agrprefab", car).value();
        f.scene.registry().get<NameComponent>(w).name = name;
    }
    f.scene.destroyEntity(wheel);
    f.makePrefab(f.scene, car, "assets://car.agrprefab");
    const Entity car2 = prefab::instantiate(f.scene, "assets://car.agrprefab").value();
    REQUIRE(f.scene.valid(f.child(f.scene, car2, "RearWheel")));
    CHECK(prefab::isInstanceRoot(f.scene, f.child(f.scene, car2, "RearWheel")));
    // A change inside the nested wheel of car2 only.
    f.scene.registry().get<ArmorComponent>(f.child(f.scene, car2, "RearWheel")).value = 7.0f;

    Scene loaded;
    f.reload(f.scene, loaded);
    const Entity lcar2 = loaded.findByUuid(f.scene.uuidOf(car2));
    REQUIRE(loaded.valid(lcar2));
    CHECK(loaded.registry().get<ArmorComponent>(f.child(loaded, lcar2, "RearWheel")).value == doctest::Approx(7.0f));
    CHECK(loaded.registry().get<ArmorComponent>(f.child(loaded, lcar2, "FrontWheel")).value == doctest::Approx(1.0f));

    // The wheel prefab changes on disk while the scene is closed: every wheel without its own
    // value follows on the next load.
    const std::string text = SceneSerializer::toString(loaded);
    json w = *f.library.get("assets://wheel.agrprefab");
    w["entities"][0]["components"]["TestArmor"]["value"] = 4.0f;
    REQUIRE(f.vfs.writeText("assets://wheel.agrprefab", w.dump()));
    f.library.clear();
    Scene again;
    REQUIRE(SceneSerializer::fromString(again, text));
    const Entity acar2 = again.findByUuid(f.scene.uuidOf(car2));
    CHECK(again.registry().get<ArmorComponent>(f.child(again, acar2, "FrontWheel")).value == doctest::Approx(4.0f));
    CHECK(again.registry().get<ArmorComponent>(f.child(again, acar2, "RearWheel")).value == doctest::Approx(7.0f));

    // While the scene is open (editor hot reload): refresh with the previous content.
    const json previousWheel = *f.library.get("assets://wheel.agrprefab");
    w["entities"][0]["components"]["TestArmor"]["value"] = 6.0f;
    REQUIRE(f.library.save("assets://wheel.agrprefab", w));
    CHECK(prefab::refreshAll(again, "assets://wheel.agrprefab", previousWheel) == 4);  // 2 cars x 2 wheels
    const Entity rcar2 = again.findByUuid(f.scene.uuidOf(car2));
    CHECK(again.registry().get<ArmorComponent>(f.child(again, rcar2, "FrontWheel")).value == doctest::Approx(6.0f));
    CHECK(again.registry().get<ArmorComponent>(f.child(again, rcar2, "RearWheel")).value == doctest::Approx(7.0f));
}

TEST_CASE("copy / paste and duplicate make independent instances") {
    Fixture f;
    const Entity e = f.enemy(f.scene);
    f.makePrefab(f.scene, e, "assets://enemy.agrprefab");
    f.scene.registry().get<AimComponent>(f.child(f.scene, e, "Gun")).damage = 6.0f;
    f.scene.createEntity("Extra", e);

    const Entity dup = f.scene.duplicate(e);
    REQUIRE(f.scene.valid(dup));
    CHECK(prefab::isInstanceRoot(f.scene, dup));
    const Entity dupGun = f.child(f.scene, dup, "Gun");
    REQUIRE(f.scene.valid(dupGun));
    CHECK(f.scene.uuidOf(dupGun) != f.scene.uuidOf(f.child(f.scene, e, "Gun")));
    CHECK(f.scene.registry().get<AimComponent>(dupGun).damage == doctest::Approx(6.0f));
    CHECK(f.scene.registry().get<AimComponent>(dupGun).target == f.child(f.scene, dup, "Body"));  // its own body
    CHECK(f.scene.valid(f.child(f.scene, dup, "Extra")));
    CHECK(prefab::overrideCount(f.scene, dup) == prefab::overrideCount(f.scene, e));

    auto pasted = SceneSerializer::instantiate(f.scene, SceneSerializer::entityToJson(f.scene, e));
    REQUIRE(pasted);
    CHECK(f.scene.registry().get<AimComponent>(f.child(f.scene, pasted.value(), "Gun")).target == f.child(f.scene, pasted.value(), "Body"));
    Scene loaded;
    f.reload(f.scene, loaded);
    usize instances = 0;
    loaded.registry().view<PrefabInstanceComponent>().each([&](Entity, PrefabInstanceComponent&) { ++instances; });
    CHECK(instances == 3);
}

TEST_CASE("revert, unpack, missing prefab placeholder, self-containing prefab") {
    Fixture f;
    const Entity e = f.enemy(f.scene);
    f.makePrefab(f.scene, e, "assets://enemy.agrprefab");
    auto& reg = f.scene.registry();
    reg.get<TransformComponent>(e).position = {3, 0, 0};
    reg.get<AimComponent>(f.child(f.scene, e, "Gun")).damage = 9.0f;
    REQUIRE(prefab::revert(f.scene, e));
    const Entity reverted = f.scene.findByName("Enemy");
    CHECK(reg.get<AimComponent>(f.child(f.scene, reverted, "Gun")).damage == doctest::Approx(1.0f));
    CHECK(reg.get<TransformComponent>(reverted).position.x == doctest::Approx(3.0f));  // the place is kept

    // Missing prefab: a placeholder keeps the saved entry, written back unchanged.
    json saved = SceneSerializer::toJson(f.scene);
    saved["entities"][0]["prefab"] = "assets://gone.agrprefab";
    Scene missing;
    REQUIRE(SceneSerializer::fromJson(missing, saved));
    const Entity ph = missing.roots().front();
    CHECK(missing.registry().get<TransformComponent>(ph).position.x == doctest::Approx(3.0f));
    const json resaved = SceneSerializer::toJson(missing);
    CHECK(resaved["entities"][0]["prefab"] == "assets://gone.agrprefab");
    CHECK(resaved["entities"][0]["overrides"] == saved["entities"][0]["overrides"]);

    // Unpack: plain entities again.
    prefab::unpack(f.scene, reverted);
    CHECK(SceneSerializer::toJson(f.scene)["entities"].size() == 3);

    // A prefab that contains an instance of itself fails cleanly.
    const Entity loop = f.scene.createEntity("Loop");
    f.makePrefab(f.scene, loop, "assets://loop.agrprefab");
    json content = *f.library.get("assets://loop.agrprefab");
    content["entities"].push_back({{"id", Uuid::generate().toString()}, {"parent", content["entities"][0]["id"]}, {"prefab", "assets://loop.agrprefab"}});
    REQUIRE(f.library.save("assets://loop.agrprefab", content));
    auto r = prefab::instantiate(f.scene, "assets://loop.agrprefab");
    REQUIRE(r);  // the self-reference becomes a placeholder; the rest is created
    usize placeholders = 0;
    f.scene.registry().view<PrefabInstanceComponent>().each([&](Entity, PrefabInstanceComponent& p) { placeholders += !p.unresolved.empty(); });
    CHECK(placeholders == 1);
}

}
