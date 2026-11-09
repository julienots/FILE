#pragma once
// AGR ECS: sparse-set entity component system.
//
//  * Components are plain structs stored contiguously per type (cache friendly).
//  * add / remove / has are O(1); iteration walks the smallest pool of a view.
//  * Component type ids are derived from the type *name* hash, so the same
//    component has the same id in the host executable and in plugin libraries.
#include <algorithm>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "agr/core/Assert.hpp"
#include "agr/core/StringId.hpp"
#include "agr/ecs/Entity.hpp"

namespace agr::ecs {

class Registry;
using ComponentTypeId = u64;

template <typename T>
constexpr ComponentTypeId componentTypeId() { return typeHash<std::remove_cvref_t<T>>(); }

/// Type-erased view of a component pool (used by reflection, editor, serializer).
class PoolBase {
public:
    virtual ~PoolBase() = default;
    virtual ComponentTypeId typeId() const = 0;
    virtual bool contains(Entity e) const = 0;
    virtual void remove(Registry& r, Entity e) = 0;
    virtual void* getRaw(Entity e) = 0;
    /// Default-constructs the component; returns nullptr if not default constructible.
    virtual void* emplaceDefault(Registry& r, Entity e) = 0;
    /// Copies component from `src` to `dst` (overwrites). False if not copyable / missing.
    virtual bool copy(Registry& r, Entity src, Entity dst) = 0;
    virtual usize size() const = 0;
    virtual const std::vector<Entity>& entities() const = 0;
    virtual void clear(Registry& r) = 0;

    using Hook = std::function<void(Registry&, Entity)>;
    void onConstruct(Hook h) { m_onConstruct.push_back(std::move(h)); }
    void onDestroy(Hook h) { m_onDestroy.push_back(std::move(h)); }

protected:
    std::vector<Hook> m_onConstruct;
    std::vector<Hook> m_onDestroy;
};

template <typename T>
class Pool final : public PoolBase {
public:
    static constexpr u32 kAbsent = 0xFFFFFFFFu;

    ComponentTypeId typeId() const override { return componentTypeId<T>(); }

    bool contains(Entity e) const override {
        return e.index < m_sparse.size() && m_sparse[e.index] != kAbsent && m_dense[m_sparse[e.index]] == e;
    }

    template <typename... Args>
    T& emplace(Registry& r, Entity e, Args&&... args) {
        AGR_ASSERT(!contains(e), "component already present");
        if (e.index >= m_sparse.size()) m_sparse.resize(std::max<usize>(e.index + 1, m_sparse.size() * 2), kAbsent);
        m_sparse[e.index] = static_cast<u32>(m_dense.size());
        m_dense.push_back(e);
        if constexpr (std::is_aggregate_v<T>) {
            m_data.push_back(T{std::forward<Args>(args)...});
        } else {
            m_data.emplace_back(std::forward<Args>(args)...);
        }
        for (auto& h : m_onConstruct) h(r, e);
        // Hooks may have added components to this pool and reallocated m_data.
        return m_data[m_sparse[e.index]];
    }

    T& get(Entity e) {
        AGR_ASSERT(contains(e), "component missing");
        return m_data[m_sparse[e.index]];
    }
    T* tryGet(Entity e) { return contains(e) ? &m_data[m_sparse[e.index]] : nullptr; }

    void remove(Registry& r, Entity e) override {
        if (!contains(e)) return;
        for (auto& h : m_onDestroy) h(r, e);
        if (!contains(e)) return;  // a hook removed it already
        // Swap-and-pop keeps storage dense.
        const u32 idx = m_sparse[e.index];
        const u32 last = static_cast<u32>(m_dense.size() - 1);
        if (idx != last) {
            m_dense[idx] = m_dense[last];
            m_data[idx] = std::move(m_data[last]);
            m_sparse[m_dense[idx].index] = idx;
        }
        m_dense.pop_back();
        m_data.pop_back();
        m_sparse[e.index] = kAbsent;
    }

    void* getRaw(Entity e) override { return tryGet(e); }

    void* emplaceDefault(Registry& r, Entity e) override {
        if constexpr (std::is_default_constructible_v<T>) {
            if (contains(e)) return tryGet(e);
            return &emplace(r, e);
        } else {
            return nullptr;
        }
    }

    bool copy(Registry& r, Entity src, Entity dst) override {
        if constexpr (std::is_copy_constructible_v<T>) {
            if (!contains(src)) return false;
            T value = get(src);
            if (contains(dst)) get(dst) = std::move(value);
            else emplace(r, dst, std::move(value));
            return true;
        } else {
            return false;
        }
    }

    usize size() const override { return m_dense.size(); }
    const std::vector<Entity>& entities() const override { return m_dense; }
    std::vector<T>& data() { return m_data; }

    void clear(Registry& r) override {
        while (!m_dense.empty()) remove(r, m_dense.back());
    }

private:
    std::vector<u32> m_sparse;
    std::vector<Entity> m_dense;
    std::vector<T> m_data;
};

template <typename... Ts>
class View;

class Registry {
public:
    Registry() = default;
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    ~Registry() { clear(); }

    // ------------------------------------------------------------ entities
    Entity create() {
        if (!m_free.empty()) {
            const u32 index = m_free.back();
            m_free.pop_back();
            m_alive[index] = 1;
            ++m_aliveCount;
            return Entity{index, m_versions[index]};
        }
        const u32 index = static_cast<u32>(m_versions.size());
        AGR_VERIFY(index != Entity::kNullIndex, "entity limit reached");
        m_versions.push_back(0);
        m_alive.push_back(1);
        ++m_aliveCount;
        return Entity{index, 0};
    }

    bool valid(Entity e) const {
        return e.index < m_versions.size() && m_alive[e.index] && m_versions[e.index] == e.version;
    }

    void destroy(Entity e) {
        if (!valid(e)) return;
        for (auto& [id, pool] : m_pools) pool->remove(*this, e);
        m_alive[e.index] = 0;
        ++m_versions[e.index];
        m_free.push_back(e.index);
        --m_aliveCount;
    }

    usize aliveCount() const { return m_aliveCount; }

    template <typename Fn>
    void eachEntity(Fn&& fn) const {
        for (u32 i = 0; i < m_versions.size(); ++i)
            if (m_alive[i]) fn(Entity{i, m_versions[i]});
    }

    /// Destroys every entity and every component.
    void clear() {
        for (auto& [id, pool] : m_pools) pool->clear(*this);
        m_versions.clear();
        m_alive.clear();
        m_free.clear();
        m_aliveCount = 0;
    }

    // ---------------------------------------------------------- components
    template <typename T, typename... Args>
    T& emplace(Entity e, Args&&... args) {
        AGR_ASSERT(valid(e), "emplace on invalid entity");
        return pool<T>().emplace(*this, e, std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    T& emplaceOrReplace(Entity e, Args&&... args) {
        auto& p = pool<T>();
        if (T* existing = p.tryGet(e)) {
            *existing = T{std::forward<Args>(args)...};
            return *existing;
        }
        return p.emplace(*this, e, std::forward<Args>(args)...);
    }

    template <typename T>
    T& get(Entity e) { return pool<T>().get(e); }

    template <typename T>
    T* tryGet(Entity e) {
        auto* p = findPool<T>();
        return p ? p->tryGet(e) : nullptr;
    }

    template <typename... Ts>
    bool has(Entity e) const {
        return (hasOne<Ts>(e) && ...);
    }

    template <typename T>
    void remove(Entity e) {
        if (auto* p = findPool<T>()) p->remove(*this, e);
    }

    template <typename... Ts>
    View<Ts...> view();

    // ---------------------------------------------------------------- pools
    template <typename T>
    Pool<T>& pool() {
        const ComponentTypeId id = componentTypeId<T>();
        auto it = m_pools.find(id);
        if (it == m_pools.end()) it = m_pools.emplace(id, std::make_unique<Pool<T>>()).first;
        return static_cast<Pool<T>&>(*it->second);
    }

    template <typename T>
    Pool<T>* findPool() const {
        auto it = m_pools.find(componentTypeId<T>());
        return it == m_pools.end() ? nullptr : static_cast<Pool<T>*>(it->second.get());
    }

    PoolBase* findPool(ComponentTypeId id) const {
        auto it = m_pools.find(id);
        return it == m_pools.end() ? nullptr : it->second.get();
    }

    /// Registers a pool created elsewhere (reflection registers pools by id).
    PoolBase& ensurePool(ComponentTypeId id, const std::function<std::unique_ptr<PoolBase>()>& factory) {
        auto it = m_pools.find(id);
        if (it == m_pools.end()) it = m_pools.emplace(id, factory()).first;
        return *it->second;
    }

    /// Clears and deletes a pool. Required before unloading a plugin that
    /// defined the component type (its code lives in the plugin library).
    void erasePool(ComponentTypeId id) {
        auto it = m_pools.find(id);
        if (it == m_pools.end()) return;
        it->second->clear(*this);
        m_pools.erase(it);
    }

    template <typename Fn>
    void eachPool(Fn&& fn) {
        for (auto& [id, p] : m_pools) fn(*p);
    }

private:
    template <typename T>
    bool hasOne(Entity e) const {
        auto* p = findPool<T>();
        return p && p->contains(e);
    }

    std::vector<u32> m_versions;
    std::vector<u8> m_alive;
    std::vector<u32> m_free;
    usize m_aliveCount = 0;
    std::unordered_map<ComponentTypeId, std::unique_ptr<PoolBase>> m_pools;
};

/// Iterates entities owning every component in Ts. Driven by the smallest pool.
/// Adding/removing components of the viewed types during iteration is not
/// allowed (except removing the current entity's components after use);
/// collect entities first or use Scene::destroyDeferred().
template <typename... Ts>
class View {
public:
    explicit View(Registry& r) : m_pools{&r.pool<Ts>()...} {
        m_driver = nullptr;
        usize best = static_cast<usize>(-1);
        std::apply(
            [&](auto*... p) {
                ((p->size() < best ? (best = p->size(), m_driver = static_cast<PoolBase*>(p)) : nullptr), ...);
            },
            m_pools);
    }

    template <typename Fn>
    void each(Fn&& fn) {
        if (!m_driver) return;
        const auto& ents = m_driver->entities();
        // Iterate backwards so that removing the current entity is safe.
        for (usize i = ents.size(); i-- > 0;) {
            const Entity e = ents[i];
            if (!std::apply([&](auto*... p) { return (p->contains(e) && ...); }, m_pools)) continue;
            std::apply([&](auto*... p) { fn(e, p->get(e)...); }, m_pools);
            if (i > ents.size()) i = ents.size();
        }
    }

    usize sizeHint() const { return m_driver ? m_driver->size() : 0; }

    std::vector<Entity> entities() {
        std::vector<Entity> out;
        each([&](Entity e, Ts&...) { out.push_back(e); });
        std::reverse(out.begin(), out.end());
        return out;
    }

private:
    std::tuple<Pool<Ts>*...> m_pools;
    PoolBase* m_driver;
};

template <typename... Ts>
View<Ts...> Registry::view() {
    return View<Ts...>(*this);
}

} // namespace agr::ecs
