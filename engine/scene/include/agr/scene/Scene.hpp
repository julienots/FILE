#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "agr/ecs/Registry.hpp"
#include "agr/scene/Components.hpp"

namespace agr {

/// A scene is an ECS world plus a transform hierarchy and persistent ids.
/// It is the unit that is edited, saved, loaded and simulated.
class Scene {
public:
    using Entity = ecs::Entity;

    explicit Scene(std::string name = "Untitled");
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    const std::string& name() const { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }

    ecs::Registry& registry() { return m_registry; }

    // ------------------------------------------------------------- lifetime
    /// Creates an entity with Id, Name, State, Transform, WorldTransform and Hierarchy.
    Entity createEntity(std::string_view name = "Entity", Entity parent = {}, Uuid id = {});
    /// Destroys the entity and all its descendants immediately.
    void destroyEntity(Entity e);
    /// Marks for destruction at the next flushDestroyed() (safe during iteration).
    void destroyDeferred(Entity e);
    void flushDestroyed();
    bool valid(Entity e) const { return m_registry.valid(e); }
    usize entityCount() const { return m_registry.aliveCount(); }

    /// Deep copy (components + children) placed right after the source sibling.
    Entity duplicate(Entity e);

    // ------------------------------------------------------------ hierarchy
    /// Re-parents `child` under `parent` (null = root), inserted before `before`
    /// (null = last). Returns false if it would create a cycle.
    bool setParent(Entity child, Entity parent, Entity before = {}, bool keepWorldTransform = true);
    Entity parent(Entity e) const;
    std::vector<Entity> children(Entity e) const;
    std::vector<Entity> roots() const;
    bool isAncestorOf(Entity ancestor, Entity e) const;
    /// True when the entity and all its ancestors are active.
    bool isActiveInHierarchy(Entity e) const;

    template <typename Fn>
    void forEachChild(Entity e, Fn&& fn) const {
        const auto* h = m_hierarchy->tryGet(e);
        for (Entity c = h ? h->firstChild : Entity{}; !c.isNull();) {
            const Entity next = m_hierarchy->get(c).nextSibling;
            fn(c);
            c = next;
        }
    }

    /// Depth-first pre-order traversal (parents before children, sibling order).
    template <typename Fn>
    void traverse(Fn&& fn) const {
        for (Entity r = m_firstRoot; !r.isNull(); r = hierarchy(r).nextSibling) traverseFrom(r, 0, fn);
    }

    // ------------------------------------------------------------ transforms
    /// Recomputes every WorldTransformComponent (matrix + activeInHierarchy) from local data.
    void updateTransforms();
    math::Mat4 worldMatrix(Entity e) const;
    /// Sets the local transform such that the entity lands at `world`.
    void setWorldMatrix(Entity e, const math::Mat4& world);

    // --------------------------------------------------------------- lookup
    Entity findByUuid(Uuid id) const;
    Entity findByName(std::string_view name) const;
    Uuid uuidOf(Entity e) const;
    const std::string& nameOf(Entity e) const;

    /// Removes every entity.
    void clear();

private:
    template <typename Fn>
    void traverseFrom(Entity e, u32 depth, Fn& fn) const {
        fn(e, depth);
        for (Entity c = hierarchy(e).firstChild; !c.isNull(); c = hierarchy(c).nextSibling) traverseFrom(c, depth + 1, fn);
    }
    HierarchyComponent& hierarchy(Entity e) const { return m_hierarchy->get(e); }
    void unlink(Entity e);
    void link(Entity e, Entity parent, Entity before);
    void destroyRecursive(Entity e);
    Entity duplicateRecursive(Entity src, Entity parent, Entity before);
    void updateTransformRecursive(Entity e, const math::Mat4& parentWorld, bool parentActive);

    std::string m_name;
    mutable ecs::Registry m_registry;
    ecs::Pool<HierarchyComponent>* m_hierarchy = nullptr;  ///< Cached: hot in every traversal.
    Entity m_firstRoot{};
    Entity m_lastRoot{};
    std::unordered_map<Uuid, Entity> m_uuidMap;
    std::vector<Entity> m_pendingDestroy;
};

} // namespace agr
