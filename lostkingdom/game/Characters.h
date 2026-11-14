// LOST KINGDOM — characters & creatures: skeletons, procedural skinned meshes, animation library.
#pragma once
#include "../engine/anim/Animation.h"
#include "../engine/gfx/MeshBuilder.h"

namespace lk {
using namespace agr;

enum class Arch : uint8_t { Humanoid, Quadruped, Spider, Dragon };

// Humanoid bones
enum HB { H_ROOT = 0, H_SPINE, H_CHEST, H_NECK, H_HEAD, H_SHL, H_UARML, H_LARML, H_HANDL, H_SHR, H_UARMR, H_LARMR, H_HANDR,
          H_THIGHL, H_SHINL, H_FOOTL, H_THIGHR, H_SHINR, H_FOOTR, H_PROPR, H_PROPL, H_COUNT };
// Quadruped bones (also used by the dragon, which adds wings and a longer neck/tail)
enum QB { Q_ROOT = 0, Q_SPINE, Q_CHEST, Q_NECK, Q_HEAD, Q_JAW, Q_TAIL1, Q_TAIL2, Q_TAIL3,
          Q_FL1, Q_FL2, Q_FL3, Q_FR1, Q_FR2, Q_FR3, Q_BL1, Q_BL2, Q_BL3, Q_BR1, Q_BR2, Q_BR3,
          Q_NECK2, Q_WINGL1, Q_WINGL2, Q_WINGR1, Q_WINGR2, Q_COUNT };
// Spider bones
enum SB { S_ROOT = 0, S_ABDOMEN, S_HEAD, S_LEG0 /* 8 legs x 2 segments */ };

const Skeleton& skeleton(Arch a);

enum class Species : uint8_t { Human, Elf, Dwarf, Goblin, Skeleton, Troll, Golem, Treant, Specter, Knight, Undead, Demon };
enum class Hair : uint8_t { None, Short, Long, Ponytail, Braids, Bun, Hood, Mohawk };
enum class Helm : uint8_t { None, Hood, Leather, Iron, Great, Crown, Horned, Circlet, Skull };
enum class Body : uint8_t { Tunic, Leather, Chain, Plate, Robe, Rags, Dress, Fur, Apron, Bones };

struct HumanoidLook {
    Species species = Species::Human;
    float height = 1.f, bulk = 1.f, headSize = 1.f;
    vec3 skin = hexColor(0xe0b18e), hairCol = hexColor(0x5a3a22), eyes = hexColor(0x3a5a7a);
    Hair hair = Hair::Short;
    bool beard = false, female = false;
    Helm helm = Helm::None;
    Body body = Body::Tunic;
    vec3 c1 = hexColor(0x3a5a8a), c2 = hexColor(0x6a4a2a), c3 = hexColor(0x3a3028);  // tunic, trousers, boots
    vec3 metal = hexColor(0xa8aab0), trim = hexColor(0xc9a45c);
    bool pauldrons = false, gauntlets = false, greaves = false, belt = true, cape = false, satchel = false;
    int hat = 0;              // villagers (1.6.1): 0 none, 1 straw hat, 2 cloth cap, 3 headscarf
    float emissiveEyes = 0;   // glowing eyes (undead, specters, corrupted knights)
    vec3 glow = hexColor(0x6ff0ff);
    uint32_t seed = 1;
};

struct CreatureLook {
    float scale = 1.f, length = 1.f, legLen = 1.f, girth = 1.f;
    vec3 c1 = hexColor(0x6a6a6a), c2 = hexColor(0xb0b0b0), c3 = hexColor(0x202020);
    bool horns = false, mane = false, tusks = false, armored = false, saddle = false, spikes = false, fur = true, crystal = false;
    float emissiveEyes = 0.5f;
    vec3 glow = hexColor(0xffc040);
    int kind = 0;  // 0 wolf, 1 boar, 2 bear, 3 horse, 4 deer, 5 rabbit, 6 giant wolf mount, 7 elk, 8 lizard
    uint32_t seed = 1;
};

void buildHumanoid(MeshBuilder& mb, const HumanoidLook& L);
void buildQuadruped(MeshBuilder& mb, const CreatureLook& L);
void buildSpider(MeshBuilder& mb, const CreatureLook& L);
void buildDragon(MeshBuilder& mb, const CreatureLook& L);

// ---- Weapons ---------------------------------------------------------------------------------
enum class WeaponType : uint8_t { None, Sword, Longsword, Dagger, Axe, Hammer, Spear, Bow, Crossbow, Staff, Shield, Pickaxe, WoodAxe, FishingRod, Torch, Count };
// Weapon meshes are built along +Z from the grip (origin). tipOut receives the blade tip (for trails).
void buildWeapon(MeshBuilder& mb, WeaponType t, int tier, vec3 accent, float* tipOut = nullptr);

// ---- Stances & animation ---------------------------------------------------------------------
enum Stance { ST_UNARMED = 0, ST_ONEHAND, ST_TWOHAND, ST_DAGGER, ST_SPEAR, ST_BOW, ST_STAFF, ST_TOOL };
Stance stanceFor(WeaponType t);

enum ClipId {
    // humanoid combat
    C_1H_A1, C_1H_A2, C_1H_A3, C_1H_HEAVY, C_1H_CHARGED,
    C_2H_A1, C_2H_A2, C_2H_A3, C_2H_HEAVY,
    C_DG_A1, C_DG_A2, C_DG_A3, C_DG_HEAVY,
    C_SP_A1, C_SP_A2, C_SP_HEAVY,
    C_BOW_SHOT, C_BOW_HEAVY,
    C_STAFF_CAST, C_STAFF_HEAVY,
    C_FIST_A1, C_FIST_A2,
    C_CAST_FORWARD, C_CAST_AREA, C_CAST_SELF,
    C_BLOCK, C_PARRY, C_RIPOSTE, C_SHIELD_BASH,
    C_ROLL, C_DODGE_BACK, C_LAND, C_VAULT,
    C_HIT_LIGHT, C_HIT_HEAVY, C_DEATH,
    C_INTERACT, C_PICKUP, C_OPEN_CHEST, C_GATHER_SWING, C_HERB_PICK, C_FISH_CAST, C_DRINK,
    C_MOUNT, C_DISMOUNT, C_VICTORY, C_WAVE, C_TALK, C_HAMMER_WORK, C_SWEEP_WORK, C_CHEER, C_TAUNT, C_ROAR_H,
    // quadruped
    C_Q_BITE, C_Q_POUNCE, C_Q_HOWL, C_Q_HIT, C_Q_DEATH, C_Q_CHARGE, C_Q_REAR,
    // spider
    C_S_BITE, C_S_SPIT, C_S_LEAP, C_S_HIT, C_S_DEATH,
    // dragon
    C_D_BITE, C_D_TAIL, C_D_BREATH, C_D_WING, C_D_ROAR, C_D_HIT, C_D_DEATH, C_D_STOMP,
    // big humanoids (troll, golem, treant, bosses)
    C_B_SLAM, C_B_SWIPE, C_B_STOMP, C_B_ROAR, C_B_SPIN, C_B_THROW,
    C_COUNT
};
// Event ids fired by clips (Animator::firedEvents)
enum AnimEventId { EV_HIT = 1, EV_STEP = 2, EV_RELEASE = 3, EV_SOUND_WHOOSH = 4, EV_IMPACT = 5, EV_DRINK = 6, EV_SPAWN_FX = 7 };

const Clip& clip(ClipId id);
void initClips();
void setupAnimator(Animator& a, Arch arch);

// Bone attachment helper: transform of bone in model space
inline mat4 boneMatrix(const PoseWorld& pw, int bone) { return pw.world[bone]; }

}  // namespace lk
