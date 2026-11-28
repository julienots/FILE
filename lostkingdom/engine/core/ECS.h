// AGR Engine — Entity Component System.
// Sparse-set storage: each component type lives in a dense, chunked array (std::deque keeps
// references stable when systems spawn entities while iterating);
// entities are 32-bit ids with a generation counter so stale handles are detected.
#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <cassert>
#include <functional>
#include <deque>

namespace agr {

using Entity = uint32_t;  // [generation:12 | index:20]
constexpr Entity NullEntity = 0xffffffffu;
inline uint32_t entityIndex(Entity e) { return e & 0xfffffu; }
inline uint32_t entityGen(Entity e) { return e >> 20; }

struct IPool {
    virtual ~IPool() = default;
    virtual void remove(Entity e) = 0;
    virtual bool has(Entity e) const = 0;
    virtual size_t size() const = 0;
};

template <typename T>
class Pool final : public IPool {
public:
    std::vector<uint32_t> sparse;  // entity index -> dense slot + 1 (0 = absent)
    std::vector<Entity> dense;     // dense slot -> entity
    std::deque<T> data;   // deque: references stay valid when components are added during iteration

    T& add(Entity e, T value) {
        uint32_t i = entityIndex(e);
        if (i >= sparse.size()) sparse.resize(i + 1 + i / 2, 0);
        if (sparse[i]) { data[sparse[i] - 1] = std::move(value); return data[sparse[i] - 1]; }
        dense.push_back(e); data.push_back(std::move(value));
        sparse[i] = (uint32_t)dense.size();
        return data.back();
    }
    void remove(Entity e) override {
        uint32_t i = entityIndex(e);
        if (i >= sparse.size() || !sparse[i] || dense[sparse[i] - 1] != e) return;
        uint32_t slot = sparse[i] - 1, last = (uint32_t)dense.size() - 1;
        if (slot != last) {
            dense[slot] = dense[last];
            data[slot] = std::move(data[last]);
            sparse[entityIndex(dense[slot])] = slot + 1;
        }
        dense.pop_back(); data.pop_back(); sparse[i] = 0;
    }
    bool has(Entity e) const override {
        uint32_t i = entityIndex(e);
        return i < sparse.size() && sparse[i] && dense[sparse[i] - 1] == e;
    }
    T* get(Entity e) { return has(e) ? &data[sparse[entityIndex(e)] - 1] : nullptr; }
    size_t size() const override { return dense.size(); }
};

class Registry {
public:
    Entity create() {
        uint32_t idx;
        if (!freeList.empty()) { idx = freeList.back(); freeList.pop_back(); }
        else { idx = (uint32_t)gens.size(); gens.push_back(0); }
        alive++;
        return (gens[idx] << 20) | idx;
    }
    bool valid(Entity e) const {
        if (e == NullEntity) return false;
        uint32_t i = entityIndex(e);
        return i < gens.size() && gens[i] == entityGen(e) && !deadFlag(i);
    }
    void destroy(Entity e) {
        if (!valid(e)) return;
        for (auto& p : pools) if (p) p->remove(e);
        uint32_t i = entityIndex(e);
        gens[i] = (gens[i] + 1) & 0xfff;
        freeList.push_back(i);
        alive--;
    }
    // Deferred destruction, safe to call while iterating a view.
    void destroyLater(Entity e) { pending.push_back(e); }
    void flush() { for (Entity e : pending) destroy(e); pending.clear(); }

    template <typename T> T& add(Entity e, T v = T{}) { return pool<T>().add(e, std::move(v)); }
    template <typename T> T* get(Entity e) { return pool<T>().get(e); }
    template <typename T> bool has(Entity e) { return pool<T>().has(e); }
    template <typename T> void remove(Entity e) { pool<T>().remove(e); }

    template <typename T> Pool<T>& pool() {
        size_t id = typeId<T>();
        if (id >= pools.size()) pools.resize(id + 1);
        if (!pools[id]) pools[id] = std::make_unique<Pool<T>>();
        return *static_cast<Pool<T>*>(pools[id].get());
    }

    // Iterate all entities owning A (and optionally others). Iterates the first pool; filters by the rest.
    template <typename A, typename... Rest, typename F>
    void each(F&& fn) {
        Pool<A>& pa = pool<A>();
        // iterate backwards so removals of the current element during iteration are safe
        for (size_t i = pa.dense.size(); i-- > 0;) {
            if (i >= pa.dense.size()) continue;
            Entity e = pa.dense[i];
            if ((pool<Rest>().has(e) && ...)) fn(e, pa.data[i], *pool<Rest>().get(e)...);
        }
    }
    size_t count() const { return alive; }
    void clear() {
        for (auto& p : pools) p.reset();
        gens.clear(); freeList.clear(); pending.clear(); alive = 0;
    }

private:
    static size_t nextTypeId() { static size_t n = 0; return n++; }
    template <typename T> static size_t typeId() { static size_t id = nextTypeId(); return id; }
    bool deadFlag(uint32_t) const { return false; }
    std::vector<std::unique_ptr<IPool>> pools;
    std::vector<uint32_t> gens;
    std::vector<uint32_t> freeList;
    std::vector<Entity> pending;
    size_t alive = 0;
};

}  // namespace agr
