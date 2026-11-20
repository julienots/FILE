// LOST KINGDOM — procedural environment art: vegetation, rocks, buildings and landmarks.
// Each prop/structure is modelled in code (MeshBuilder), cached as a template per variant,
// and stamped into per-chunk batches (static batching) with a transform and colour variation.
#pragma once
#include "World.h"
#include "../engine/gfx/Model.h"
#include <memory>
#include <string>

namespace lk {

enum PropKind : uint8_t {
    P_OAK, P_BIRCH, P_PINE, P_SNOWPINE, P_DEADTREE, P_CHARRED, P_ELDERTREE, P_GIANTSHROOM, P_WILLOW, P_TWISTED,
    P_BUSH, P_FERN, P_ROCK, P_BOULDER, P_ICECRYSTAL, P_OBSIDIAN, P_MAGICCRYSTAL, P_GRAVESTONE, P_RUINCOLUMN,
    P_REEDS, P_STUMP, P_LOG, P_SMALLSHROOMS, P_CACTUSFERN, P_BONES, P_SNOWROCK, P_LILYPADS, P_HAYBALE, P_CRATE, P_BARREL,
    // world dressing (1.5): ground clutter, road furniture, wild landmarks
    P_FLOWERS, P_PEBBLES, P_TALLGRASS, P_BRANCHES, P_FENCE, P_SIGNPOST, P_CART, P_LANTERNPOST, P_STANDSTONE, P_CAMPFIRE, P_TENT,
    // 2.0.0 the Amber Sands and the Coral Coast
    P_ACACIA, P_CACTUS, P_AGAVE, P_SANDROCK, P_TUMBLEWEED, P_DRYBUSH, P_DATEPALM,
    P_PALM, P_BANANA, P_TROPBUSH, P_SEAROCK, P_DRIFTWOOD, P_SHELLS, P_HAMMOCK, P_NETRACK, P_BOAT, P_AMPHORA, P_CARPET, P_BRAZIER,
    P_COUNT
};

enum StructureKind : uint8_t {
    S_HOUSE, S_STONEHOUSE, S_LONGHOUSE, S_TOWER, S_WINDMILL, S_WELL, S_STALL, S_FENCE, S_FIELD,
    S_CASTLE_WALL, S_CASTLE_TOWER, S_KEEP, S_CHAPEL, S_RUIN_WALL, S_ELVEN_ARCH, S_CRYPT, S_CAVE, S_MINE,
    S_WAYSTONE, S_CAMPFIRE, S_BRIDGE, S_TENT, S_STILT_HUT, S_MONASTERY, S_STATUE, S_FORGE, S_INN, S_LANTERN,
    S_GATE, S_PALISADE, S_DOCK, S_OBELISK, S_ALTAR, S_PORTAL,
    S_PEN, S_BANNER,   // 1.9.2 player domain: livestock enclosure, the domain's banner
    // 2.0.0 sandstone towns, harbours, the temple of Shirokawa
    S_DOMEHOUSE, S_BAZAAR, S_MINARET, S_ZIGGURAT, S_LIGHTHOUSE, S_PAGODA, S_SPHINX, S_PIER, S_BEACHHUT, S_COLONNADE, S_ARCHWAY,
    S_COUNT
};

// Emitter / animated object kinds stored in ChunkBuild::emitters (w component)
enum EmitterKind { EM_SMOKE = 1, EM_FIRE = 2, EM_FIREFLIES = 3, EM_MIST = 4, EM_WINDMILL = 5, EM_LAVA_BUBBLES = 6, EM_SPARKLES = 7, EM_EMBERS = 8, EM_BANNER = 9, EM_WATERFALL = 10 };
// Light kinds stored in ChunkBuild::lights (w component)
enum LightKind { LI_LANTERN = 1, LI_FIRE = 2, LI_LAVA = 3, LI_MAGIC = 4, LI_CRYSTAL = 5 };

struct PropTemplate { MeshBuilder solid, foliage; float radius; float height; };

void initPropTemplates();   // build template cache (thread-safe after this call)
const PropTemplate& propTemplate(PropKind k, int variant, bool lite = false);  // lite: mid-distance version (no leaf cards)
int propVariants(PropKind k);

// Stamp a prop into the batches with transform & tint. Adds collider when solid.
void stampProp(ChunkBuild& out, PropKind k, int variant, vec3 pos, float rotY, float scale, vec3 tint, bool collide);
// Height at which a structure stands (lowest ground under its footprint for buildings).
float structureGroundY(const Structure& s, const World& w);
vec2 structureFootprint(int kind);   // half extents (m) of a structure's base
float structurePlinth(const Structure& s, const World& w);   // height of the stone foundation (0: none)
// Build a structure into the chunk batches.
void buildStructure(ChunkBuild& out, const Structure& s, const World& w);
// 2.0.0 the yard around a dwelling (props only; see Props.cpp)
void dressStructure(ChunkBuild& out, const Structure& s, const World& w);

// 1.9.0 imported building models: assets/models/props/<name>.lkm + .jpg, variants <name>_2, <name>_3
// (tools/import_prop.py). When a model exists for a structure kind, the chunk places it on the structure's
// footprint (uniform scale, door towards +Z like the code-built house) instead of building it in code;
// colliders, lights and chimney smoke stay the same.
struct PropModel {
    std::string id;
    SkinnedModelData data;     // CPU copy (GPU objects are rebuilt after a context loss)
    vec3 bmin, bmax;
    std::shared_ptr<Mesh> mesh;
    Texture tex;
    // 2.3.6 far version (vertex clustering, ~20-30 % of the triangles): used beyond ~32 m and for distant shadows
    std::vector<Vertex> lodVerts; std::vector<uint32_t> lodIdx;
    std::shared_ptr<Mesh> lodMesh;
};
int loadPropModels();                        // main thread, before the chunk workers start; returns the count
int propModelFor(int kind, int variant);     // -1: no model, the structure is built in code
PropModel* propModel(int i);
const char* structureModelName(int kind);   // "house", "inn"... (nullptr: kind without model support)

// Windmill blades: separate animated mesh
void buildWindmillBlades(MeshBuilder& mb);
// Region-specific palettes
vec3 regionGroundTint(int region);
vec3 foliageColor(int region, float t);

}  // namespace lk
