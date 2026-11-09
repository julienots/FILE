#pragma once
#include <atomic>
#include <cstddef>
#include <new>
#include <utility>

#include "agr/core/Types.hpp"

namespace agr {

/// Global memory accounting per category, readable by the profiler.
enum class MemoryTag : u8 { General, Ecs, Scene, Assets, Render, Audio, Physics, Scripting, Editor, Count };
const char* enumName(MemoryTag tag);

struct MemoryStats {
    static void onAllocate(MemoryTag tag, usize bytes);
    static void onFree(MemoryTag tag, usize bytes);
    static usize currentBytes(MemoryTag tag);
    static usize peakBytes(MemoryTag tag);
    static usize totalCurrentBytes();
};

/// Polymorphic allocator interface used by engine containers that need custom memory.
class Allocator {
public:
    virtual ~Allocator() = default;
    virtual void* allocate(usize size, usize alignment = alignof(std::max_align_t)) = 0;
    virtual void deallocate(void* ptr) = 0;

    template <typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        return mem ? new (mem) T(std::forward<Args>(args)...) : nullptr;
    }
    template <typename T>
    void destroy(T* obj) {
        if (!obj) return;
        obj->~T();
        deallocate(obj);
    }
};

/// Heap allocator with 16-byte minimum alignment and tagged accounting.
class HeapAllocator final : public Allocator {
public:
    explicit HeapAllocator(MemoryTag tag = MemoryTag::General) : m_tag(tag) {}
    void* allocate(usize size, usize alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr) override;

private:
    MemoryTag m_tag;
};

/// Bump allocator: O(1) allocations, freed all at once with reset().
/// Ideal for per-frame scratch data and load-time temporary buffers.
class LinearAllocator final : public Allocator, NonCopyable {
public:
    explicit LinearAllocator(usize capacity, MemoryTag tag = MemoryTag::General);
    ~LinearAllocator() override;
    LinearAllocator(LinearAllocator&&) = delete;
    LinearAllocator& operator=(LinearAllocator&&) = delete;

    void* allocate(usize size, usize alignment = alignof(std::max_align_t)) override;
    void deallocate(void*) override {}  ///< Individual frees are no-ops.

    struct Marker { usize offset; };
    Marker mark() const { return {m_offset}; }
    void rewind(Marker marker);
    void reset() { m_offset = 0; }

    usize used() const { return m_offset; }
    usize capacity() const { return m_capacity; }
    usize highWater() const { return m_highWater; }

private:
    std::byte* m_buffer = nullptr;
    usize m_capacity = 0;
    usize m_offset = 0;
    usize m_highWater = 0;
    MemoryTag m_tag;
};

/// Fixed-size block allocator with an intrusive free list. O(1) alloc/free,
/// no fragmentation. Grows by whole chunks when exhausted.
class PoolAllocator final : public Allocator, NonCopyable {
public:
    PoolAllocator(usize blockSize, usize blocksPerChunk, MemoryTag tag = MemoryTag::General);
    ~PoolAllocator() override;
    PoolAllocator(PoolAllocator&&) = delete;
    PoolAllocator& operator=(PoolAllocator&&) = delete;

    /// `size` must be <= blockSize; alignment must be <= the block alignment (16).
    void* allocate(usize size, usize alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr) override;

    usize blockSize() const { return m_blockSize; }
    usize liveBlocks() const { return m_live; }
    usize chunkCount() const { return m_chunkCount; }

private:
    void addChunk();

    struct FreeNode { FreeNode* next; };
    struct Chunk { Chunk* next; };

    usize m_blockSize;
    usize m_blocksPerChunk;
    FreeNode* m_freeList = nullptr;
    Chunk* m_chunks = nullptr;
    usize m_chunkCount = 0;
    usize m_live = 0;
    MemoryTag m_tag;
};

/// Two linear allocators swapped every frame: data allocated in frame N stays
/// valid during frame N+1 (useful for render command lists consumed one frame late).
class FrameAllocator final : NonCopyable {
public:
    explicit FrameAllocator(usize capacityPerFrame, MemoryTag tag = MemoryTag::General);
    LinearAllocator& current() { return *m_frames[m_index]; }
    void beginFrame();

private:
    LinearAllocator m_a;
    LinearAllocator m_b;
    LinearAllocator* m_frames[2];
    u32 m_index = 0;
};

} // namespace agr
