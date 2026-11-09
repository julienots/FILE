#pragma once
#include <optional>
#include <vector>

#include "agr/core/Assert.hpp"
#include "agr/core/Types.hpp"

namespace agr {

/// Generational handle: safe weak reference to a resource stored in a HandlePool.
/// A handle becomes invalid as soon as its slot is released, even if reused.
template <typename Tag>
struct Handle {
    u32 index = 0xFFFFFFFFu;
    u32 generation = 0;

    constexpr bool isNull() const { return index == 0xFFFFFFFFu; }
    constexpr explicit operator bool() const { return !isNull(); }
    constexpr auto operator<=>(const Handle&) const = default;
};

/// Dense-ish storage addressed by generational handles. Used for GPU resources,
/// assets, audio voices... Slots are recycled through a free list.
template <typename T, typename Tag = T>
class HandlePool {
public:
    using HandleType = Handle<Tag>;

    template <typename... Args>
    HandleType create(Args&&... args) {
        u32 index;
        if (!m_free.empty()) {
            index = m_free.back();
            m_free.pop_back();
        } else {
            index = static_cast<u32>(m_slots.size());
            m_slots.emplace_back();
        }
        Slot& slot = m_slots[index];
        slot.value.emplace(std::forward<Args>(args)...);
        ++m_size;
        return HandleType{index, slot.generation};
    }

    bool valid(HandleType h) const {
        return h.index < m_slots.size() && m_slots[h.index].generation == h.generation && m_slots[h.index].value.has_value();
    }

    T* get(HandleType h) { return valid(h) ? &*m_slots[h.index].value : nullptr; }
    const T* get(HandleType h) const { return valid(h) ? &*m_slots[h.index].value : nullptr; }

    bool release(HandleType h) {
        if (!valid(h)) return false;
        Slot& slot = m_slots[h.index];
        slot.value.reset();
        ++slot.generation;
        m_free.push_back(h.index);
        --m_size;
        return true;
    }

    usize size() const { return m_size; }
    usize capacity() const { return m_slots.size(); }

    template <typename Fn>
    void forEach(Fn&& fn) {
        for (u32 i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].value) fn(HandleType{i, m_slots[i].generation}, *m_slots[i].value);
        }
    }

private:
    struct Slot {
        std::optional<T> value;
        u32 generation = 1;
    };
    std::vector<Slot> m_slots;
    std::vector<u32> m_free;
    usize m_size = 0;
};

} // namespace agr
