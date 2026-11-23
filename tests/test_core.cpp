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
#include "agr/core/Profiler.hpp"
#include <nlohmann/json.hpp>

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

#include "agr/core/Profiler.hpp"

TEST_CASE("profiler: nested scopes, calls, averages, CSV, scopes left open are closed") {
    Profiler& p = Profiler::instance();
    p.reset();
    for (int frame = 0; frame < 3; ++frame) {
        p.beginFrame();
        {
            AGR_PROFILE_SCOPE("Update");
            for (int i = 0; i < 2; ++i) {
                AGR_PROFILE_SCOPE("Script");
                volatile double x = 0;
                for (int k = 0; k < 20000; ++k) x = x + k;
            }
        }
        p.push("Forgotten");  // never popped: endFrame closes it
        p.endFrame();
    }
    const auto rows = p.snapshot();
    REQUIRE(rows.size() == 3);
    CHECK(rows[0].path == "Update");
    CHECK(rows[1].path == "Update/Script");
    CHECK(rows[1].depth == 1);
    CHECK(rows[1].calls == 2);
    CHECK(rows[1].lastMs > 0.0);
    CHECK(rows[0].lastMs >= rows[1].lastMs);
    CHECK(rows[1].avgMs > 0.0);
    CHECK(rows[1].maxMs >= rows[1].lastMs);
    CHECK(rows[2].name == "Forgotten");
    CHECK(p.depth() == 0);
    CHECK(p.toCsv().find("Update/Script;") != std::string::npos);
    p.setEnabled(false);
    p.beginFrame();
    p.push("Off");
    p.pop();
    p.endFrame();
    CHECK(p.snapshot().size() == 3);
    p.setEnabled(true);
    p.reset();
}

TEST_CASE("profiler capture: nested scopes on a timeline, exported as a Chrome trace") {
    Profiler& p = Profiler::instance();
    p.setEnabled(true);
    p.startCapture(2);
    for (int f = 0; f < 3; ++f) {  // the third frame is not captured
        p.beginFrame();
        p.push("Update");
        p.push("Physics");
        p.pop();
        p.push("Script");
        p.pop();
        p.pop();
        p.endFrame();
    }
    CHECK_FALSE(p.capturing());
    const auto& ev = p.capture();
    REQUIRE(ev.size() == 8);  // per frame: Physics, Script, Update, Frame
    CHECK(ev[0].name == "Physics");
    CHECK(ev[0].depth == 2);
    CHECK(ev[2].name == "Update");
    CHECK(ev[2].depth == 1);
    CHECK(ev[3].name == "Frame");
    // Children inside their parent, frames in order.
    CHECK(ev[0].startUs >= ev[2].startUs);
    CHECK(ev[0].startUs + ev[0].durationUs <= ev[2].startUs + ev[2].durationUs + 1e-3);
    CHECK(ev[7].startUs >= ev[3].startUs + ev[3].durationUs - 1e-3);
    const std::string trace = p.captureToChromeTrace();
    CHECK(trace.find("\"traceEvents\"") != std::string::npos);
    CHECK(trace.find("\"name\":\"Physics\",\"cat\":\"agr\",\"ph\":\"X\"") != std::string::npos);
    const auto j = nlohmann::json::parse(trace, nullptr, false);  // valid JSON
    REQUIRE_FALSE(j.is_discarded());
    CHECK(j["traceEvents"].size() == 8);
}

#include "agr/core/Compression.hpp"
#include <random>

TEST_CASE("LZ compression: round trips, shrinks repetitive data, rejects corrupt input safely") {
    auto roundTrip = [](const std::vector<u8>& data) {
        const auto packed = lz::compress(data);
        const auto back = lz::decompress(packed, data.size() + 16);
        REQUIRE(back);
        CHECK(*back == data);
        return packed.size();
    };
    roundTrip({});
    roundTrip({1});
    roundTrip({1, 2, 3});
    std::vector<u8> text;
    const std::string json = R"({"kind":"state","x":12.5,"y":3.25,"hp":100,"name":"player"},)";
    for (int i = 0; i < 40; ++i) text.insert(text.end(), json.begin(), json.end());
    CHECK(roundTrip(text) < text.size() / 5);  // repetitive JSON: 5x smaller at least
    std::vector<u8> zeros(5000, 0);
    CHECK(roundTrip(zeros) < 64);  // long runs (overlapping copies)
    std::mt19937 rng(7);
    std::vector<u8> noise(3000);
    for (auto& b : noise) b = static_cast<u8>(rng());
    CHECK(roundTrip(noise) <= noise.size() + noise.size() / 64 + 16);  // incompressible: small overhead
    for (int len : {16, 255, 256, 270, 1000, 70000}) {  // long literal / match lengths, offsets near 65535
        std::vector<u8> d(static_cast<usize>(len));
        for (int k = 0; k < len; ++k) d[static_cast<usize>(k)] = static_cast<u8>((k * 7) % 251 + (k / 300 % 2));
        roundTrip(d);
    }
    // Corrupt input never crashes or overflows: random mutations and truncations.
    const auto packed = lz::compress(text);
    for (int t = 0; t < 2000; ++t) {
        std::vector<u8> bad = packed;
        const int edits = 1 + static_cast<int>(rng() % 4);
        for (int e = 0; e < edits; ++e) bad[rng() % bad.size()] = static_cast<u8>(rng());
        if (rng() % 3 == 0) bad.resize(rng() % bad.size());
        const auto out = lz::decompress(bad, text.size());
        if (out) CHECK(out->size() <= text.size());
    }
    CHECK_FALSE(lz::decompress(packed, text.size() - 1));  // too big for the limit
}
