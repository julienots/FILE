#include <doctest/doctest.h>

#include <string>

#include "agr/ecs/Registry.hpp"

using namespace agr;
using namespace agr::ecs;

namespace {
struct Position { float x = 0, y = 0; };
struct Velocity { float x = 0, y = 0; };
struct Tag {};
struct Named {
    std::string name;
    explicit Named(std::string n) : name(std::move(n)) {}
};
}  // namespace

TEST_SUITE("ecs") {

TEST_CASE("entities are recycled with a new version") {
    Registry r;
    const Entity a = r.create();
    CHECK(r.valid(a));
    r.destroy(a);
    CHECK_FALSE(r.valid(a));
    const Entity b = r.create();
    CHECK(b.index == a.index);
    CHECK(b.version == a.version + 1);
    CHECK_FALSE(r.valid(a));
    CHECK(r.aliveCount() == 1);
}

TEST_CASE("components can be added, read and removed") {
    Registry r;
    const Entity e = r.create();
    r.emplace<Position>(e, 1.0f, 2.0f);
    r.emplace<Named>(e, "hero");
    CHECK(r.has<Position, Named>(e));
    CHECK_FALSE(r.has<Velocity>(e));
    CHECK(r.get<Position>(e).y == 2.0f);
    CHECK(r.get<Named>(e).name == "hero");
    r.remove<Position>(e);
    CHECK_FALSE(r.has<Position>(e));
    CHECK(r.tryGet<Position>(e) == nullptr);
    r.emplaceOrReplace<Position>(e, 5.0f, 6.0f);
    r.emplaceOrReplace<Position>(e, 7.0f, 8.0f);
    CHECK(r.get<Position>(e).x == 7.0f);
    r.destroy(e);
    CHECK(r.pool<Named>().size() == 0);
}

TEST_CASE("swap-and-pop keeps other components intact") {
    Registry r;
    std::vector<Entity> es;
    for (int i = 0; i < 10; ++i) {
        es.push_back(r.create());
        r.emplace<Position>(es.back(), static_cast<float>(i), 0.0f);
    }
    r.destroy(es[2]);
    r.destroy(es[0]);
    for (int i = 0; i < 10; ++i) {
        if (i == 0 || i == 2) continue;
        CHECK(r.get<Position>(es[i]).x == static_cast<float>(i));
    }
}

TEST_CASE("views iterate only matching entities") {
    Registry r;
    for (int i = 0; i < 100; ++i) {
        const Entity e = r.create();
        r.emplace<Position>(e);
        if (i % 3 == 0) r.emplace<Velocity>(e, 1.0f, 0.0f);
        if (i % 2 == 0) r.emplace<Tag>(e);
    }
    int count = 0;
    r.view<Position, Velocity>().each([&](Entity, Position& p, Velocity& v) {
        p.x += v.x;
        ++count;
    });
    CHECK(count == 34);
    int both = 0;
    r.view<Velocity, Tag>().each([&](Entity, Velocity&, Tag&) { ++both; });
    CHECK(both == 17);  // multiples of 6 in [0, 100)
    CHECK(r.view<Position, Velocity>().entities().size() == 34);
}

TEST_CASE("removing the current entity during iteration is safe") {
    Registry r;
    for (int i = 0; i < 50; ++i) r.emplace<Position>(r.create(), static_cast<float>(i), 0.0f);
    int visited = 0;
    r.view<Position>().each([&](Entity e, Position& p) {
        ++visited;
        if (static_cast<int>(p.x) % 2 == 0) r.destroy(e);
    });
    CHECK(visited == 50);
    CHECK(r.pool<Position>().size() == 25);
}

TEST_CASE("construct / destroy hooks fire") {
    Registry r;
    int constructed = 0, destroyed = 0;
    r.pool<Velocity>().onConstruct([&](Registry&, Entity) { ++constructed; });
    r.pool<Velocity>().onDestroy([&](Registry& reg, Entity e) {
        CHECK(reg.has<Velocity>(e));  // still readable inside the hook
        ++destroyed;
    });
    const Entity e = r.create();
    r.emplace<Velocity>(e);
    r.destroy(e);
    CHECK(constructed == 1);
    CHECK(destroyed == 1);
}

TEST_CASE("type-erased pool operations") {
    Registry r;
    const Entity a = r.create(), b = r.create();
    r.emplace<Position>(a, 3.0f, 4.0f);
    PoolBase* pool = r.findPool(componentTypeId<Position>());
    REQUIRE(pool);
    CHECK(pool->copy(r, a, b));
    CHECK(r.get<Position>(b).y == 4.0f);
    CHECK(static_cast<Position*>(pool->getRaw(a))->x == 3.0f);
    const Entity c = r.create();
    CHECK(pool->emplaceDefault(r, c) != nullptr);
    CHECK(r.has<Position>(c));
    // Named has no default constructor.
    r.emplace<Named>(a, "x");
    CHECK(r.findPool(componentTypeId<Named>())->emplaceDefault(r, b) == nullptr);
    r.erasePool(componentTypeId<Position>());
    CHECK(r.findPool(componentTypeId<Position>()) == nullptr);
    CHECK(r.valid(a));
}

}  // TEST_SUITE
