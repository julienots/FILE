// LOST KINGDOM — game content definitions (items, enemies, bosses, spells, skills, NPCs, quests).
#pragma once
#include "Characters.h"
#include "World.h"
#include <string>
#include <vector>

namespace lk {

enum class Element : uint8_t { Physical, Fire, Ice, Lightning, Nature, Shadow, Light, Arcane, Count };
const char* elementName(Element e);
vec3 elementColor(Element e);

enum class Rarity : uint8_t { Common, Rare, Epic, Legendary, Mythic, Ancestral, Count };
const char* rarityName(Rarity r);
vec3 rarityColor(Rarity r);
float rarityMult(Rarity r);

enum class ItemKind : uint8_t { Weapon, Shield, Head, Chest, Hands, Feet, Ring, Amulet, Consumable, Material, Quest, Tool, MountToken, Relic, Cosmetic };
enum EquipSlot { SL_WEAPON = 0, SL_OFFHAND, SL_HEAD, SL_CHEST, SL_HANDS, SL_FEET, SL_RING, SL_AMULET, SL_COUNT };
const char* slotName(int s);
int slotForKind(ItemKind k);

struct ItemDef {
    std::string id, name, desc;
    ItemKind kind = ItemKind::Material;
    Rarity rarity = Rarity::Common;
    int tier = 0;
    WeaponType weapon = WeaponType::None;
    float atk = 0, def = 0, hp = 0, mp = 0, stamina = 0, crit = 0, speed = 0;
    Element element = Element::Physical;
    float elemDmg = 0;
    int set = -1;
    int value = 1;
    int stack = 1;
    std::string icon;          // atlas icon name (i_*) used when no 3D icon applies
    // visuals (armour)
    Helm helm = Helm::None;
    Body body = Body::Tunic;
    vec3 color{0.5f, 0.5f, 0.5f};
    bool pauldrons = false, gauntlets = false, greaves = false, cape = false;
    // consumables
    float heal = 0, mana = 0, stam = 0;
    int buff = 0; float buffTime = 0;
    // mounts
    int mount = -1;
};
const ItemDef* item(const std::string& id);
const std::vector<ItemDef>& allItems();

struct SetDef { std::string name, bonus2, bonus4; float atk2 = 0, def2 = 0, hp2 = 0, crit4 = 0, atk4 = 0, mp4 = 0, def4 = 0, speed4 = 0; Element elem4 = Element::Physical; float elemDmg4 = 0; };
const std::vector<SetDef>& sets();

struct Recipe { std::string result; int count; std::vector<std::pair<std::string, int>> inputs; int station; int minLevel; };
enum Station { ST_ANY = 0, ST_FORGE = 1, ST_ALCHEMY = 2, ST_CAMPFIRE = 3, ST_WORKBENCH = 4 };
const char* stationName(int s);
const std::vector<Recipe>& recipes();

// ---- spells & skills ------------------------------------------------------------------------
enum class SpellKind : uint8_t { Projectile, Area, Self, Dash, Chain, Shield, Beam, Rain };
struct SpellDef {
    std::string id, name, desc, icon;
    Element school;
    SpellKind kind;
    float mana, cooldown, power, radius, duration;
};
const std::vector<SpellDef>& spells();
int spellIndex(const std::string& id);

enum Branch { BR_WARRIOR = 0, BR_ARCHER, BR_ASSASSIN, BR_MAGE, BR_PALADIN, BR_HUNTER, BR_COUNT };
const char* branchName(int b);
struct SkillNode {
    std::string id, name, desc;
    int branch, tier, cost;
    std::string requires_;   // node id
    std::string spell;      // unlocks a spell
    float atk = 0, def = 0, hp = 0, mp = 0, stamina = 0, crit = 0, critDmg = 0, speed = 0, bowDmg = 0, daggerDmg = 0, spellDmg = 0, heal = 0, staminaCost = 0, parryWindow = 0, petDmg = 0;
};
const std::vector<SkillNode>& skillTree();
int skillIndex(const std::string& id);

// ---- enemies ---------------------------------------------------------------------------------
enum class Behavior : uint8_t { Melee, Ranged, Caster, Charger, Pack, Tank, Ambusher, Flyer, Swarm, Boss };
enum class AttackType : uint8_t { Melee, Projectile, Area, Charge, Leap, Breath, Summon, Beam };
struct AttackDef { ClipId clip; AttackType type; float range, mult, cooldown, telegraph; Element element; float radius; int phase; };
struct EnemyDef {
    std::string id, name, desc, family;
    Arch arch = Arch::Humanoid;
    HumanoidLook hlook;
    CreatureLook clook;
    WeaponType weapon = WeaponType::None, offhand = WeaponType::None;
    float scale = 1;
    int region = 0, level = 1;
    float hp = 50, atk = 8, def = 2, speed = 3.5f;
    Behavior behavior = Behavior::Melee;
    std::vector<AttackDef> attacks;
    Element weak = Element::Count, resist = Element::Count;
    float aggro = 14, fov = 120, poise = 20;
    int xp = 10, gold = 3, group = 1;
    std::vector<std::pair<std::string, float>> loot;  // item id, chance
    bool night = false;     // spawns mostly at night
    float radius = 0.45f;
};
const std::vector<EnemyDef>& enemies();
int enemyIndex(const std::string& id);

struct BossDef {
    std::string id, name, title, intro, outro;
    int enemy;              // base enemy def (model/attacks)
    std::string arenaPoi;   // world arena (or dungeon boss room if dungeon >= 0)
    int dungeon = -1;
    float hpMult = 1;
    std::vector<float> phases;  // hp thresholds starting new phases (e.g. 0.66, 0.33)
    std::vector<std::string> rewards;
    std::string shard;      // crown shard item id
    std::string music;
};
const std::vector<BossDef>& bosses();

// ---- NPCs, dialogue, quests -----------------------------------------------------------------
enum class NpcRole : uint8_t { Villager, Merchant, Blacksmith, Innkeeper, Guard, Alchemist, Stablemaster, Elder, Hunter, Priest, Farmer, Child, Scholar, Bard };
struct NpcDef {
    std::string id, name, title;
    NpcRole role;
    std::string home;       // poi id
    HumanoidLook look;
    std::vector<std::string> greetings, lore;
    std::vector<std::string> shop;  // item ids sold
    int faction = -1;
    vec2 offset;            // position around the village centre
};
const std::vector<NpcDef>& npcs();
int npcIndex(const std::string& id);

enum Faction { FA_GREENHAVEN = 0, FA_FROSTHEIM, FA_ELDERWOOD, FA_ASHEN, FA_HIGHCROWNS, FA_GRAVEMOOR, FA_COUNT };
const char* factionName(int f);

enum class QuestType : uint8_t { Main, Side, Hidden, Hunt, Treasure, Faction };
enum class StepKind : uint8_t { Talk, Goto, Kill, KillBoss, Collect, Deliver, Discover, Craft, Harvest, Dungeon };
struct QuestStep { std::string desc; StepKind kind; std::string target; int count = 1; std::string poi; };
struct QuestDef {
    std::string id, title, desc;
    QuestType type;
    std::string giver;   // npc id ("" = auto start / found in world)
    std::string offer, progress, done;
    std::vector<QuestStep> steps;
    int xp = 50, gold = 20;
    std::vector<std::string> rewards;
    int faction = -1, rep = 0;
    std::string requires_;  // quest that must be completed first
    int minShards = 0;
};
const std::vector<QuestDef>& quests();
int questIndex(const std::string& id);

// ---- mounts ---------------------------------------------------------------------------------
struct MountDef { std::string id, name, desc; CreatureLook look; float scale, speed, sprint, stamina; int price; };
const std::vector<MountDef>& mounts();

// ---- dungeons ---------------------------------------------------------------------------------
struct DungeonDef { std::string id, name, poi; int region; int seed; int rooms; std::vector<std::string> enemies; int boss = -1; int theme; std::string reward; };
const std::vector<DungeonDef>& dungeons();

// Lore pages collected in the world (Collection)
struct LoreDef { std::string id, title, text; };
const std::vector<LoreDef>& lore();

}  // namespace lk
