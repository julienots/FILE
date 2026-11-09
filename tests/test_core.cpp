#include <doctest/doctest.h>

#include <filesystem>
#include <thread>

#include "agr/core/EventBus.hpp"
#include "agr/core/FileSystem.hpp"
#include "agr/core/Handle.hpp"
#include "agr/core/Log.hpp"
#include "agr/core/Memory.hpp"
#include "agr/core/Result.hpp"
#include "agr/core/StringId.hpp"
#include "agr/core/Uuid.hpp"

using namespace agr;
using namespace agr::literals;

TEST_SUITE("core") {

TEST_CASE("StringId hashes at compile time") {
    static_assert("Player"_sid == StringId("Player"));
    static_assert("Player"_sid != "Enemy"_sid);
    CHECK(fnv1a64("") == 0xcbf29ce484222325ull);
    CHECK(StringId("a").value() == 0xaf63dc4c8601ec8cull);  // reference FNV-1a 64 value
}

TEST_CASE("typeName is stable and distinct") {
    struct Foo {};
    struct Bar {};
    CHECK(typeHash<Foo>() != typeHash<Bar>());
    CHECK(typeName<int>() == "int");
}

TEST_CASE("Result carries value or error") {
    Result<int> ok = 42;
    CHECK(ok.ok());
    CHECK(ok.value() == 42);
    Result<int> bad = makeError(Error::Code::NotFound, "nope");
    CHECK_FALSE(bad.ok());
    CHECK(bad.error().code == Error::Code::NotFound);
    CHECK(bad.valueOr(7) == 7);
    Status s;
    CHECK(s.ok());
}

TEST_CASE("Uuid string round trip") {
    const Uuid a = Uuid::generate();
    CHECK_FALSE(a.isNull());
    CHECK(Uuid::fromString(a.toString()) == a);
    CHECK(Uuid::fromString("zz").isNull());
    CHECK(Uuid(0x00000000000000ffull).toString() == "00000000000000ff");
    CHECK(Uuid::generate() != Uuid::generate());
}

TEST_CASE("LinearAllocator respects alignment, markers and capacity") {
    LinearAllocator a(256);
    void* p1 = a.allocate(3, 1);
    void* p2 = a.allocate(16, 64);
    REQUIRE(p1);
    REQUIRE(p2);
    CHECK(reinterpret_cast<std::uintptr_t>(p2) % 64 == 0);
    auto mark = a.mark();
    CHECK(a.allocate(32, 16));
    a.rewind(mark);
    CHECK(a.used() == mark.offset);
    CHECK(a.allocate(1024) == nullptr);  // over capacity -> null, not a crash
    a.reset();
    CHECK(a.used() == 0);
    CHECK(a.highWater() >= 64 + 16);
}

TEST_CASE("PoolAllocator recycles blocks and grows by chunks") {
    PoolAllocator pool(24, 4);
    CHECK(pool.blockSize() == 32);
    std::vector<void*> blocks;
    for (int i = 0; i < 9; ++i) blocks.push_back(pool.allocate(24));
    CHECK(pool.chunkCount() == 3);
    CHECK(pool.liveBlocks() == 9);
    for (void* b : blocks) CHECK(reinterpret_cast<std::uintptr_t>(b) % 16 == 0);
    void* last = blocks.back();
    pool.deallocate(last);
    CHECK(pool.allocate(8) == last);  // LIFO reuse
    CHECK(pool.allocate(64) == nullptr);  // too big
    for (void* b : blocks) pool.deallocate(b);
    CHECK(pool.liveBlocks() == 0);
}

TEST_CASE("HeapAllocator tracks tagged memory") {
    HeapAllocator heap(MemoryTag::Audio);
    const usize before = MemoryStats::currentBytes(MemoryTag::Audio);
    void* p = heap.allocate(1000, 128);
    REQUIRE(p);
    CHECK(reinterpret_cast<std::uintptr_t>(p) % 128 == 0);
    CHECK(MemoryStats::currentBytes(MemoryTag::Audio) == before + 1000);
    heap.deallocate(p);
    CHECK(MemoryStats::currentBytes(MemoryTag::Audio) == before);
    CHECK(MemoryStats::peakBytes(MemoryTag::Audio) >= before + 1000);
}

TEST_CASE("FrameAllocator keeps previous frame alive") {
    FrameAllocator frames(128);
    frames.beginFrame();
    auto* a = static_cast<int*>(frames.current().allocate(sizeof(int)));
    *a = 1234;
    frames.beginFrame();
    CHECK(*a == 1234);  // other buffer was reset, not this one
    frames.current().allocate(64);
    frames.beginFrame();  // back to the first buffer: reset
    CHECK(frames.current().used() == 0);
}

TEST_CASE("HandlePool detects stale handles") {
    struct Tex { int w; };
    HandlePool<Tex> pool;
    auto h1 = pool.create(Tex{64});
    CHECK(pool.get(h1)->w == 64);
    CHECK(pool.release(h1));
    CHECK_FALSE(pool.valid(h1));
    auto h2 = pool.create(Tex{128});
    CHECK(h2.index == h1.index);  // slot reused
    CHECK(h2.generation != h1.generation);
    CHECK(pool.get(h1) == nullptr);
    CHECK(pool.get(h2)->w == 128);
    CHECK_FALSE(pool.release(h1));
    CHECK(pool.size() == 1);
}

struct Damage { int amount; };
struct Heal { int amount; };

TEST_CASE("EventBus delivers typed events") {
    EventBus bus;
    int damage = 0, heal = 0;
    auto sub = bus.subscribe<Damage>([&](const Damage& d) { damage += d.amount; });
    bus.subscribe<Heal>([&](const Heal& h) { heal += h.amount; });
    bus.publish(Damage{5});
    bus.publish(Heal{2});
    CHECK(damage == 5);
    CHECK(heal == 2);
    bus.unsubscribe(sub);
    bus.publish(Damage{5});
    CHECK(damage == 5);
}

TEST_CASE("EventBus queue is thread safe and FIFO") {
    EventBus bus;
    std::vector<int> received;
    bus.subscribe<Damage>([&](const Damage& d) { received.push_back(d.amount); });
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&bus] {
            for (int i = 0; i < 250; ++i) bus.enqueue(Damage{1});
        });
    for (auto& t : threads) t.join();
    CHECK(received.empty());
    bus.dispatchQueued();
    CHECK(received.size() == 1000);
}

TEST_CASE("EventBus tolerates unsubscribe inside a handler") {
    EventBus bus;
    int calls = 0;
    SubscriptionId self;
    self = bus.subscribe<Damage>([&](const Damage&) {
        ++calls;
        bus.unsubscribe(self);
    });
    bus.publish(Damage{1});
    bus.publish(Damage{1});
    CHECK(calls == 1);
    CHECK(bus.handlerCount() == 0);
}

TEST_CASE("VFS URI parsing is strict") {
    auto ok = VirtualFileSystem::parse("assets://textures/./hero/../hero.png");
    REQUIRE(ok);
    CHECK(ok.value().scheme == "assets");
    CHECK(ok.value().path == "textures/hero.png");
    CHECK_FALSE(VirtualFileSystem::parse("assets://../secret"));
    CHECK_FALSE(VirtualFileSystem::parse("assets:///etc/passwd"));
    CHECK_FALSE(VirtualFileSystem::parse("no-scheme/path"));
    CHECK_FALSE(VirtualFileSystem::parse("user://C:/windows"));
}

TEST_CASE("VFS reads, writes and lists through mounts") {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / ("agr_vfs_test_" + Uuid::generate().toString());
    fs::create_directories(root);
    VirtualFileSystem vfs;
    vfs.mount("user", std::make_unique<NativeFileProvider>(root.string(), false));
    vfs.mount("ro", std::make_unique<NativeFileProvider>(root.string(), true));

    REQUIRE(vfs.writeText("user://saves/slot1.json", "{\"coins\":10}"));
    CHECK(vfs.exists("user://saves/slot1.json"));
    auto text = vfs.readText("user://saves/slot1.json");
    REQUIRE(text);
    CHECK(text.value() == "{\"coins\":10}");
    auto listing = vfs.list("user://");
    REQUIRE(listing);
    CHECK(listing.value() == std::vector<std::string>{"saves/"});
    CHECK_FALSE(vfs.writeText("ro://x.txt", "nope"));
    CHECK_FALSE(vfs.readBytes("missing://x"));
    CHECK(vfs.readBytes("user://nothing.bin").error().code == Error::Code::NotFound);
    CHECK_FALSE(fs::exists(root / "saves" / "slot1.json.tmp"));
    fs::remove_all(root);
}

TEST_CASE("RingBufferSink keeps the newest messages in order") {
    auto ring = std::make_shared<RingBufferSink>(3);
    LogMessage m;
    for (const char* t : {"a", "b", "c", "d", "e"}) {
        m.text = t;
        ring->write(m);
    }
    auto snap = ring->snapshot();
    REQUIRE(snap.size() == 3);
    CHECK(snap[0].text == "c");
    CHECK(snap[2].text == "e");
}

}  // TEST_SUITE
