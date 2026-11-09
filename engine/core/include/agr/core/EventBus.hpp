#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "agr/core/StringId.hpp"
#include "agr/core/Types.hpp"

namespace agr {

/// Opaque subscription token; pass it to EventBus::unsubscribe().
struct SubscriptionId {
    u64 value = 0;
    bool valid() const { return value != 0; }
};

/// Strongly typed publish / subscribe bus.
///  - publish(): synchronous dispatch on the calling thread.
///  - enqueue(): thread-safe, delivered on the next dispatchQueued() (main thread).
/// Subscribing / unsubscribing from inside a handler is safe.
class EventBus {
public:
    template <typename E, typename Fn>
    SubscriptionId subscribe(Fn&& fn) {
        const SubscriptionId id{++m_nextId};
        auto wrapper = std::make_shared<Handler>(Handler{id.value, [f = std::forward<Fn>(fn)](const void* e) {
                                                             f(*static_cast<const E*>(e));
                                                         }});
        m_handlers[typeHash<E>()].push_back(std::move(wrapper));
        return id;
    }

    void unsubscribe(SubscriptionId id) {
        for (auto& [type, list] : m_handlers) {
            std::erase_if(list, [&](const auto& h) { return h->id == id.value; });
        }
    }

    template <typename E>
    void publish(const E& event) {
        dispatchRaw(typeHash<E>(), &event);
    }

    template <typename E>
    void enqueue(E event) {
        auto holder = std::make_shared<E>(std::move(event));
        std::lock_guard lock(m_queueMutex);
        m_queue.push_back(Queued{typeHash<E>(), holder});
    }

    /// Delivers every queued event in FIFO order.
    void dispatchQueued() {
        std::vector<Queued> pending;
        {
            std::lock_guard lock(m_queueMutex);
            pending.swap(m_queue);
        }
        for (auto& q : pending) dispatchRaw(q.type, q.payload.get());
    }

    usize handlerCount() const {
        usize n = 0;
        for (auto& [type, list] : m_handlers) n += list.size();
        return n;
    }

private:
    struct Handler {
        u64 id;
        std::function<void(const void*)> fn;
    };
    struct Queued {
        u64 type;
        std::shared_ptr<void> payload;
    };

    void dispatchRaw(u64 type, const void* event) {
        auto it = m_handlers.find(type);
        if (it == m_handlers.end()) return;
        // Snapshot so handlers may (un)subscribe while we iterate.
        auto snapshot = it->second;
        for (auto& h : snapshot) h->fn(event);
    }

    std::unordered_map<u64, std::vector<std::shared_ptr<Handler>>> m_handlers;
    std::mutex m_queueMutex;
    std::vector<Queued> m_queue;
    u64 m_nextId = 0;
};

} // namespace agr
