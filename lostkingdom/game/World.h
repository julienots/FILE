// LOST KINGDOM — the continuous open world: regions, terrain field, water, roads, settlements,
// points of interest and chunk streaming.
#pragma once
#include "../engine/core/Math.h"
#include "../engine/core/Noise.h"
#include "../engine/gfx/GL.h"
#include "../engine/gfx/MeshBuilder.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <functional>

namespace lk {
using namespace agr;

constexpr float WORLD_SIZE = 3072.f;
constexpr float CHUNK = 32.f;
constexpr int CHUNKS = (int)(WORLD_SIZE / CHUNK);
constexpr float SEA_LEVEL = 0.f;

enum Region : uint8_t { R_GREENHAVEN = 0, R_FROSTHEIM, R_ASHEN, R_ELDERWOOD, R_SHADOWS, R_HIGHCROWNS, R_COUNT };
const char* regionName(int r);
const char* regionSubtitle(int r);

struct RegionWeights { float w[R_COUNT]; int dominant; };

enum class PoiType : uint8_t { Village, Castle, Ruins, Cave, Tower, Shrine, Camp, BossArena, Mine, Monastery, Graveyard, ElvenRuins, Farm, Mill, Lake, Volcano, Bridge, Waystone };

struct Poi {
    std::string id, name;
    PoiType type;
    vec2 pos;
    float radius;          // flattened area
    float height = 0;      // flattened height (computed)
    int region;
    int dungeon = -1;      // entrance to dungeon index
    int boss = -1;         // boss index for arenas
    bool discovered = false;
};

struct Road { std::vector<vec2> pts; float width; bool cobble; };
struct River { std::vector<vec2> pts; std::vector<float> surf; float width; };
struct Lake { vec2 c; float r; float level; bool lava; bool frozen; };

// Static collider (in the XZ plane). Circles and oriented boxes.
struct Collider {
    vec2 c; float r = 0;          // circle if hx == 0
    float hx = 0, hz = 0, rot = 0; // oriented box half extents
    float top = 4.f;               // height above base (for jumping onto low things)
    float base = 0;
    uint8_t climb = 0;             // 1 = can be vaulted / climbed
};

// A building or structure placed by the world plan (settlements, castles, ruins...).
struct Structure {
    int kind;      // StructureKind (Props.h)
    vec2 pos;
    float rot;
    float scale = 1;
    int variant = 0;
    int region;
    int poi = -1;
};

// Harvestable / interactive node spawned with chunks.
enum class NodeType : uint8_t { Tree, Rock, IronOre, CopperOre, RareOre, Herb, Crystal, Chest, FishingSpot, Relic, Mushroom, Berries };
struct NodeSpawn { NodeType type; vec3 pos; float rot; uint32_t uid; int tier; };

struct ChunkKey {
    int x, z;
    bool operator==(const ChunkKey& o) const { return x == o.x && z == o.z; }
};
struct ChunkKeyHash { size_t operator()(const ChunkKey& k) const { return (size_t)(k.x * 73856093) ^ (size_t)(k.z * 19349663); } };

// CPU-side chunk data produced by worker threads.
struct ChunkBuild {
    ChunkKey key;
    int lod = 0;
    MeshBuilder terrain, props, foliage, water, lava;  // foliage = wind-animated batch
    std::vector<InstanceData> grass, flowers;
    std::vector<Collider> colliders;
    std::vector<NodeSpawn> nodes;
    std::vector<vec4> lights;      // xyz + kind (lanterns, braziers, lava glow)
    std::vector<vec4> emitters;    // xyz + kind (chimney smoke, fire, waterfall mist, fireflies)
    int region = 0;
    float snow = 0;
};

struct Chunk {
    ChunkKey key;
    int lod = -1;
    bool ready = false;
    Mesh terrain, props, foliage, water, lava, grass, flowers;
    std::vector<Collider> colliders;
    std::vector<NodeSpawn> nodes;
    std::vector<vec4> lights, emitters;
    AABB bounds;
    int region = 0;
    float snow = 0;
    double lastUsed = 0;
    void destroy();
};

class World {
public:
    World();
    ~World();
    void buildPlan();  // deterministic: POIs, roads, rivers, lakes, structures (fast)

    // ---- terrain field (thread-safe, pure functions of position) ------------------------------
    RegionWeights regionAt(float x, float z) const;
    int regionId(float x, float z) const { return regionAt(x, z).dominant; }
    float baseHeight(float x, float z, const RegionWeights& rw) const;   // without roads / pois
    float height(float x, float z) const;                                 // final terrain height
    vec3 normal(float x, float z) const;
    float waterLevel(float x, float z, bool* lava = nullptr, bool* frozen = nullptr, vec2* flow = nullptr) const;  // -1e9 if none
    float roadInfluence(float x, float z, bool* cobble = nullptr) const;
    // Ground height including walkable structures (bridges, docks)
    float groundHeight(float x, float z) const;

    // ---- streaming --------------------------------------------------------------------------
    void startWorkers(int n);
    void stopWorkers();
    void update(vec3 focus, float viewRadius, int uploadBudget, double now);  // main thread
    void forEachVisibleChunk(const std::function<void(Chunk&)>& fn);
    Chunk* chunkAt(float x, float z);
    size_t loadedChunks() const { return chunks.size(); }
    size_t pendingChunks();
    float grassDensity = 1.f;
    void unloadAll();
    void generateChunkSync(ChunkKey k, int lod, ChunkBuild& out) const;   // used by tests and workers

    // ---- collision ------------------------------------------------------------------------
    // Pushes a circle (pos.xz, radius) out of static colliders; returns true if collided.
    bool collide(vec3& pos, float radius, float feetY, float* stepHeight = nullptr) const;
    bool lineBlocked(vec3 a, vec3 b) const;  // line of sight across structures (AI)

    // ---- world plan ---------------------------------------------------------------------------
    std::vector<Poi> pois;
    std::vector<Road> roads;
    std::vector<River> rivers;
    std::vector<Lake> lakes;
    std::vector<Structure> structures;
    std::vector<std::vector<int>> chunkStructures;   // per chunk index -> structures
    std::vector<std::vector<int>> chunkRoads;        // per chunk index -> road segment ids (packed road<<16|seg)
    const Poi* poiById(const std::string& id) const;
    int nearestPoi(vec2 p, float maxDist, PoiType* typeFilter = nullptr) const;

    // Minimap / world map colour image (generated in the background).
    std::vector<uint8_t> mapImage; int mapRes = 512; std::atomic<bool> mapReady{false};
    void generateMapImage();

    // Modified world state (harvested nodes, opened chests) - owned by save system
    std::unordered_map<uint32_t, double> harvested;  // uid -> respawn time (game seconds), 0 = never
    bool nodeAvailable(uint32_t uid, double gameTime) const;

    // statistics
    std::atomic<int> chunksGenerated{0};
    double genMsTotal = 0;

private:
    std::unordered_map<ChunkKey, std::unique_ptr<Chunk>, ChunkKeyHash> chunks;
    // job queue
    std::vector<std::thread> workers;
    std::mutex qmtx;
    std::condition_variable qcv;
    std::deque<std::pair<ChunkKey, int>> jobs;
    std::unordered_set<ChunkKey, ChunkKeyHash> inFlight;
    std::deque<std::unique_ptr<ChunkBuild>> done;
    std::atomic<bool> running{false};
    void workerLoop();
    void uploadChunk(ChunkBuild& b);
    float riverInfluence(float x, float z, float& surf, vec2& flow) const;
    float poiFlatten(float x, float z, float h, float& weight) const;
    std::vector<std::vector<int>> chunkPois;
    std::vector<std::vector<int>> chunkRivers;
    void indexPlan();
};

// World helpers
inline ChunkKey chunkOf(float x, float z) { return {(int)std::floor(x / CHUNK), (int)std::floor(z / CHUNK)}; }
inline int chunkIndex(int cx, int cz) { return (cx < 0 || cz < 0 || cx >= CHUNKS || cz >= CHUNKS) ? -1 : cz * CHUNKS + cx; }
float distToSegment(vec2 p, vec2 a, vec2 b, float* t = nullptr);

}  // namespace lk
