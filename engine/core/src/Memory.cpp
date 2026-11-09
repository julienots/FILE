#include "agr/core/Memory.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

#include "agr/core/Assert.hpp"

namespace agr {
namespace {

struct TagCounters {
    std::atomic<usize> current{0};
    std::atomic<usize> peak{0};
};
std::array<TagCounters, static_cast<usize>(MemoryTag::Count)> g_counters;

constexpr usize kBlockAlignment = 16;

void* alignedAlloc(usize size, usize alignment) {
#if defined(_WIN32)
    return _aligned_malloc(size, alignment);
#else
    void* ptr = nullptr;
    if (posix_memalign(&ptr, std::max(alignment, sizeof(void*)), size) != 0) return nullptr;
    return ptr;
#endif
}

void alignedFree(void* ptr) {
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

// HeapAllocator header stored right before the returned pointer.
struct alignas(16) HeapHeader {
    usize size;
    usize offset;  // distance from the raw allocation to the user pointer
};

} // namespace

const char* enumName(MemoryTag tag) {
    static constexpr const char* names[] = {"General", "ECS",     "Scene",     "Assets", "Render",
                                            "Audio",   "Physics", "Scripting", "Editor"};
    return tag < MemoryTag::Count ? names[static_cast<usize>(tag)] : "?";
}

void MemoryStats::onAllocate(MemoryTag tag, usize bytes) {
    auto& c = g_counters[static_cast<usize>(tag)];
    const usize now = c.current.fetch_add(bytes, std::memory_order_relaxed) + bytes;
    usize peak = c.peak.load(std::memory_order_relaxed);
    while (now > peak && !c.peak.compare_exchange_weak(peak, now, std::memory_order_relaxed)) {
    }
}
void MemoryStats::onFree(MemoryTag tag, usize bytes) {
    g_counters[static_cast<usize>(tag)].current.fetch_sub(bytes, std::memory_order_relaxed);
}
usize MemoryStats::currentBytes(MemoryTag tag) { return g_counters[static_cast<usize>(tag)].current.load(); }
usize MemoryStats::peakBytes(MemoryTag tag) { return g_counters[static_cast<usize>(tag)].peak.load(); }
usize MemoryStats::totalCurrentBytes() {
    usize total = 0;
    for (auto& c : g_counters) total += c.current.load(std::memory_order_relaxed);
    return total;
}

// ------------------------------------------------------------- HeapAllocator
void* HeapAllocator::allocate(usize size, usize alignment) {
    alignment = std::max(alignment, alignof(HeapHeader));
    AGR_ASSERT(isPowerOfTwo(alignment), "alignment must be a power of two");
    const usize offset = alignUp(sizeof(HeapHeader), alignment);
    auto* raw = static_cast<std::byte*>(alignedAlloc(offset + size, alignment));
    if (!raw) return nullptr;
    auto* header = reinterpret_cast<HeapHeader*>(raw + offset - sizeof(HeapHeader));
    header->size = size;
    header->offset = offset;
    MemoryStats::onAllocate(m_tag, size);
    return raw + offset;
}

void HeapAllocator::deallocate(void* ptr) {
    if (!ptr) return;
    auto* user = static_cast<std::byte*>(ptr);
    auto* header = reinterpret_cast<HeapHeader*>(user - sizeof(HeapHeader));
    MemoryStats::onFree(m_tag, header->size);
    alignedFree(user - header->offset);
}

// ----------------------------------------------------------- LinearAllocator
LinearAllocator::LinearAllocator(usize capacity, MemoryTag tag) : m_capacity(capacity), m_tag(tag) {
    m_buffer = static_cast<std::byte*>(alignedAlloc(capacity, 64));
    AGR_VERIFY(m_buffer != nullptr, "LinearAllocator: out of memory");
    MemoryStats::onAllocate(m_tag, capacity);
}

LinearAllocator::~LinearAllocator() {
    alignedFree(m_buffer);
    MemoryStats::onFree(m_tag, m_capacity);
}

void* LinearAllocator::allocate(usize size, usize alignment) {
    AGR_ASSERT(isPowerOfTwo(alignment), "alignment must be a power of two");
    const auto base = reinterpret_cast<std::uintptr_t>(m_buffer);
    const usize aligned = alignUp(base + m_offset, alignment) - base;
    if (aligned + size > m_capacity) return nullptr;
    m_offset = aligned + size;
    m_highWater = std::max(m_highWater, m_offset);
    return m_buffer + aligned;
}

void LinearAllocator::rewind(Marker marker) {
    AGR_ASSERT(marker.offset <= m_offset, "rewind marker is ahead of the current offset");
    m_offset = marker.offset;
}

// ------------------------------------------------------------- PoolAllocator
PoolAllocator::PoolAllocator(usize blockSize, usize blocksPerChunk, MemoryTag tag)
    : m_blockSize(alignUp(std::max(blockSize, sizeof(FreeNode)), kBlockAlignment)),
      m_blocksPerChunk(std::max<usize>(blocksPerChunk, 1)),
      m_tag(tag) {}

PoolAllocator::~PoolAllocator() {
    AGR_ASSERT(m_live == 0, "PoolAllocator destroyed with live blocks (leak)");
    while (m_chunks) {
        Chunk* next = m_chunks->next;
        alignedFree(m_chunks);
        m_chunks = next;
    }
    MemoryStats::onFree(m_tag, m_chunkCount * (kBlockAlignment + m_blockSize * m_blocksPerChunk));
}

void PoolAllocator::addChunk() {
    const usize header = kBlockAlignment;  // Chunk header padded to block alignment.
    const usize bytes = header + m_blockSize * m_blocksPerChunk;
    auto* raw = static_cast<std::byte*>(alignedAlloc(bytes, kBlockAlignment));
    AGR_VERIFY(raw != nullptr, "PoolAllocator: out of memory");
    auto* chunk = reinterpret_cast<Chunk*>(raw);
    chunk->next = m_chunks;
    m_chunks = chunk;
    ++m_chunkCount;
    MemoryStats::onAllocate(m_tag, bytes);
    // Thread blocks onto the free list in address order.
    std::byte* blocks = raw + header;
    for (usize i = m_blocksPerChunk; i-- > 0;) {
        auto* node = reinterpret_cast<FreeNode*>(blocks + i * m_blockSize);
        node->next = m_freeList;
        m_freeList = node;
    }
}

void* PoolAllocator::allocate(usize size, usize alignment) {
    if (size > m_blockSize || alignment > kBlockAlignment) return nullptr;
    if (!m_freeList) addChunk();
    FreeNode* node = m_freeList;
    m_freeList = node->next;
    ++m_live;
    return node;
}

void PoolAllocator::deallocate(void* ptr) {
    if (!ptr) return;
    AGR_ASSERT(m_live > 0, "PoolAllocator: double free");
    auto* node = static_cast<FreeNode*>(ptr);
    node->next = m_freeList;
    m_freeList = node;
    --m_live;
}

// ------------------------------------------------------------ FrameAllocator
FrameAllocator::FrameAllocator(usize capacityPerFrame, MemoryTag tag)
    : m_a(capacityPerFrame, tag), m_b(capacityPerFrame, tag), m_frames{&m_a, &m_b} {}

void FrameAllocator::beginFrame() {
    m_index ^= 1u;
    m_frames[m_index]->reset();
}

} // namespace agr
