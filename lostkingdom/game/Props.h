// LOST KINGDOM — procedural environment art: vegetation, rocks, buildings and landmarks.
// Each prop/structure is modelled in code (MeshBuilder), cached as a template per variant,
// and stamped into per-chunk batches (static batching) with a transform and colour variation.
#pragma once
#include "World.h"

namespace lk {

enum PropKind : uint8_t {
    P_OAK, P_BIRCH, P_PINE, P_SNOWPINE, P_DEADTREE, P_CHARRED, P_ELDERTREE, P_GIANTSHROOM, P_WILLOW, P_TWISTED,
    P_BUSH, P_FERN, P_ROCK, P_BOULDER, P_ICECRYSTAL, P_OBSIDIAN, P_MAGICCRYSTAL, P_GRAVESTONE, P_RUINCOLUMN,
    P_REEDS, P_STUMP, P_LOG, P_SMALLSHROOMS, P_CACTUSFERN, P_BONES, P_SNOWROCK, P_LILYPADS, P_HAYBALE, P_CRATE, P_BARREL,
    P_COUNT
};

enum StructureKind : uint8_t {
    S_HOUSE, S_STONEHOUSE, S_LONGHOUSE, S_TOWER, S_WINDMILL, S_WELL, S_STALL, S_FENCE, S_FIELD,
    S_CASTLE_WALL, S_CASTLE_TOWER, S_KEEP, S_CHAPEL, S_RUIN_WALL, S_ELVEN_ARCH, S_CRYPT, S_CAVE, S_MINE,
    S_WAYSTONE, S_CAMPFIRE, S_BRIDGE, S_TENT, S_STILT_HUT, S_MONASTERY, S_STATUE, S_FORGE, S_INN, S_LANTERN,
    S_GATE, S_PALISADE, S_DOCK, S_OBELISK, S_ALTAR, S_PORTAL, S_COUNT
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
// Build a structure into the chunk batches.
void buildStructure(ChunkBuild& out, const Structure& s, const World& w);

// Windmill blades: separate animated mesh
void buildWindmillBlades(MeshBuilder& mb);
// Region-specific palettes
vec3 regionGroundTint(int region);
vec3 foliageColor(int region, float t);

}  // namespace lk
