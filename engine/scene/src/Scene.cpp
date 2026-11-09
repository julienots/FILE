#include "agr/scene/Scene.hpp"

#include <algorithm>

#include "agr/core/Log.hpp"
#include "agr/scene/Reflection.hpp"

namespace agr {

using ecs::Entity;

Scene::Scene(std::string name) : m_name(std::move(name)) {
    registerSceneComponents();
    // Make sure core pools exist so const lookups never return null.
    m_hierarchy = &m_registry.pool<HierarchyComponent>();
    m_registry.pool<IdComponent>();
    m_registry.pool<NameComponent>();
}

Entity Scene::createEntity(std::string_view name, Entity parent, Uuid id) {
    if (id.isNull()) id = Uuid::generate();
    if (m_uuidMap.contains(id)) {
        AGR_LOG_WARN("Scene", "duplicate entity id {} - generating a new one", id.toString());
        id = Uuid::generate();
    }
    const Entity e = m_registry.create();
    m_registry.emplace<IdComponent>(e, id);
    m_registry.emplace<NameComponent>(e, std::string(name));
    m_registry.emplace<StateComponent>(e);
    m_registry.emplace<TransformComponent>(e);
    m_registry.emplace<WorldTransformComponent>(e);
    m_registry.emplace<HierarchyComponent>(e);
    m_uuidMap.emplace(id, e);
    link(e, valid(parent) ? parent : Entity{}, Entity{});
    return e;
}

void Scene::destroyEntity(Entity e) {
    if (!valid(e)) return;
    unlink(e);
    destroyRecursive(e);
}

void Scene::destroyRecursive(Entity e) {
    // Collect first: destroying mutates the sibling links we walk.
    std::vector<Entity> kids = children(e);
    for (Entity c : kids) destroyRecursive(c);
    m_uuidMap.erase(uuidOf(e));
    m_registry.destroy(e);
}

void Scene::destroyDeferred(Entity e) {
    if (valid(e)) m_pendingDestroy.push_back(e);
}

void Scene::flushDestroyed() {
    auto pending = std::move(m_pendingDestroy);
    m_pendingDestroy.clear();
    for (Entity e : pending) destroyEntity(e);  // tolerant to already-destroyed descendants
}

// ------------------------------------------------------------------ hierarchy
void Scene::unlink(Entity e) {
    auto& h = hierarchy(e);
    if (!h.prevSibling.isNull()) hierarchy(h.prevSibling).nextSibling = h.nextSibling;
    if (!h.nextSibling.isNull()) hierarchy(h.nextSibling).prevSibling = h.prevSibling;
    if (h.parent.isNull()) {
        if (m_firstRoot == e) m_firstRoot = h.nextSibling;
        if (m_lastRoot == e) m_lastRoot = h.prevSibling;
    } else {
        auto& p = hierarchy(h.parent);
        if (p.firstChild == e) p.firstChild = h.nextSibling;
        if (p.lastChild == e) p.lastChild = h.prevSibling;
        --p.childCount;
    }
    h.parent = h.prevSibling = h.nextSibling = Entity{};
}

void Scene::link(Entity e, Entity parent, Entity before) {
    auto& h = hierarchy(e);
    h.parent = parent;
    Entity* first;
    Entity* last;
    if (parent.isNull()) {
        first = &m_firstRoot;
        last = &m_lastRoot;
    } else {
        auto& p = hierarchy(parent);
        first = &p.firstChild;
        last = &p.lastChild;
        ++p.childCount;
    }
    if (before.isNull() || !valid(before) || hierarchy(before).parent != parent) {
        h.prevSibling = *last;
        h.nextSibling = Entity{};
        if (!last->isNull()) hierarchy(*last).nextSibling = e;
        *last = e;
        if (first->isNull()) *first = e;
    } else {
        auto& b = hierarchy(before);
        h.nextSibling = before;
        h.prevSibling = b.prevSibling;
        if (!b.prevSibling.isNull()) hierarchy(b.prevSibling).nextSibling = e;
        else *first = e;
        b.prevSibling = e;
    }
}

bool Scene::setParent(Entity child, Entity newParent, Entity before, bool keepWorldTransform) {
    if (!valid(child)) return false;
    if (!newParent.isNull() && (!valid(newParent) || newParent == child || isAncestorOf(child, newParent))) {
        AGR_LOG_WARN("Scene", "setParent rejected: would create a cycle or parent is invalid");
        return false;
    }
    if (before == child) return true;
    math::Mat4 world;
    if (keepWorldTransform) world = worldMatrix(child);
    unlink(child);
    link(child, newParent, before);
    if (keepWorldTransform) setWorldMatrix(child, world);
    return true;
}

Entity Scene::parent(Entity e) const { return valid(e) ? hierarchy(e).parent : Entity{}; }

std::vector<Entity> Scene::children(Entity e) const {
    std::vector<Entity> out;
    if (!valid(e)) return out;
    out.reserve(hierarchy(e).childCount);
    for (Entity c = hierarchy(e).firstChild; !c.isNull(); c = hierarchy(c).nextSibling) out.push_back(c);
    return out;
}

std::vector<Entity> Scene::roots() const {
    std::vector<Entity> out;
    for (Entity r = m_firstRoot; !r.isNull(); r = hierarchy(r).nextSibling) out.push_back(r);
    return out;
}

bool Scene::isAncestorOf(Entity ancestor, Entity e) const {
    for (Entity p = parent(e); !p.isNull(); p = parent(p))
        if (p == ancestor) return true;
    return false;
}

bool Scene::isActiveInHierarchy(Entity e) const {
    for (Entity cur = e; !cur.isNull(); cur = parent(cur)) {
        const auto* s = m_registry.tryGet<StateComponent>(cur);
        if (s && !s->active) return false;
    }
    return valid(e);
}

// ----------------------------------------------------------------- duplicate
Entity Scene::duplicate(Entity e) {
    if (!valid(e)) return {};
    return duplicateRecursive(e, parent(e), hierarchy(e).nextSibling);
}

Entity Scene::duplicateRecursive(Entity src, Entity newParent, Entity before) {
    const Entity dst = createEntity(nameOf(src), Entity{});
    // Copy every component except identity / hierarchy bookkeeping.
    const auto skipId = ecs::componentTypeId<IdComponent>();
    const auto skipHierarchy = ecs::componentTypeId<HierarchyComponent>();
    std::vector<ecs::PoolBase*> pools;
    m_registry.eachPool([&](ecs::PoolBase& p) { pools.push_back(&p); });
    for (ecs::PoolBase* p : pools) {
        if (p->typeId() == skipId || p->typeId() == skipHierarchy) continue;
        if (p->contains(src) && !p->copy(m_registry, src, dst))
            AGR_LOG_WARN("Scene", "component {:#x} is not copyable; skipped during duplicate", p->typeId());
    }
    unlink(dst);
    link(dst, newParent, before);
    for (Entity c : children(src)) duplicateRecursive(c, dst, Entity{});
    return dst;
}

// ---------------------------------------------------------------- transforms
void Scene::updateTransforms() {
    for (Entity r = m_firstRoot; !r.isNull(); r = hierarchy(r).nextSibling) updateTransformRecursive(r, math::Mat4::identity());
}

void Scene::updateTransformRecursive(Entity e, const math::Mat4& parentWorld) {
    const auto& t = m_registry.get<TransformComponent>(e);
    auto& w = m_registry.get<WorldTransformComponent>(e);
    w.matrix = parentWorld * t.localMatrix();
    for (Entity c = hierarchy(e).firstChild; !c.isNull(); c = hierarchy(c).nextSibling) updateTransformRecursive(c, w.matrix);
}

math::Mat4 Scene::worldMatrix(Entity e) const {
    math::Mat4 m = math::Mat4::identity();
    for (Entity cur = e; valid(cur); cur = parent(cur)) m = m_registry.get<TransformComponent>(cur).localMatrix() * m;
    return m;
}

void Scene::setWorldMatrix(Entity e, const math::Mat4& world) {
    if (!valid(e)) return;
    const Entity p = parent(e);
    const math::Mat4 local = p.isNull() ? world : math::inverse(worldMatrix(p)) * world;
    auto& t = m_registry.get<TransformComponent>(e);
    math::decompose(local, t.position, t.rotation, t.scale);
}

// -------------------------------------------------------------------- lookup
Entity Scene::findByUuid(Uuid id) const {
    auto it = m_uuidMap.find(id);
    return it == m_uuidMap.end() ? Entity{} : it->second;
}

Entity Scene::findByName(std::string_view name) const {
    Entity found{};
    traverse([&](Entity e, u32) {
        if (found.isNull() && m_registry.get<NameComponent>(e).name == name) found = e;
    });
    return found;
}

Uuid Scene::uuidOf(Entity e) const {
    const auto* id = m_registry.tryGet<IdComponent>(e);
    return id ? id->id : Uuid{};
}

const std::string& Scene::nameOf(Entity e) const {
    static const std::string empty;
    const auto* n = m_registry.tryGet<NameComponent>(e);
    return n ? n->name : empty;
}

void Scene::clear() {
    m_registry.clear();
    m_firstRoot = m_lastRoot = Entity{};
    m_uuidMap.clear();
    m_pendingDestroy.clear();
}

} // namespace agr
