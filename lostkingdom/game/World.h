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

constexpr float WORLD_SIZE = 4608.f;   // 2.0.0: 3072 -> 4608 m (the old island keeps its coordinates; the new lands lie east and south)
constexpr float CHUNK = 32.f;
constexpr int CHUNKS = (int)(WORLD_SIZE / CHUNK);
constexpr float SEA_LEVEL = 0.f;

enum Region : uint8_t { R_GREENHAVEN = 0, R_FROSTHEIM, R_ASHEN, R_ELDERWOOD, R_SHADOWS, R_HIGHCROWNS, R_AMBER, R_CORAL, R_COUNT };
const char* regionName(int r);
const char* regionSubtitle(int r);

struct RegionWeights { float w[R_COUNT]; int dominant; };

enum class PoiType : uint8_t { Village, Castle, Ruins, Cave, Tower, Shrine, Camp, BossArena, Mine, Monastery, Graveyard, ElvenRuins, Farm, Mill, Lake, Volcano, Bridge, Waystone, Temple };

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
    bool coastal = false;  // 2.0.0 harbour: kept low so the sea stays where the piers are
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
    float baseY = -1e9f;   // 2.2.1 levelled ground height under the building (the terrain is flattened to it); -1e9: not levelled
};

// Harvestable / interactive node spawned with chunks.
enum class NodeType : uint8_t { Tree, Rock, IronOre, CopperOre, RareOre, Herb, Crystal, Chest, FishingSpot, Relic, Mushroom, Berries,
    // 2.3.5 more resources
    GoldOre, SilverOre, Coal, Cactus, Shells, Flax, AmberRock, FireRoot, NodeTypeCount };
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
    std::vector<InstanceData> grass, flowers, grassNear;   // grassNear: extra clumps drawn only near the hero
    std::vector<InstanceData> tallGrass;                   // 1.9.1 waist-high meadow patches (bend around characters)
    std::vector<Collider> colliders;
    std::vector<NodeSpawn> nodes;
    std::vector<vec4> lights;      // xyz + kind (lanterns, braziers, lava glow)
    std::vector<vec4> emitters;    // xyz + kind (chimney smoke, fire, waterfall mist, fireflies)
    int region = 0;
    float snow = 0;
    bool liteProps = false;        // use mid-distance prop templates
    std::vector<std::pair<int, mat4>> models;   // imported building models (propModel index, transform)
};

struct Chunk {
    ChunkKey key;
    int lod = -1;
    bool ready = false;
    Mesh terrain, props, foliage, water, lava, grass, flowers, grassNear, tallGrass;
    std::vector<Collider> colliders;
    std::vector<NodeSpawn> nodes;
    std::vector<vec4> lights, emitters;
    std::vector<std::pair<int, mat4>> models;
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
    // 1.9.1 tall grass meadows: 0 (none) .. 1 (dense), same answer for the chunk builder and for gameplay
    float tallGrassAmount(float x, float z) const;
    float forestAt(float x, float z) const;   // 1.13.0 tree cover 0..1 (the scatter density), for the forest atmosphere
    // Ground height including walkable structures (bridges, docks)
    float groundHeight(float x, float z) const;
    // 1.11.2 one deck profile for the bridge mesh, the walkable ground and the railings: `along` in metres from
    // the centre (-halfLen..halfLen), returns the world height of the deck top. It meets the road at both ends.
    static float bridgeHalfLen(const Structure& s) { return 8.f * s.scale + 4.f; }
    float bridgeDeck(const Structure& s, float along) const;

    // ---- streaming --------------------------------------------------------------------------
    void startWorkers(int n);
    void stopWorkers();
    void update(vec3 focus, float viewRadius, int uploadBudget, double now);  // main thread
    void forEachVisibleChunk(const std::function<void(Chunk&)>& fn);
    Chunk* chunkAt(float x, float z);
    // fraction (0..1) of the chunks within `radius` of `focus` that are generated and uploaded (cinematics wait
    // for their scenery before fading in)
    float areaReady(vec3 focus, float radius) const;
    size_t loadedChunks() const { return chunks.size(); }
    size_t pendingChunks();
    // 1.9.2 runtime additions (the player's domain): a POI and structures added after the plan was built.
    // Workers are paused while the tables change; the chunks around are rebuilt in place (old meshes stay
    // visible until the new ones are uploaded).
    int addRuntimePoi(const Poi& p);
    int addRuntimeStructure(const Structure& s);
    void resetRuntime();                        // back to the generated plan (new game / load)
    void refreshArea(vec2 c, float r);
    int plannedPois = -1, plannedStructures = -1, workerCount = 0;
    float grassDensity = 1.f;
    bool detailedFoliage = true;   // leaf cards on near trees (off on low-end profiles)
    void unloadAll();
    void generateChunkSync(ChunkKey k, int lod, ChunkBuild& out) const;
    static void uploadBuild(ChunkBuild& b, Chunk& c);   // 2.2.2 meshes of a build into any chunk (realm scenery)   // used by tests and workers

    // ---- collision ------------------------------------------------------------------------
    // Pushes a circle (pos.xz, radius) out of static colliders; returns true if collided.
    bool collide(vec3& pos, float radius, float feetY, float* stepHeight = nullptr) const;
    bool lineBlocked(vec3 a, vec3 b) const;  // line of sight across structures (AI)
    // Camera collision: is the point inside a building-sized box collider (walls, houses, towers)?
    // Trees, rocks, fences and low props are ignored (the see-through shader fades those instead).
    bool cameraBlocked(vec3 p, float margin) const;
    // 1.6.1: coarse relief of the whole island (and the sea around it) for the horizon, drawn beyond the
    // detailed chunks so mountains are seen from afar. Lowered slightly so it never pokes through them.
    void buildFarMesh(MeshBuilder& out, int res) const;

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
    std::vector<uint8_t> mapImage; int mapRes = 768; std::atomic<bool> mapReady{false};
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
    std::vector<std::vector<int>> chunkLevel;   // 2.2.1 per chunk: buildings whose levelled pad reaches it
    bool levelReady = false;
    void levelBuildings();
    void resolvePlacement();   // 1.12.0 buildings off the roads, no two buildings in each other
};

// World helpers
inline ChunkKey chunkOf(float x, float z) { return {(int)std::floor(x / CHUNK), (int)std::floor(z / CHUNK)}; }
inline int chunkIndex(int cx, int cz) { return (cx < 0 || cz < 0 || cx >= CHUNKS || cz >= CHUNKS) ? -1 : cz * CHUNKS + cx; }
float distToSegment(vec2 p, vec2 a, vec2 b, float* t = nullptr);

}  // namespace lk
