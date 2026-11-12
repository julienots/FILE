// LOST KINGDOM — game runtime (AGR Engine application).
#pragma once
#include "../engine/core/App.h"
#include "../engine/core/ECS.h"
#include "../engine/gfx/Renderer.h"
#include "../engine/gfx/Particles.h"
#include "../engine/gfx/UI.h"
#include "../engine/audio/Audio.h"
#include "World.h"
#include "Props.h"
#include "Characters.h"
#include "Data.h"
#include <memory>
#include <map>
#include <set>
#include <deque>

namespace lk {

inline vec3 perpVec(vec3 f) { return normalize(vec3(-f.z, 0, f.x)); }

// ------------------------------------------------------------------------------------------ components
struct Visual {
    Arch arch = Arch::Humanoid;
    std::shared_ptr<Mesh> mesh, weapon, offhand, capeMesh;
    Animator anim;
    PoseWorld pw;
    WeaponType wt = WeaponType::None, ot = WeaponType::None;
    float tip = 0.8f;
    float scale = 1.f;
    vec3 tint{1, 1, 1};
    float flash = 0, dissolve = 0, rim = 0.5f, emissiveBoost = 0;
    vec3 dissolveColor{1.f, 0.5f, 0.15f};
    bool outline = true;
    vec4 outlineColor{0.06f, 0.04f, 0.03f, 1};
    bool cape = false;
    VerletCloth cloth;
    vec3 capeColor{0.5f, 0.1f, 0.1f};
    bool tail = false;
    VerletChain hair;
    vec3 hairColor;
    std::shared_ptr<Mesh> hairMesh;
    bool footIK = true;
    float pelvisAdj = 0;
    bool visible = true;
    // weapon trail
    std::deque<std::pair<vec3, vec3>> trail;   // base, tip (world)
    std::shared_ptr<Mesh> trailMesh;
    vec3 trailColor{1, 0.9f, 0.7f};
};

enum class Team : uint8_t { Player, Enemy, Neutral, Animal, Mount };

struct Status { float burn = 0, freeze = 0, slow = 0, root = 0, poison = 0, stun = 0, shield = 0, regen = 0, strength = 0, frostRes = 0, wellFed = 0; };

struct Character {
    Team team = Team::Neutral;
    std::string name;
    vec3 pos, vel;
    float yaw = 0, radius = 0.45f, height = 1.8f, mass = 1;
    bool grounded = true, inWater = false, swimming = false;
    float groundY = 0, airTime = 0;
    Visual vis;
    float hp = 100, maxHp = 100, poise = 30, maxPoise = 30;
    float atk = 10, def = 2;
    Element element = Element::Physical;
    float elemDmg = 0;
    bool dead = false;
    float deathT = 0, hitStun = 0, invuln = 0, flashT = 0;
    vec3 knock;
    Status status;
    int enemyDef = -1, npcDef = -1, bossDef = -1, mountDef = -1, animal = -1;
    int level = 1;
    // combat action
    const Clip* attackClip = nullptr;
    int attackIdx = -1;
    float attackMult = 1;
    Element attackElem = Element::Physical;
    std::vector<Entity> hitThisSwing;
    float lastMotion = 0;
    float telegraph = 0;
    uint32_t uid = 0;       // spawn uid (for persistence of kills)
    float lastHitBy = 0;
    float speedMul = 1;
    bool elite = false;
};

enum class AIState : uint8_t { Idle, Patrol, Alert, Chase, Strafe, Attack, Recover, Flee, Search, Return, Dead, Intro };
struct AIBrain {
    AIState state = AIState::Idle;
    Entity target = NullEntity;
    vec3 home, lastSeen, patrolTarget;
    float timer = 0, think = 0, strafeDir = 1, aggro = 0, cooldowns[8] = {0};
    int pendingAttack = -1;
    float telegraphT = 0;
    int phase = 0;
    bool introDone = false;
    int summons = 0;
    float fly = 0;        // dragons
    std::vector<vec2> path;
    int pathIdx = 0;
    float repathT = 0;
};

enum class NpcActivity : uint8_t { Idle, Walk, Work, Talk, Sleep, Sit, Pray, Flee };
struct NpcBrain {
    vec3 home, work, social, bed;
    NpcActivity act = NpcActivity::Idle;
    vec3 dest;
    float timer = 0;
    bool indoors = false;
    bool generic = true;
    int village = -1;
    float waveCD = 0;
    ClipId workClip = C_HAMMER_WORK;
};

struct AnimalBrain { vec3 home; vec3 dest; float timer = 0; bool fleeing = false; int kind = 0; };

enum class ProjKind : uint8_t { Arrow, Fireball, IceSpike, Arcane, Shadow, Nature, Boulder, Web, Lightning, Holy };
struct Projectile {
    ProjKind kind;
    vec3 pos, vel;
    float dmg, life = 3, radius = 0.3f, splash = 0, gravity = 0;
    Element elem;
    Entity owner = NullEntity, homing = NullEntity;
    Team team;
    bool pierce = false;
    std::vector<Entity> hit;
    float slow = 0;
};

struct Pickup { std::string item; int count; Rarity rarity; float t = 0; vec3 pos, vel; };

struct NodeComp { NodeSpawn node; std::shared_ptr<Mesh> mesh; float hits = 0; float shake = 0; };

enum class InterKind : uint8_t { None, Npc, Node, Chest, Pickup, Station, DungeonEntrance, DungeonExit, Waystone, Lever, Plate, Brazier, Bed, Fishing, Mount, Board, Altar };
struct Interact { InterKind kind = InterKind::None; int param = 0; std::string text; float radius = 2.f; };

struct ItemStack {
    std::string id;
    int count = 1;
    Rarity rarity = Rarity::Common;
    int level = 0;          // upgrade +n
    uint32_t uid = 0;
    bool isNew = true;
};

struct QuestState { int idx; int step = 0; std::vector<int> progress; bool done = false; bool tracked = true; };

struct Notification { std::string text; vec3 color; float t = 0; std::string icon; };
struct FloatText { vec3 pos; std::string text; vec3 color; float t = 0; float size = 22; };

struct DialogState {
    bool open = false;
    int npc = -1;
    Entity ent = NullEntity;
    std::vector<std::string> lines;
    int line = 0;
    float reveal = 0;
    struct Choice { std::string text, action; };
    std::vector<Choice> choices;
    std::string speaker, title;
};

struct Input {
    vec2 move;
    bool sprint = false;
    bool attack = false, attackHeld = false, attackRelease = false;
    float attackHoldT = 0;
    bool heavy = false;
    bool dodge = false, jump = false, block = false, interact = false, potion = false, mount = false;
    bool skill[3] = {false, false, false};
    vec2 camDrag;
    float pinch = 0;
    void clearEdges() { attack = attackRelease = heavy = dodge = jump = interact = potion = mount = false; skill[0] = skill[1] = skill[2] = false; camDrag = vec2(0, 0); pinch = 0; }
};

// ------------------------------------------------------------------------------------------ dungeons
enum class TileType : uint8_t { Void, Floor, Wall, Pit, Spikes, Plate, Door, Water, Lava };
struct DungeonRoom { int x, z, w, h; int kind; bool cleared = false, visited = false; };
struct DungeonState {
    int def = -1;
    bool active = false;
    vec3 origin;                    // world position of tile (0,0)
    int W = 0, H = 0;
    std::vector<TileType> tiles;
    std::vector<DungeonRoom> rooms;
    Mesh floor, walls, props;
    std::vector<Collider> colliders;
    std::vector<vec4> lights;
    std::vector<vec4> emitters;
    vec3 entrance, exitPos, bossPos;
    int gateRoom = -1;
    bool gateOpen = false;
    // puzzles: rune braziers (light in order), pressure plates, levers
    std::vector<int> puzzleOrder;
    std::vector<vec3> braziers;
    std::vector<bool> brazierLit;
    int puzzleProgress = 0;
    bool puzzleSolved = false;
    vec3 gatePos; float gateRot = 0;
    std::vector<std::pair<vec3, float>> traps;   // spikes: pos, phase
    std::vector<vec3> blades;                    // swinging blades
    std::vector<vec3> fireJets;
    bool completed = false;
    bool bossDefeated = false;
    vec3 returnPos;                               // overworld position to return to
    int theme = 0;
};

// ------------------------------------------------------------------------------------------ settings
// HUD orientation helpers (pure functions, unit-tested).
// Screen rotation of an up-pointing sprite so that it shows world heading `yaw` ((sin yaw, cos yaw) in x/z)
// on a map whose "up" is the camera forward (-sin camYaw, -cos camYaw). camYaw = 0 gives the world map (north up).
inline float minimapArrowAngle(float yaw, float camYaw) { return PI - yaw + camYaw; }
inline vec2 minimapProject(vec2 worldOffset, float camYaw) { return rotate(worldOffset, camYaw); }

// Default touch layout: button centres in UI units relative to the bottom-right corner (mirrored for left-handed).
// Three arcs around the attack button (radius ~158: dodge/block/jump, ~275: spells, ~390: interact/potion/mount);
// interact is leftmost so its label has room. No two buttons overlap (checked by unit tests).
constexpr int LAYOUT_VERSION = 3;
inline float ctlSize(int id) { return id == 0 ? 136.f : (id >= 4 && id <= 6 ? 96.f : (id == 7 || id == 9 ? 78.f : 100.f)); }  // attack, spells, potion/mount, others
inline const vec2* defaultLayoutPos() {
    static const vec2 def[10] = {{-150, -140}, {-305, -107}, {-144, -298}, {-247, -264}, {-422, -102}, {-373, -302}, {-271, -387}, {-509, -292}, {-540, -154}, {-430, -411}};
    return def;
}

struct ControlLayout { vec2 pos[10]; float scale = 1.f; float opacity = 0.85f; bool leftHanded = false; };
enum CtlId { CTL_ATTACK = 0, CTL_DODGE, CTL_JUMP, CTL_BLOCK, CTL_SKILL1, CTL_SKILL2, CTL_SKILL3, CTL_POTION, CTL_INTERACT, CTL_MOUNT, CTL_COUNT };

struct Settings {
    QualityProfile profile = QualityProfile::High;
    float master = 0.9f, music = 0.6f, sfx = 0.9f, ambience = 0.7f;
    float camSensitivity = 1.f, joystickDeadzone = 0.12f;
    bool haptics = true, showFps = false, autoTarget = true, subtitles = true, cameraShake = true, invertCamera = false;
    float uiScale = 1.f;
    ControlLayout layout;
    bool chromatic = false;
};

// ------------------------------------------------------------------------------------------ game
enum class GameState : uint8_t { Splash, Title, Playing, Credits, Loading };
enum class Menu : uint8_t { None, Inventory, Equipment, Skills, Crafting, Map, Quests, Bestiary, Collection, Settings, Saves, Shop, Pause, Death, LayoutEdit };

class Game : public App {
public:
    // App
    bool init(int w, int h) override;
    void frame(float dt) override;
    void resize(int w, int h) override;
    void touch(const TouchEvent& e) override;
    bool back() override;
    void pause() override;
    void resume() override;
    void contextLost() override;
    void lowMemory() override;
    void shutdown() override;
    int targetFps() const override { return renderer.settings.fpsCap; }
    void renderAudio(float* out, int frames, int rate) override { audio.render(out, frames, rate); }

    // ---- core ---------------------------------------------------------------------------------
    int W = 1, H = 1;
    bool gpuReady = false;
    Renderer renderer;
    ParticleSystem particles;
    UI ui;
    AudioEngine audio;
    World world;
    Registry reg;
    Settings settings;
    GameState state = GameState::Splash;
    Menu menu = Menu::None;
    double gameTime = 0;      // seconds of play (persisted)
    float timeOfDay = 8.f;    // hours 0..24
    int day = 1;
    float realTime = 0, splashT = 0, stateT = 0;
    float timeScale = 1.f, hitStop = 0;
    Input input;
    std::string pendingScreenshot;

    // ---- player ---------------------------------------------------------------------------------
    Entity player = NullEntity;
    Entity mountEnt = NullEntity;
    bool riding = false;
    bool mountCalled = false;
    int level = 1;
    int xp = 0;
    int gold = 50;
    int skillPoints = 0;
    float mp = 50, maxMp = 50, stamina = 100, maxStamina = 100;
    float critChance = 0.05f, critDmg = 1.6f, moveSpeedMul = 1;
    float resist[(int)Element::Count] = {0};
    std::vector<ItemStack> inventory;
    ItemStack equipment[SL_COUNT];
    std::string dye;              // cosmetic colour
    std::set<std::string> skills;  // unlocked skill nodes
    std::vector<std::string> knownSpells;
    int equippedSpells[3] = {0, -1, -1};
    float spellCD[16] = {0};
    std::vector<std::string> ownedMounts;
    int activeMount = -1;
    int comboStep = 0;
    float comboTimer = 0, chargeT = 0;
    // input buffering: a tap during an animation is kept for a short time and executed as soon as possible
    float attackBuf = 0, dodgeBuf = 0; bool attackBufHeavy = false, attackBufCharged = false;
    int attackFinger = -1, dodgeFinger = -1, blockFinger = -1;
    float btnFlash[CTL_COUNT] = {};
    bool ctlVisible(int id) const;
    bool charging = false;
    float potionCD = 0, dodgeCD = 0, parryT = 0, blockT = 0;
    float staminaRegenDelay = 0;
    vec3 respawnPos;
    Entity lockTarget = NullEntity;
    Entity interactTarget = NullEntity;
    float fishingT = -1, fishBite = -1;
    int lastRegion = -1;
    float regionBannerT = 0;
    std::string regionBanner, regionBannerSub;

    // ---- progression state ----------------------------------------------------------------------
    std::vector<QuestState> questLog;
    std::set<std::string> questsDone;
    int trackedQuest = -1;
    std::set<std::string> discovered;      // poi ids
    std::set<std::string> defeatedBosses;
    std::map<std::string, int> bestiary;   // enemy id -> kills
    std::set<std::string> loreFound;
    std::set<std::string> relicsFound;
    std::set<std::string> talkedTo;
    std::set<uint32_t> killedUnique;       // elite / unique spawns
    std::set<uint32_t> openedChests;
    std::map<std::string, double> clearedSpawns;  // chunk key -> respawn time
    int reputation[FA_COUNT] = {0};
    std::vector<uint8_t> fog;              // map discovery 128x128
    std::set<std::string> dungeonsDone;
    std::set<std::string> treasuresFound;
    int saveSlot = 0;
    float autosaveT = 0;
    double playSeconds = 0;

    // ---- UI state -------------------------------------------------------------------------------
    std::deque<Notification> notes;
    std::vector<FloatText> floatTexts;
    DialogState dialog;
    int shopNpc = -1;
    int menuTab = 0, selItem = -1, selSlot = -1, selRecipe = -1, selSkill = -1, selQuest = -1, selEnemy = -1;
    int craftStation = ST_ANY;
    float mapZoom = 1.f; vec2 mapPan;
    std::string bossBarName;
    Entity bossEnt = NullEntity;
    float bossBarShow = 0;
    float deathT = 0;
    float levelUpT = 0;
    float fps = 60, fpsAcc = 0; int fpsFrames = 0;
    bool layoutEdit = false; int layoutDrag = -1;
    float titleCamT = 0;
    int titleSel = 0;
    float creditsT = 0;
    std::string toastHint;
    float hintT = 0;
    GLuint mapTex = 0;
    RenderTarget iconRT; bool iconsReady = false;
    std::map<std::string, vec4> iconUV;      // 3D-rendered item icons (uv rect in iconRT)
    RenderTarget portraitRT; int portraitNpc = -2;

    // ---- camera ---------------------------------------------------------------------------------
    Camera cam;
    float camYaw = 0.78f, camPitch = 0.66f, camDist = 15.5f, camDistTarget = 15.5f;
    vec3 camFocus;
    float shake = 0;
    float cinematicT = -1;
    vec3 cineFrom, cineTo, cineLook;
    std::string cinematicText;
    float dofFocus = 20, dofAmount = 0;

    // ---- environment -----------------------------------------------------------------------------
    Environment env;
    enum Weather { W_CLEAR = 0, W_CLOUDY, W_RAIN, W_STORM, W_SNOW, W_FOG, W_ASHFALL, W_COUNT };
    int weather = W_CLEAR, weatherNext = W_CLEAR;
    float weatherBlend = 1, weatherTimer = 120, rainAmt = 0, snowAmt = 0, fogAmt = 0, stormAmt = 0, ashAmt = 0, lightningT = 0, lightningFlash = 0;
    vec3 regionTint;  // smoothed per-region grading blend
    float regionW[R_COUNT] = {1, 0, 0, 0, 0, 0};

    // ---- dungeon ---------------------------------------------------------------------------------
    DungeonState dungeon;
    std::string dungeonPrompt;
    int nearDungeon = -1;
    float transitionT = -1;
    std::function<void()> onTransition;

    // ---- shared meshes ---------------------------------------------------------------------------
    std::map<std::string, std::shared_ptr<Mesh>> meshCache;
    std::shared_ptr<Mesh> windmillBlades, chestMesh, chestOpenMesh, pickupMesh, arrowMesh, boulderMesh, orbMesh, gateMesh, brazierMesh, bladeMesh, spikeMesh, leverMesh, nodeMeshes[12];
    std::map<uint32_t, Entity> nodeEntities;   // uid -> entity
    std::set<uint32_t> spawnedChunks;          // chunk hash with spawned creatures
    std::map<uint32_t, std::vector<Entity>> chunkSpawns;
    std::vector<int> villageNpcsSpawned;

    // ---- audio ids --------------------------------------------------------------------------------
    std::map<std::string, SoundId> sfx;
    void play(const std::string& s, float vol = 1, float pitch = 1);
    void play3D(const std::string& s, vec3 p, float vol = 1, float pitch = 1, float range = 32);
    float footstepT = 0;

    // ============================================================== methods (split across files)
    // Game.cpp
    void newGame();
    void startPlaying();
    void updatePlaying(float dt);
    void renderWorld(float dt);
    void renderCharacter(Entity e, Character& c);
    void updateCamera(float dt);
    void updateEnvironment(float dt);
    void updateAmbientFx(float dt);
    void updateAudioDirector(float dt);
    void loadAssets();
    void buildSharedMeshes();
    float groundAt(float x, float z) const;
    float waterAt(float x, float z, bool* lava = nullptr, bool* frozen = nullptr) const;
    bool collideStatic(vec3& p, float r, float feet, float* step = nullptr) const;
    void notify(const std::string& text, vec3 color = vec3(1, 0.92f, 0.7f), const std::string& icon = "");
    void floatText(vec3 p, const std::string& t, vec3 col, float size = 22);
    void haptic(int ms, int amp);
    void addShake(float s) { if (settings.cameraShake) shake = std::max(shake, s); }
    void startCinematic(vec3 from, vec3 to, vec3 look, float dur, const std::string& text);
    void fadeTransition(std::function<void()> fn);
    void requestScreenshot(const std::string& path) { pendingScreenshot = path; }

    // Spawning (Spawn.cpp)
    Entity spawnPlayer(vec3 pos);
    Entity spawnEnemy(int def, vec3 pos, bool elite = false, uint32_t uid = 0);
    Entity spawnBoss(int bossIdx, vec3 pos);
    Entity spawnNpc(int def, vec3 pos, int village);
    Entity spawnVillager(vec3 pos, int village, uint32_t seed);
    Entity spawnAnimal(int kind, vec3 pos);
    Entity spawnMount(int def, vec3 pos);
    Entity spawnPickup(const std::string& item, int count, Rarity r, vec3 pos);
    void spawnNode(const NodeSpawn& n);
    void updateSpawning(float dt);
    void despawnFar();
    void setupVisualHumanoid(Visual& v, const HumanoidLook& L, WeaponType wt, WeaponType ot, float scale);
    void setupVisualCreature(Visual& v, Arch a, const CreatureLook& L, float scale);
    std::shared_ptr<Mesh> cachedMesh(const std::string& key, const std::function<void(MeshBuilder&)>& build);
    std::shared_ptr<Mesh> weaponMesh(WeaponType t, int tier, vec3 accent, float* tip = nullptr);

    // Player.cpp
    void updatePlayer(float dt);
    void playerAttack(bool heavy, bool charged);
    void playerDodge();
    void playerJump();
    void playerInteract();
    void castSpell(int slot);
    void usePotion();
    void toggleMount();
    void recomputeStats();
    void rebuildPlayerVisual();
    void gainXp(int amount);
    void findInteractTarget();
    void updateFishing(float dt);
    Character& pc();

    // Combat.cpp
    void updateCharacters(float dt);
    void updateAttack(Entity e, Character& c, float dt);
    void startAttack(Entity e, Character& c, const Clip* clip, float mult, Element elem, float speed = 1.f);
    float dealDamage(Entity src, Entity dst, float amount, Element elem, vec3 dir, float poiseDmg, bool canCrit = true, bool isSpell = false);
    void areaDamage(Entity src, vec3 center, float radius, float amount, Element elem, Team team, float poise);
    void killCharacter(Entity e, Character& c, Entity killer);
    void dropLoot(Character& c, vec3 pos);
    void updateProjectiles(float dt);
    void fireProjectile(Entity owner, ProjKind k, vec3 from, vec3 dir, float speed, float dmg, Element e, Team team, Entity homing = NullEntity);
    void updateStatus(Entity e, Character& c, float dt);
    void applyStatus(Character& c, Element e, float power);
    void updateTrails(Character& c, float dt);
    void physicsStep(Entity e, Character& c, float dt);
    Entity findTarget(vec3 pos, float yaw, float range, Team team, float cone = 0.6f);
    void hitFx(vec3 p, vec3 dir, Element e, float scale, bool metal);

    // AI.cpp
    void updateEnemyAI(Entity e, Character& c, AIBrain& ai, float dt);
    void updateBossAI(Entity e, Character& c, AIBrain& ai, float dt);
    void updateNpc(Entity e, Character& c, NpcBrain& nb, float dt);
    void updateAnimal(Entity e, Character& c, AnimalBrain& ab, float dt);
    void executeEnemyAttack(Entity e, Character& c, AIBrain& ai, const AttackDef& a);
    vec2 steer(Character& c, vec3 target, float dt);
    bool canSee(const Character& a, const Character& b, float range, float fov);

    // Rpg.cpp
    bool addItem(const std::string& id, int count = 1, Rarity r = Rarity::Count, bool notifyUser = true);
    int countItem(const std::string& id) const;
    bool removeItem(const std::string& id, int count);
    void equip(int invIndex);
    void unequip(int slot);
    void useItem(int invIndex);
    void sellItem(int invIndex);
    bool buyItem(const std::string& id, int price);
    bool canCraft(const Recipe& r) const;
    bool craft(int recipeIdx);
    bool upgradeItem(int slot);
    void startQuest(const std::string& id);
    void advanceQuests(StepKind kind, const std::string& target, int amount = 1);
    void completeQuest(QuestState& q);
    bool questActive(const std::string& id) const;
    QuestState* questState(const std::string& id);
    void checkQuestGivers();
    void openDialog(Entity npcEnt);
    void dialogAction(const std::string& action);
    void discover(const std::string& poi);
    bool unlockSkill(int idx);
    void learnSpell(const std::string& id);
    void addReputation(int faction, int amount);
    int itemPrice(const std::string& id, bool selling) const;
    std::string questObjectiveText() const;
    vec3 questMarker(bool* has) const;
    void restAtInn();
    bool saveGame(int slot);
    bool loadGame(int slot);
    std::string saveSummary(int slot) const;
    void saveSettings();
    void loadSettings();
    void applyGraphicsProfile(QualityProfile p);

    // Dungeon.cpp
    void enterDungeon(int idx);
    void exitDungeon();
    void buildDungeon(int idx);
    void updateDungeon(float dt);
    void renderDungeon();
    bool dungeonFloor(float x, float z, float& y) const;

    // Hud.cpp / Menus.cpp
    void updateTouchControls();
    void drawHud(float dt);
    void drawMinimap(Rect r);
    void drawDialog();
    void drawMenus(float dt);
    void drawTitle(float dt);
    void drawSplash(float dt);
    void drawCredits(float dt);
    void drawNotifications(float dt);
    void drawLayoutEditor();
    void renderItemIcons();
    void renderPortrait(int npc);
    bool itemIcon(const std::string& id, Rect r, float alpha = 1.f);
    vec2 worldToScreen(vec3 p, bool* onScreen = nullptr) const;
    std::vector<int> touchCaptured;   // finger ids owned by controls
    int joyFinger = -1, camFinger = -1;
    vec2 joyOrigin, joyPos;
    float pinchDist = 0;
    std::map<int, float> btnHold;

    // AutoPlay.cpp (desktop automated play-test)
    bool autoplay = false;
    std::string autoplayScenario;
    float autoplayT = 0;
    int autoplayStep = 0;
    void updateAutoplay(float dt);
};

extern Game* gGame;
float skillBonus(const std::set<std::string>& skills, float SkillNode::*field);
float skillBonusPublic(const std::set<std::string>& skills, int which);  // 0 bow, 1 dagger, 2 beasts

}  // namespace lk
