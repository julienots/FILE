#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>
#include <cstring>

namespace lk {

// Lightweight CPU section profiler (desktop autoplay reports it; negligible cost on device).
struct SectionProf { const char* name; double acc = 0; };
static SectionProf gProf[] = {{"player"}, {"characters+AI"}, {"projectiles"}, {"spawning"}, {"dungeon"}, {"environment"}, {"ambientFx"}, {"particles"}, {"audioDir"}, {"renderWorld"}, {"hud+menus"}};
static int gProfFrames = 0;
#define PROF(i, stmt) do { double _t = platform::timeSeconds(); stmt; gProf[i].acc += platform::timeSeconds() - _t; } while (0)
void Game::reportProfile() {
    if (gProfFrames == 0) return;
    std::string out = "cpu profile (ms/frame):";
    for (auto& p : gProf) { char b[64]; snprintf(b, sizeof b, " %s %.2f", p.name, p.acc * 1000.0 / gProfFrames); out += b; p.acc = 0; }
    AGR_LOG("%s over %d frames", out.c_str(), gProfFrames);
    std::string g = renderer.gpuReport(); if (!g.empty()) AGR_LOG("%s", g.c_str());
    gProfFrames = 0;
}


Game* gGame = nullptr;

}  // namespace lk

agr::App* agr::createApp() { lk::gGame = new lk::Game(); return lk::gGame; }

namespace lk {

// ============================================================================================ lifecycle
bool Game::init(int w, int h) {
    W = w; H = h;
    double t0 = platform::timeSeconds();
    loadSettings();
    applyMods();   // content patches + asset overrides of enabled mods, before any asset or data is used
    for (auto& a : platform::launchArgs()) {
        if (a.rfind("--autoplay=", 0) == 0) { autoplay = true; autoplayScenario = a.substr(11); }
        if (a.rfind("--profile=", 0) == 0) settings.profile = (QualityProfile)std::atoi(a.c_str() + 10);
    }
    if (!settingsFileFound && !autoplay) {
        // first launch: pick the quality profile from the GPU (the player can change it in Settings)
        const char* gr = (const char*)glGetString(GL_RENDERER);
        settings.profile = suggestProfileForGpu(gr ? gr : "");
        AGR_LOG("first launch: GPU '%s' -> profile %s", gr ? gr : "?", profileName(settings.profile));
    }
    GraphicsSettings gs = GraphicsSettings::forProfile(settings.profile);
    gs.chromatic = settings.chromatic;
    renderer.width = W; renderer.height = H;
    if (!renderer.init(gs)) return false;
    if (!ui.init()) { AGR_ERR("UI assets missing"); return false; }
    particles.init(5000);
    particles.setBudget(gs.particleScale);
    initPropTemplates();
    if (!gpuReady) loadPropModels();   // imported building models (and mod overrides), before the chunk workers
    initClips();
    world.grassDensity = gs.grassDensity;
    world.detailedFoliage = settings.profile <= QualityProfile::Medium;
    if (!gpuReady) {
        world.buildPlan();
        farTerrainBuild = std::async(std::launch::async, [this] { auto mb = std::make_shared<MeshBuilder>(); world.buildFarMesh(*mb, 168); return mb; });
        world.startWorkers(std::max(1, std::min(3, platform::cpuCount() - 1)));
        std::thread([this] { world.generateMapImage(); }).detach();
        loadAssets();
    }
    buildSharedMeshes();
    gpuReady = true;
    fog.assign(128 * 128, 0);
    state = autoplay ? GameState::Title : GameState::Splash;
    if (autoplay && (autoplayScenario == "splash" || autoplayScenario == "full" || autoplayScenario == "logo")) state = GameState::Splash;
    stateT = 0;
    audio.setVolumes(settings.master, settings.music, settings.sfx, settings.ambience);
    AGR_LOG("Lost Kingdom initialised in %.0f ms (%dx%d, profile %s)", (platform::timeSeconds() - t0) * 1000, W, H, profileName(settings.profile));
    return true;
}

void Game::resize(int w, int h) { W = w; H = h; renderer.resize(w, h); }

void Game::pause() {
    audio.setPaused(true);
    if (state == GameState::Playing && player != NullEntity) { saveGame(saveSlot); saveSettings(); }  // autosave slot on focus loss
}

void Game::resume() { audio.setPaused(false); }

void Game::contextLost() {
    // All GL objects are invalid: forget them without calling glDelete, rebuild on next init.
    renderer = Renderer();
    ui = UI();
    particles = ParticleSystem();
    meshCache.clear();
    for (auto& kv : importedModels) if (kv.second) { kv.second->mesh.reset(); kv.second->tex = Texture(); }
    for (int i = 0; PropModel* P = propModel(i); i++) { P->mesh.reset(); P->lodMesh.reset(); P->tex = Texture(); }   // building models: rebuilt on the next draw
    farTerrain.reset();
    windmillBlades.reset(); chestMesh.reset(); chestOpenMesh.reset(); pickupMesh.reset(); arrowMesh.reset(); boulderMesh.reset(); orbMesh.reset();
    gateMesh.reset(); brazierMesh.reset(); bladeMesh.reset(); spikeMesh.reset(); leverMesh.reset();
    for (auto& m : nodeMeshes) m.reset();
    world.unloadAll();
    reg.each<Character>([&](Entity, Character& c) { c.vis.albedo = 0; c.vis.mesh.reset(); c.vis.weapon.reset(); c.vis.offhand.reset(); c.vis.capeMesh.reset(); c.vis.trailMesh.reset(); c.vis.hairMesh.reset(); });
    reg.each<NodeComp>([&](Entity, NodeComp& n) { n.mesh.reset(); });
    iconsReady = false; mapTex = 0; portraitNpc = -2;
    dungeon.floor = Mesh(); dungeon.walls = Mesh(); dungeon.props = Mesh();
    if (dungeon.active) buildDungeon(dungeon.def);
    gpuReady = true;  // CPU-side data remains; init() recreates GPU objects
}

void Game::lowMemory() {
    world.unloadAll();
    meshCache.clear();
}

void Game::shutdown() {
    if (state == GameState::Playing) saveGame(saveSlot);
    saveSettings();
    world.stopWorkers();
    particles.shutdown();
    ui.shutdown();
    renderer.shutdown();
}

bool Game::back() {
    if (dialog.open) { dialog.open = false; return true; }
    if (menu != Menu::None) { menu = layoutEdit ? Menu::Settings : Menu::None; layoutEdit = false; return true; }
    if (state == GameState::Playing) { menu = Menu::Pause; return true; }
    if (state == GameState::Credits) { state = GameState::Title; return true; }
    return false;  // title screen: let Android handle (background the app)
}

void Game::touch(const TouchEvent& e) { ui.onTouch(e); }

void Game::haptic(int ms, int amp) { if (settings.haptics) platform::vibrate(ms, amp); }

// 2.3.4 a repeated sound never plays twice identically: a small random pitch / level spread (except UI and jingles)
static float sfxVary(const std::string& s, float& vol) {
    static Rng r(9127);
    if (s.rfind("ui_", 0) == 0 || s == "levelup" || s == "quest" || s == "discover" || s == "boss_intro" || s == "engine") return 1.f;
    vol *= r.range(0.88f, 1.f);
    return r.range(0.955f, 1.045f);
}
void Game::play(const std::string& s, float vol, float pitch) {
    auto it = sfx.find(s);
    if (it != sfx.end()) { float v = vol; float k = sfxVary(s, v); audio.play(it->second, v, pitch * k); }
}
// 2.3.4 base, base2, base3...: a random variant, never the same one twice in a row
std::string Game::variant(const std::string& base) {
    static std::unordered_map<std::string, int> last;
    static Rng r(4411);
    int n = 1; while (sfx.count(base + std::to_string(n + 1))) n++;
    if (n == 1) return base;
    int k = r.irange(0, n - 1);
    if (k == last[base]) k = (k + 1) % n;
    last[base] = k;
    return k == 0 ? base : base + std::to_string(k + 1);
}
void Game::play3D(const std::string& s, vec3 p, float vol, float pitch, float range) {
    auto it = sfx.find(s);
    if (it != sfx.end()) { float v = vol; float k = sfxVary(s, v); audio.play3D(it->second, p, v, pitch * k, range); }
}

void Game::notify(const std::string& text, vec3 color, const std::string& icon) {
    notes.push_back({text, color, 0, icon});
    while (notes.size() > 5) notes.pop_front();
}

void Game::floatText(vec3 p, const std::string& t, vec3 col, float size) { floatTexts.push_back({p, t, col, 0, size}); }

void Game::startCinematic(vec3 from, vec3 to, vec3 look, float dur, const std::string& text) {
    cineFrom = from; cineTo = to; cineLook = look; cinematicT = dur; cinematicText = text;
}

void Game::fadeTransition(std::function<void()> fn) { transitionT = 0; onTransition = fn; }

float Game::groundAt(float x, float z) const {
    float y;
    if (dungeon.active && dungeonFloor(x, z, y)) return y;
    return world.groundHeight(x, z);
}

float Game::waterAt(float x, float z, bool* lava, bool* frozen) const {
    if (dungeon.active && x > WORLD_SIZE) { if (lava) *lava = false; if (frozen) *frozen = false; return -1e9f; }
    return world.waterLevel(x, z, lava, frozen);
}

bool Game::collideStatic(vec3& p, float r, float feet, float* step) const {
    if (dungeon.active && p.x > WORLD_SIZE) {
        bool hit = false;
        for (const Collider& c : dungeon.colliders) {
            vec2 l = rotate(p.xz() - c.c, -c.rot);
            vec2 q{clampf(l.x, -c.hx, c.hx), clampf(l.y, -c.hz, c.hz)};
            vec2 d = l - q;
            float dl = length(d);
            if (dl < r) {
                vec2 push = dl > 1e-5f ? d * ((r - dl) / dl) : vec2(0, r);
                vec2 w = rotate(push, c.rot);
                p.x += w.x; p.z += w.y; hit = true;
            }
        }
        return hit;
    }
    return world.collide(p, r, feet, step);
}

// ============================================================================================ main loop
// Dynamic resolution: keep the profile's frame rate by lowering the internal resolution when frames are too
// slow, and raising it again after a stable period (that period doubles after a failed raise: no oscillation).
void Game::updateDynamicResolution(float dtRaw) {
    if (platform::isHeadlessTest() || state != GameState::Playing || menu != Menu::None) { drsAcc = 0; drsFrames = 0; return; }
    if (dtRaw > 0.25f) return;  // hitches (loading, app switch) are not representative
    drsAcc += dtRaw; drsFrames++;
    if (drsAcc < 2.f) return;
    float avg = drsAcc / drsFrames; drsAcc = 0; drsFrames = 0;
    float target = 1.f / std::max(20, renderer.settings.fpsCap);
    float k = renderer.dynamicScale;
    if (avg > target * 1.18f && k > 0.85f) {   // 3.1.0 floor 0.8 (3.0.2: 0.75, before 0.5): sharper grass when the phone is busy
        if (drsJustRaised) drsWaitUp = std::min(120.f, drsWaitUp * 2.f);
        renderer.setDynamicScale(k - 0.1f); drsStable = 0; drsJustRaised = false;
        AGR_LOG("dynamic resolution: %.0f ms/frame -> scale %.2f", avg * 1000, renderer.dynamicScale);
    } else if (avg < target * 1.06f) {
        drsStable += 2.f;
        if (drsStable >= drsWaitUp && k < 0.99f) { renderer.setDynamicScale(k + 0.1f); drsStable = 0; drsJustRaised = true; AGR_LOG("dynamic resolution: raise -> scale %.2f", renderer.dynamicScale); }
        else if (drsStable > 20.f) drsJustRaised = false;
    } else drsStable = 0;
}

void Game::frame(float dtRaw) {
    float dt = std::min(dtRaw, 1.f / 15.f);
    realTime += dt;
    stateT += dt;
    fpsAcc += dtRaw; fpsFrames++;
    updateDynamicResolution(dtRaw);
    if (fpsAcc > 0.5f) { fps = fpsFrames / fpsAcc; fpsAcc = 0; fpsFrames = 0; }
    ui.begin(W, H, dt);
    // level-triggered inputs are rebuilt every frame (by touch controls, or by the autoplay bot)
    input.move = vec2(0, 0); input.attackHeld = false; input.block = false; input.sprint = false;
    if (autoplay) updateAutoplay(dt);

    switch (state) {
    case GameState::Splash: {
        // the studio splash plays over the live world fly-over (title world streams in meanwhile)
        updateEnvironment(dt);
        renderWorld(dt);
        drawSplash(dt);
        break;
    }
    case GameState::Title: {
        updateEnvironment(dt);
        particles.update(dt);
        updateAmbientFx(dt);
        renderWorld(dt);
        drawTitle(dt);
        break;
    }
    case GameState::Credits: {
        updateEnvironment(dt);
        renderWorld(dt);
        drawCredits(dt);
        break;
    }
    case GameState::Loading: case GameState::Playing: {
        // Touch controls BEFORE the simulation: taps (attack, dodge, jump, spells...) are one-frame edges that
        // input.clearEdges() erases at the end of the frame, so they must be produced before updatePlaying.
        updateTouchControls();
        if (menu == Menu::Mods || std::fmod(realTime, 2.f) < dt) pollModImport();
        updateCinematicShots(dt);   // multi-shot cinematics own the taps while they play
        bool paused = (menu != Menu::None && menu != Menu::Death) || loadGate;
        float sdt = paused ? 0.f : dt * timeScale;
        if (hitStop > 0) { hitStop -= dt; sdt *= 0.08f; }
        updateCoop(dt);   // 1.13.0 the network runs even while a menu is open
        if (tele.active) { Input keep = input; input = Input(); input.camDrag = keep.camDrag; input.pinch = keep.pinch; }   // 2.2.0 no control while turning to dust
        if (!paused) { updateTutorial(sdt); updatePlaying(sdt); }
        if (!paused) updateTeleportFx(sdt);
        PROF(9, renderWorld(sdt));
        PROF(10, { drawHud(dt); if (dialog.open) drawDialog(); drawMenus(dt); drawEndingChoice(dt); });
        break;
    }
    }
    // 2.1.0 loading gate: after a game starts or loads, hold black until the hero's surroundings are streamed in
    if (loadGate) {
        loadGateT += dt;
        loadGateReady = std::max(loadGateReady, state == GameState::Playing && player != NullEntity ? std::min(world.areaReady(pc().pos, 80.f), world.areaReady(pc().pos, 170.f) * 1.02f) : 0.f);
        if (!cine.active && ((loadGateReady >= 0.995f && loadGateT >= loadGateHold) || loadGateT > 10.f)) { loadGate = false; loadGateFade = 1.f; }
        else if (cine.active) loadGate = false;
        else drawLoadOverlay(ui.W, ui.H, loadGateReady, 1.f, loadGateT);
    } else if (loadGateFade > 0.f) {
        loadGateFade = std::max(0.f, loadGateFade - dt * 1.6f);
        ui.rect({0, 0, ui.W, ui.H}, vec4(0.02f, 0.015f, 0.01f, loadGateFade));
    }
    if (!endingChoice && !loadGate) drawNotifications(dt);   // 1.12.0 the ending choice stands alone on screen
    // screen transition fade (dungeon entrances)
    if (transitionT >= 0) {
        transitionT += dt;
        float a = transitionT < 0.4f ? transitionT / 0.4f : std::max(0.f, 1.f - (transitionT - 0.55f) / 0.45f);
        ui.rect({0, 0, ui.W, ui.H}, vec4(0, 0, 0, a));
        if (transitionT >= 0.45f && onTransition) { auto fn = onTransition; onTransition = nullptr; fn(); }
        if (transitionT > 1.0f) transitionT = -1;
    }
    if (settings.showFps) {
        char b[96];
        snprintf(b, sizeof b, "%.0f FPS · %d draw · %dk tri · %zu chunks", fps, renderer.stats.drawCalls, renderer.stats.triangles / 1000, world.loadedChunks());
        ui.text(b, 12, ui.H - 26, 16, vec4(1, 1, 1, 0.8f), F_BODY, A_LEFT, 0, true);
    }
    ui.end(renderer.uiShader());
    ui.endInput();
    input.clearEdges();
    if (!pendingScreenshot.empty() && autoplay) reportProfile();
    if (!pendingScreenshot.empty()) { { const int* k = renderer.stats.triKind; AGR_LOG("render stats: %d draw calls, %dk triangles (%s) [terrain %dk far %dk grass %dk foliage %dk models %dk skinned %dk props %dk]", renderer.stats.drawCalls, renderer.stats.triangles / 1000, pendingScreenshot.c_str(), k[0] / 1000, k[1] / 1000, k[2] / 1000, k[3] / 1000, k[4] / 1000, k[5] / 1000, k[6] / 1000); } platform::requestScreenshot(pendingScreenshot); pendingScreenshot.clear(); }
}

void Game::updatePlaying(float dt) {
    gameTime += dt;
    playSeconds += dt;
    // time of day: 1 in-game hour = 60 s
    timeOfDay += dt / 60.f * gameTuning().timeSpeed;   // 2.7.0 mods: time speed (0 = frozen)
    if (gameTuning().hour >= 0.f) timeOfDay = gameTuning().hour;   // 2.7.0 mods: a fixed hour (eternal night...)
    if (timeOfDay >= 24.f) { timeOfDay -= 24.f; day++; }
    gProfFrames++;
    PROF(0, updatePlayer(dt));
    updateGeysers(dt);   // 2.8.0
    updateTreasures(dt);   // 3.2.0
    updateForestry(dt);    // 3.6.0
    updateVfx(dt);       // 3.0.1
    PROF(1, updateCharacters(dt));
    PROF(2, updateProjectiles(dt));
    if (!cine.active) PROF(3, updateSpawning(dt));
    updateWorldEvents(dt);   // 2.3.6
    updateBombs(dt);         // 2.4.0
    if (dungeon.active) digMode = false;
    if (!cine.active) updateNightHunters(dt);
    updateBase(dt);
    if (!cine.active) updateFortresses(dt);   // 3.0.0 the Iron Legion
    updateRaces(dt);
    updateChallenges(dt);
    updateStory(dt);
    // 1.11.0 vehicles: parked at Brindlemere on a new world; their interaction entities survive registry resets
    if (vehicles.empty()) initVehicles();
    if (vehicleEnts.size() != vehicles.size() || (!vehicleEnts.empty() && !reg.get<VehicleRef>(vehicleEnts[0]))) spawnVehicleEntities();
    PROF(4, updateDungeon(dt));
    PROF(5, updateEnvironment(dt));
    PROF(6, updateAmbientFx(dt));
    PROF(7, particles.update(dt));
    PROF(8, updateAudioDirector(dt));
    reg.flush();
    // autosave every 3 minutes (outside combat)
    autosaveT += dt;
    if (autosaveT > 180.f && bossEnt == NullEntity) { autosaveT = 0; saveGame(saveSlot); notify("Sauvegarde automatique", vec3(0.7f, 0.85f, 1.f), "i_save"); }
    // notifications & floating texts
    for (auto& f : floatTexts) { f.t += dt; f.pos.y += dt * 1.2f; }
    floatTexts.erase(std::remove_if(floatTexts.begin(), floatTexts.end(), [](const FloatText& f) { return f.t > 1.2f; }), floatTexts.end());
    if (cinematicT > 0) cinematicT -= dt;
    if (levelUpT > 0) levelUpT -= dt;
    if (regionBannerT > 0) regionBannerT -= dt;
}

// ============================================================================================ environment
struct SkyKey { float h; uint32_t sun, sky, ground, fog, top, horizon; float intensity, amb; };
static const SkyKey SKY[] = {
    {0.f, 0x8aa4ff, 0x1a2440, 0x0e1018, 0x141a2c, 0x050a18, 0x1a2440, 0.35f, 0.55f},
    {4.5f, 0x8aa4ff, 0x1c2640, 0x0e1018, 0x1a2032, 0x060c1c, 0x1e2848, 0.3f, 0.55f},
    {5.6f, 0xff9a6a, 0x5a4a6a, 0x2a2228, 0x8a6a7a, 0x2a3a6a, 0xff9a7a, 1.1f, 0.7f},
    {7.f, 0xffd2a0, 0x8aa0c8, 0x5a4a38, 0xb8c0cc, 0x3a6ab0, 0xf0c8a8, 2.4f, 0.9f},
    {10.f, 0xfff4e4, 0x9ab4d8, 0x6a5a44, 0xb8cad8, 0x2f64b8, 0xb8d0e8, 3.1f, 1.f},
    {14.f, 0xfff6ea, 0x9ab4d8, 0x6a5a44, 0xbccbd8, 0x2f64b8, 0xb8d0e8, 3.2f, 1.f},
    {17.f, 0xffd8a0, 0x8a9ec0, 0x6a5038, 0xd0b8a0, 0x3a6aa8, 0xf0c090, 2.6f, 0.9f},
    {18.8f, 0xff7a40, 0x6a4a6a, 0x3a2420, 0xb07060, 0x3a3a7a, 0xff8a50, 1.3f, 0.75f},
    {20.f, 0x9a8aff, 0x2a2a50, 0x141218, 0x2a2a44, 0x0c1028, 0x3a3060, 0.45f, 0.6f},
    {24.f, 0x8aa4ff, 0x1a2440, 0x0e1018, 0x141a2c, 0x050a18, 0x1a2440, 0.35f, 0.55f},
};

struct RegionGrade { vec3 lift, gamma, gain; float sat, contrast, temp, fogMul, fogBase; vec3 fogTint; float exposure; };
static const RegionGrade GRADE[R_COUNT] = {
    {{0.0f, 0.0f, 0.01f}, {1.0f, 1.0f, 1.0f}, {1.03f, 1.0f, 0.95f}, 0.98f, 1.1f, 0.15f, 1.0f, 0, {1, 1, 1}, 1.0f},          // Greenhaven
    {{0.01f, 0.02f, 0.04f}, {1.0f, 1.0f, 1.02f}, {0.95f, 0.99f, 1.06f}, 0.95f, 1.08f, -0.5f, 1.1f, 0, {0.85f, 0.9f, 1.0f}, 0.72f}, // Frostheim
    {{0.02f, 0.0f, 0.0f}, {1.02f, 0.99f, 0.97f}, {1.04f, 0.97f, 0.9f}, 0.92f, 1.1f, 0.3f, 1.2f, 0, {1.05f, 0.88f, 0.78f}, 1.0f},     // Ashen
    {{0.0f, 0.01f, 0.02f}, {1.0f, 1.01f, 1.01f}, {0.96f, 1.02f, 1.0f}, 0.96f, 1.08f, -0.1f, 1.3f, 0, {0.85f, 1.0f, 0.98f}, 1.0f},  // Elderwood
    {{0.01f, 0.02f, 0.02f}, {0.98f, 1.0f, 0.98f}, {0.92f, 0.98f, 0.9f}, 0.78f, 1.1f, -0.1f, 1.4f, 0, {0.62f, 0.72f, 0.64f}, 0.92f},   // Shadows
    {{0.0f, 0.0f, 0.01f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.02f}, 0.96f, 1.12f, 0.0f, 0.8f, 0, {1, 1, 1.05f}, 1.0f},              // High Crowns
    {{0.02f, 0.005f, 0.0f}, {1.02f, 1.0f, 0.97f}, {1.07f, 0.98f, 0.84f}, 1.06f, 1.14f, 0.55f, 0.65f, 0, {1.05f, 0.92f, 0.78f}, 1.08f},    // Amber Sands: blazing, orange-gold
    {{0.0f, 0.012f, 0.02f}, {1.0f, 1.0f, 1.0f}, {0.97f, 1.04f, 1.03f}, 1.16f, 1.1f, 0.1f, 0.75f, 0, {0.9f, 1.0f, 1.05f}, 1.04f},          // Coral Coast: saturated, turquoise air
    {{0.015f, 0.01f, 0.0f}, {1.01f, 1.0f, 0.98f}, {1.06f, 1.0f, 0.86f}, 1.04f, 1.08f, 0.35f, 0.6f, 0, {1.02f, 0.97f, 0.86f}, 1.06f},     // 2.4.0 Valdor Steppes: golden light, clear far horizon
    {{0.0f, 0.015f, 0.01f}, {1.0f, 1.0f, 1.0f}, {0.94f, 1.04f, 0.94f}, 1.12f, 1.1f, -0.05f, 1.45f, 0, {0.78f, 0.95f, 0.82f}, 0.96f},     // 2.4.0 Emerald Jungle: humid green haze
    {{0.0f, 0.006f, 0.014f}, {1.0f, 1.0f, 1.0f}, {0.98f, 1.0f, 1.03f}, 0.97f, 1.1f, -0.18f, 1.05f, 0, {0.88f, 0.93f, 1.0f}, 1.0f},     // 2.8.0 Lands of Aurora: clean, cool northern light
    {{0.0f, 0.004f, 0.012f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.02f}, 1.0f, 1.12f, -0.05f, 0.7f, 0, {0.92f, 0.96f, 1.04f}, 1.02f},     // 3.0.0 Titan Mountains: crisp alpine air, far views
    {{0.02f, 0.004f, 0.0f}, {1.02f, 1.0f, 0.97f}, {1.08f, 0.96f, 0.84f}, 1.08f, 1.14f, 0.5f, 0.6f, 0, {1.08f, 0.88f, 0.76f}, 1.06f},   // 3.3.0 Scarlet Canyons: red heat, long shadows
};

void Game::updateEnvironment(float dt) {
    // --- sky keys for the time of day
    float h = timeOfDay;
    int k = 0;
    while (k < 9 && SKY[k + 1].h <= h) k++;
    const SkyKey &a = SKY[k], &b = SKY[std::min(k + 1, 9)];
    float t = b.h > a.h ? (h - a.h) / (b.h - a.h) : 0.f;
    auto L = [&](uint32_t x, uint32_t y) { return lerp(srgb(x), srgb(y), t); };
    float dayAngle = (h - 6.f) / 12.f * PI;   // 0 at sunrise, PI at sunset
    bool isDay = h > 5.5f && h < 18.9f;
    vec3 sunDir = normalize(vec3(std::cos(dayAngle) * 0.85f, std::max(0.12f, std::sin(dayAngle)), 0.45f));
    if (!isDay) { float na = (h < 6 ? h + 24 - 18.9f : h - 18.9f) / 11.f * PI; sunDir = normalize(vec3(-std::cos(na) * 0.7f, std::max(0.25f, std::sin(na)) * 0.9f, -0.35f)); }
    env.time = realTime;
    env.sunDir = sunDir;
    env.sunColor = L(a.sun, b.sun);
    env.sunIntensity = lerpf(a.intensity, b.intensity, t);
    env.skyAmbient = L(a.sky, b.sky) * lerpf(a.amb, b.amb, t);
    env.groundAmbient = L(a.ground, b.ground) * 0.9f;
    vec3 fogC = L(a.fog, b.fog);
    env.skyTop = L(a.top, b.top);
    env.skyHorizon = L(a.horizon, b.horizon);
    env.night = 1.f - smoothstepf(5.2f, 6.6f, h) + smoothstepf(18.5f, 20.f, h);
    env.night = saturate(env.night);

    // --- region blending (smoothed weights at the focus point)
    vec3 focus = (state == GameState::Playing && player != NullEntity) ? reg.get<Character>(player)->pos : camFocus;
    RegionWeights rw = world.regionAt(focus.x, focus.z);
    if (dungeon.active && focus.x > WORLD_SIZE) { int dr = dungeon.interior ? dungeon.interiorRegion : dungeons()[dungeon.def].region; for (int i = 0; i < R_COUNT; i++) rw.w[i] = i == dr ? 1.f : 0.f; }
    for (int i = 0; i < R_COUNT; i++) regionW[i] = damp(regionW[i], rw.w[i], 0.8f, dt);
    RegionGrade g{};
    float sum = 0;
    for (int i = 0; i < R_COUNT; i++) sum += regionW[i];
    for (int i = 0; i < R_COUNT; i++) {
        float w = regionW[i] / std::max(sum, 1e-4f);
        const RegionGrade& r = GRADE[i];
        g.lift += r.lift * w; g.gamma += r.gamma * w; g.gain += r.gain * w;
        g.sat += r.sat * w; g.contrast += r.contrast * w; g.temp += r.temp * w; g.fogMul += r.fogMul * w; g.fogTint += r.fogTint * w; g.exposure += r.exposure * w;
    }

    // --- weather state machine (per region)
    weatherTimer -= dt;
    if (gameTuning().weather >= 0) weatherNext = gameTuning().weather;   // 2.7.0 a mod fixes the weather (at once)
    if (weatherTimer <= 0 && state == GameState::Playing) {
        int r = rw.dominant;
        Rng wr((uint64_t)(gameTime * 13) + day);
        float p = wr.f();
        int next = W_CLEAR;
        switch (r) {
        case R_GREENHAVEN: next = p < 0.5f ? W_CLEAR : p < 0.72f ? W_CLOUDY : p < 0.9f ? W_RAIN : W_STORM; break;
        case R_FROSTHEIM: next = p < 0.35f ? W_CLEAR : p < 0.55f ? W_CLOUDY : W_SNOW; break;
        case R_ASHEN: next = p < 0.45f ? W_ASHFALL : p < 0.75f ? W_CLEAR : W_STORM; break;
        case R_ELDERWOOD: next = p < 0.45f ? W_CLEAR : p < 0.75f ? W_FOG : W_RAIN; break;
        case R_SHADOWS: next = p < 0.45f ? W_FOG : p < 0.75f ? W_RAIN : W_STORM; break;
        case R_AMBER: next = p < 0.55f ? W_CLEAR : p < 0.8f ? W_SANDSTORM : p < 0.92f ? W_CLOUDY : W_CLEAR; break;
        case R_CORAL: next = p < 0.5f ? W_CLEAR : p < 0.68f ? W_CLOUDY : p < 0.9f ? W_RAIN : W_STORM; break;
        case R_STEPPE: next = p < 0.6f ? W_CLEAR : p < 0.85f ? W_CLOUDY : p < 0.95f ? W_STORM : W_RAIN; break;   // 2.4.0 big skies, summer storms
        case R_CANYON: next = p < 0.6f ? W_CLEAR : p < 0.75f ? W_SANDSTORM : p < 0.9f ? W_CLOUDY : W_STORM; break;   // 3.3.0 dry heat, red dust storms
        case R_TITANS: next = p < 0.42f ? W_CLEAR : p < 0.66f ? W_CLOUDY : p < 0.82f ? W_FOG : p < 0.93f ? W_RAIN : W_SNOW; break;   // 3.0.0 mountain weather
        case R_AURORA: next = p < 0.5f ? W_CLEAR : p < 0.72f ? W_CLOUDY : p < 0.88f ? W_FOG : W_RAIN; break;   // 2.8.0 northern skies: clear nights, sea mist, showers
        case R_JUNGLE: next = p < 0.35f ? W_CLEAR : p < 0.55f ? W_FOG : p < 0.85f ? W_RAIN : W_STORM; break;     // 2.4.0 hot rain and morning mist
        default: next = p < 0.55f ? W_CLEAR : p < 0.75f ? W_CLOUDY : W_SNOW; break;
        }
        if (gameTuning().weather >= 0) next = gameTuning().weather;   // 2.7.0 a mod fixes the weather
        weatherNext = next;
        weatherTimer = wr.range(90.f, 240.f);
    }
    if (weatherNext != weather) { weatherBlend -= dt / 12.f; if (weatherBlend <= 0) { weather = weatherNext; weatherBlend = 0; } }
    else weatherBlend = std::min(1.f, weatherBlend + dt / 12.f);
    auto target = [&](int wv, int which) { return (wv == which) ? 1.f : 0.f; };
    float wb = weatherBlend;
    int wcur = weather;
    // regional overrides: snow instead of rain in the cold, ash in the volcanic lands
    float frostW = regionW[R_FROSTHEIM] + regionW[R_HIGHCROWNS] * 0.4f;
    float tRain = (target(wcur, W_RAIN) + target(wcur, W_STORM)) * wb * (1 - frostW);
    float tSnow = (target(wcur, W_SNOW) * wb + (target(wcur, W_RAIN) + target(wcur, W_STORM)) * wb * frostW) * (1 - regionW[R_ASHEN]);
    float tFog = target(wcur, W_FOG) * wb + regionW[R_SHADOWS] * 0.35f;
    float tStorm = target(wcur, W_STORM) * wb;
    float tAsh = target(wcur, W_ASHFALL) * wb + regionW[R_ASHEN] * 0.25f;
    float tSand = target(wcur, W_SANDSTORM) * wb * saturate(regionW[R_AMBER] * 1.6f);   // 2.0.0: the storms stay in the Amber Sands
    float tCloud = target(wcur, W_CLOUDY) * wb + tRain + tSnow * 0.7f + tStorm + tSand * 0.5f;
    if (dungeon.active) tRain = tSnow = tStorm = tAsh = tCloud = tSand = 0;
    rainAmt = damp(rainAmt, tRain, 0.5f, dt);
    snowAmt = damp(snowAmt, tSnow, 0.5f, dt);
    fogAmt = damp(fogAmt, saturate(tFog), 0.4f, dt);
    stormAmt = damp(stormAmt, tStorm, 0.5f, dt);
    ashAmt = damp(ashAmt, saturate(tAsh), 0.5f, dt);
    sandAmt = damp(sandAmt, saturate(tSand), 0.35f, dt);
    float cloud = saturate(tCloud);

    // lightning
    if (stormAmt > 0.3f) {
        lightningT -= dt;
        if (lightningT <= 0) {
            Rng lr((uint64_t)(realTime * 1000));
            lightningT = lr.range(5.f, 14.f);
            lightningFlash = 1.f;
            play("thunder", 0.5f + 0.4f * stormAmt, lr.range(0.8f, 1.1f));
        }
    }
    lightningFlash = std::max(0.f, lightningFlash - dt * 3.5f);
    float flash = lightningFlash > 0.5f ? 1.f : (lightningFlash > 0.25f && std::fmod(realTime * 30.f, 2.f) < 1.f ? 0.4f : 0.f);

    // --- compose environment
    float overcast = cloud * 0.55f + fogAmt * 0.25f + ashAmt * 0.3f + sandAmt * 0.4f;
    env.sunIntensity *= (1.f - overcast * 0.75f);
    env.shadowStrength = 1.f - overcast * 0.7f;
    env.skyAmbient = lerp(env.skyAmbient, env.skyAmbient * 0.75f + vec3(0.05f), overcast);
    env.cloudShadows = 0.35f + cloud * 0.4f;
    env.cloudCover = 0.3f + cloud * 0.6f;
    env.fogColor = lerp(fogC, fogC * 0.7f + vec3(0.12f), overcast) * g.fogTint;
    if (ashAmt > 0.01f) env.fogColor = lerp(env.fogColor, srgb(0x5a4a44) * (0.3f + 0.7f * (1 - env.night)), ashAmt * 0.4f);
    if (fogAmt > 0.01f) env.fogColor = lerp(env.fogColor, env.fogColor * 0.75f, fogAmt);
    if (sandAmt > 0.01f) env.fogColor = lerp(env.fogColor, srgb(0xc98f4e) * (0.35f + 0.65f * (1 - env.night)), sandAmt * 0.8f);
    env.fogDensity = (0.0042f + fogAmt * 0.009f + rainAmt * 0.003f + snowAmt * 0.005f + ashAmt * 0.004f + sandAmt * 0.014f) * g.fogMul;
    // 1.13.0 forest haze and morning mist: denser air under the trees and at dawn (the sun shafts cut through it)
    if (!dungeon.active) {
        float fsum = 0;
        for (int i = 0; i < 5; i++) { vec2 o = i == 0 ? vec2(0) : vec2(std::cos(i * 1.57f), std::sin(i * 1.57f)) * 14.f; fsum += world.forestAt(focus.x + o.x, focus.z + o.y); }
        forestHaze = damp(forestHaze, saturate((fsum / 5.f - 0.3f) / 0.5f), 0.5f, dt);
        float dawn = smoothstepf(4.8f, 6.0f, h) * (1.f - smoothstepf(7.6f, 9.6f, h));
        morningMist = damp(morningMist, dawn * (1.f - rainAmt) * (1.f - ashAmt), 0.5f, dt);
        env.fogDensity += (forestHaze * 0.0045f + morningMist * 0.006f) * g.fogMul;
        if (forestHaze > 0.01f) env.fogColor = lerp(env.fogColor, env.fogColor * vec3(0.92f, 1.0f, 0.9f), forestHaze * 0.5f);   // green-tinted under the canopy
    }
    // 3.4.1 a longer view: the fog thins with the draw distance of the quality profile, so what is now drawn
    // further away (villages, forests) can be seen
    env.fogDensity *= lerpf(1.f, 0.6f, saturate((renderer.settings.drawDistance - 110.f) / 90.f));
    env.fogFalloff = 0.035f;
    env.fogBase = focus.y - 2.f;
    env.wetness = damp(env.wetness, saturate(rainAmt * 1.2f), 0.15f, dt);
    env.snowCover = saturate(snowAmt * 0.7f + regionW[R_FROSTHEIM] * 0.15f);
    float wind = 0.35f + rainAmt * 0.3f + stormAmt * 0.6f + snowAmt * 0.3f + regionW[R_HIGHCROWNS] * 0.25f + sandAmt * 0.9f + regionW[R_AMBER] * 0.15f + regionW[R_CORAL] * 0.1f + regionW[R_STEPPE] * 0.35f;
    env.windStrength = damp(env.windStrength, wind, 0.5f, dt);
    env.windDir = normalize(vec2(0.8f, 0.35f + std::sin(realTime * 0.02f) * 0.3f));
    env.gust = 0.6f + 0.4f * std::sin(realTime * 0.13f);
    env.lightning = flash * 0.6f;
    // grading
    env.exposure = g.exposure * (1.f + env.night * 0.35f);
    env.saturation = g.sat * (1.f - overcast * 0.2f) * (1.f - env.night * 0.25f);
    env.contrast = g.contrast;
    env.temperature = g.temp + (h > 16.5f && h < 19.5f ? 0.4f : 0.f) - env.night * 0.3f;
    env.lift = g.lift; env.gamma = g.gamma; env.gain = g.gain;
    env.bloomIntensity = 0.05f + env.night * 0.04f;
    // 1.11.1 light shafts: strongest with a low sun (morning, evening) and in mist or after rain, faint at noon
    {
        float low = 1.f - saturate((env.sunDir.y - 0.15f) / 0.6f);
        env.shafts = (0.3f + 0.55f * low + fogAmt * 0.5f + env.wetness * 0.25f + forestHaze * 0.45f + morningMist * 0.5f) * (1.f - overcast * 0.6f);
    }
    // 2.0.0 sky and post effects
    {
        float clear = (1.f - overcast) * (1.f - rainAmt) * (1.f - fogAmt);
        env.aurora = dungeon.active ? 0.f : saturate((regionW[R_FROSTHEIM] * 1.0f + regionW[R_HIGHCROWNS] * 0.55f + regionW[R_AURORA] * 1.3f) * clear * 1.4f) * (1.f - morningMist);
        env.dust = dungeon.active ? 0.f : saturate(regionW[R_AMBER] * 0.55f + sandAmt + ashAmt * 0.5f + regionW[R_ASHEN] * 0.2f);
        env.rainbow = dungeon.active ? 0.f : saturate((env.wetness - 0.25f) * 2.f) * (1.f - rainAmt) * (1.f - env.night) * (1.f - overcast * 0.6f);
        env.heat = dungeon.active ? 0.f : saturate(regionW[R_AMBER] * 1.2f + regionW[R_ASHEN] * 0.35f) * (1.f - env.night) * (1.f - sandAmt) * (1.f - rainAmt) * saturate(env.sunIntensity / 2.5f);
        env.sandVeil = dungeon.active ? 0.f : sandAmt;
        env.flare = dungeon.active ? 0.f : (1.f - env.night) * saturate(env.sunIntensity / 2.2f) * (1.f - overcast * 0.9f) * (1.f - rainAmt) * (1.f - fogAmt) * 0.75f;
        // 3.4.0 the blazing midday sun: a clear sky from late morning to mid-afternoon, fiercest in the hot lands.
        // The sun hits harder and whiter, the shade gets deeper and cooler (less sky light), the air shimmers
        // over the ground, highlights burn and the sun itself flares in a starburst.
        float noon = smoothstepf(0.6f, 0.88f, env.sunDir.y) * (1.f - env.night);
        float warm = 0.5f + 0.5f * saturate(regionW[R_AMBER] + regionW[R_CANYON] + regionW[R_ASHEN] * 0.6f + regionW[R_STEPPE] * 0.7f + regionW[R_JUNGLE] * 0.6f + regionW[R_CORAL] * 0.5f + regionW[R_GREENHAVEN] * 0.2f)
                     - 0.45f * saturate(regionW[R_FROSTHEIM] + regionW[R_AURORA] + regionW[R_TITANS] * 0.6f);
        float target = dungeon.active ? 0.f : saturate(noon * clear * (1.f - sandAmt) * (1.f - ashAmt * 0.7f) * saturate(warm));
        env.scorch = damp(env.scorch, target, 0.6f, dt);
        if (env.scorch > 0.001f) {
            env.sunIntensity *= 1.f + 0.32f * env.scorch;
            env.sunColor = lerp(env.sunColor, srgb(0xfff0d4), 0.45f * env.scorch);
            env.skyAmbient = lerp(env.skyAmbient, env.skyAmbient * vec3(0.68f, 0.76f, 0.95f), env.scorch);   // cooler, darker shade
            env.groundAmbient *= 1.f - 0.3f * env.scorch;
            env.shadowStrength = std::max(env.shadowStrength, 0.92f * env.scorch);
            env.heat = std::max(env.heat, env.scorch * 0.45f);
            env.flare = std::max(env.flare, 0.85f * env.scorch);
            env.bloomIntensity += 0.05f * env.scorch;
            env.temperature += 0.25f * env.scorch;
        }
    }
    if (underwaterView) { env.flare = 0.f; env.heat = 0.f; env.shafts = 0.f; env.scorch = 0.f; env.bloomIntensity += 0.03f; }   // 3.5.0 the underwater look replaces them
    env.vignette = underwaterView ? 0.55f : 0.32f;
    env.grain = 0.012f;
    env.chromatic = 0;
    // depth of field: tilt-shift diorama in gameplay, real DoF during dialogues & cinematics
    // 3.1.0 no tilt-shift at all in play: it blurred the bottom of the screen, where the near grass is (and much
    // more when zoomed out). The picture stays sharp; real depth of field only in dialogues and cinematics.
    env.tiltShift = state == GameState::Playing ? 0.f : 0.55f;
    if (state == GameState::Playing) env.fogDensity *= lerpf(1.f, 0.4f, camRig.mapK);   // zoomed far out: fog pushed away
    dofAmount = damp(dofAmount, (dialog.open || cinematicT > 0) ? 1.f : 0.f, 4.f, dt);
    env.dofAmount = dofAmount;
    env.dofFocus = dofFocus;
    if (player != NullEntity) {
        Character* pcp = reg.get<Character>(player);
        if (pcp) {
            env.playerPos = pcp->pos;
            env.damagePulse = pcp->hp / std::max(1.f, pcp->maxHp) < 0.25f && !pcp->dead ? (0.35f + 0.25f * std::sin(realTime * 5.f)) : 0.f;
        }
    }
    if (dungeon.active) {
        // underground: no sun, warm torch light, darker fog
        env.sunIntensity = 0.25f; env.shadowStrength = 0.3f; env.sunColor = srgb(0x8a9aff);
        env.skyAmbient = srgb(0x48485c) * 0.95f; env.groundAmbient = srgb(0x3e3024) * 0.8f;   // 3.1.0 brighter: the dressed walls and rooms read
        int th = dungeon.interior ? 9 : dungeons()[dungeon.def].theme;
        env.fogColor = th == 3 ? srgb(0x2a1410) : (th == 4 ? srgb(0x18283a) : (th == 5 ? srgb(0x1a0a24) : (th == 6 ? srgb(0x3a2a14) : (th == 7 ? srgb(0x0e2a34) : (th == 8 ? srgb(0xc8a8e8) : srgb(0x141216))))));
        env.fogDensity = 0.025f; env.fogBase = -10.f; env.cloudShadows = 0; env.snowCover = 0; env.wetness = 0.2f;
        env.exposure = 1.35f; env.tiltShift = 0.f;
        env.night = 1.f;  // window/lantern glows on
        if (dungeon.interior) {   // 2.4.1 indoors: warm lamplight, a little daylight through the windows, no fog
            env.sunIntensity = 0.22f; env.sunColor = srgb(0xffe8c8); env.shadowStrength = 0.35f;
            env.skyAmbient = srgb(0x5a5248) * 0.6f; env.groundAmbient = srgb(0x3a3028) * 0.55f;
            env.fogColor = srgb(0x2a241e); env.fogDensity = 0.004f; env.exposure = 0.92f; env.tiltShift = 0.f;
            if (dungeon.interiorKind == -2) {   // 3.6.0 the cellar: rock and torchlight, a cool shaft of day at the ladder
                env.sunIntensity = 0.08f; env.skyAmbient = srgb(0x3a3640) * 0.55f; env.groundAmbient = srgb(0x2a2018) * 0.5f;
                env.fogColor = srgb(0x121010); env.fogDensity = 0.02f; env.exposure = 1.25f;
            }
        }
        if (th == 8) {
            // 2.2.1 the celestial realm is open sky: low golden sun, violet zenith, aurora, light mist far below
            env.sunDir = normalize(vec3(0.5f, 0.32f, 0.6f)); env.sunColor = srgb(0xffd8a8); env.sunIntensity = 2.4f; env.shadowStrength = 0.8f;
            env.skyAmbient = srgb(0x7a70b8) * 0.75f; env.groundAmbient = srgb(0x3a2e5a) * 0.7f;
            env.skyTop = srgb(0x3a2a8a); env.skyHorizon = srgb(0xe8a8c8); env.cloudCover = 0.45f;
            env.fogColor = srgb(0x8a74b8); env.fogDensity = 0.0022f; env.fogBase = -30.f;
            env.night = 0.35f; env.exposure = 0.82f; env.aurora = 0.9f; env.saturation = 1.12f; env.contrast = 1.12f; env.dust = 0.f; env.heat = 0.f; env.sandVeil = 0.f;
            env.flare = 0.6f; env.shafts = 0.6f; env.tiltShift = 0.f; env.wetness = 0.f;
        }
        if (th == 10) {
            // 3.2.0 space: a hard white sun, black shadows softened by the planet's blue glow from below, no air at all
            env.space = 1.f;
            env.sunDir = normalize(vec3(0.62f, 0.45f, 0.4f)); env.sunColor = srgb(0xfff4e8); env.sunIntensity = 3.4f; env.shadowStrength = 0.95f;
            env.skyAmbient = srgb(0x2a2a4a) * 0.8f; env.groundAmbient = srgb(0x3a5a9a) * 0.55f;   // the planet lights the asteroids from below
            env.fogColor = srgb(0x05040c); env.fogDensity = 0.0004f; env.fogBase = -200.f; env.cloudCover = 0; env.cloudShadows = 0;
            env.night = 0.f; env.exposure = 0.95f; env.aurora = 0.f; env.saturation = 1.15f; env.contrast = 1.14f; env.dust = 0.f; env.heat = 0.f; env.sandVeil = 0.f;
            env.flare = 0.9f; env.shafts = 0.f; env.tiltShift = 0.f; env.wetness = 0.f; env.bloomIntensity = 0.1f;
        }
    }
    audio.setReverb(dungeon.active ? (dungeon.interior ? 0.12f : 0.45f) : (underwaterView ? 0.6f : 0.f));
    applyCinematicEnvironment();
}

void Game::updateAmbientFx(float dt) {
    vec3 c = camFocus;
    Rng& r = particles.rng;
    float budget = renderer.settings.particleScale;
    // rain streaks around the camera focus
    int nRain = (int)(rainAmt * 140 * dt * 10 * budget);
    for (int i = 0; i < nRain; i++) {
        Particle p;
        vec2 d = r.inDisk() * 26.f;
        p.pos = c + vec3(d.x, r.range(10.f, 16.f), d.y);
        p.vel = vec3(env.windDir.x * 3.f * env.windStrength, -22.f, env.windDir.y * 3.f * env.windStrength);
        p.maxLife = 0.8f; p.size0 = p.size1 = 0.065f; p.stretch = 3.8f;
        p.c0 = vec4(0.78f, 0.84f, 0.95f, 0.6f); p.c1 = p.c0; p.frame = PF_RAIN; p.additive = false;
        p.groundKill = groundAt(p.pos.x, p.pos.z);
        particles.spawn(p);
    }
    // 1.9.2 rain curtains: long faint streaks further away (the storm visibly sweeps the landscape)
    int nFar = (int)(rainAmt * rainAmt * 50 * dt * 10 * budget);
    for (int i = 0; i < nFar; i++) {
        Particle p;
        float ang = r.range(0, TAU), dist = r.range(26.f, 60.f);
        p.pos = c + vec3(std::cos(ang) * dist, r.range(14.f, 26.f), std::sin(ang) * dist);
        p.vel = vec3(env.windDir.x * 5.f * env.windStrength, -26.f, env.windDir.y * 5.f * env.windStrength);
        p.maxLife = 1.1f; p.size0 = p.size1 = 0.16f; p.stretch = 5.f;
        p.c0 = vec4(0.7f, 0.76f, 0.86f, 0.22f); p.c1 = p.c0; p.frame = PF_RAIN; p.additive = false;
        p.groundKill = groundAt(p.pos.x, p.pos.z);
        particles.spawn(p);
    }
    // splashes where the drops hit: little crowns on the ground, rings on water
    int nSplash = (int)(rainAmt * 90 * dt * 10 * budget);
    for (int i = 0; i < nSplash; i++) {
        vec2 d = r.inDisk() * 14.f;
        vec3 g{c.x + d.x, 0, c.z + d.y};
        g.y = groundAt(g.x, g.z);
        float wl = waterAt(g.x, g.z);
        bool onWater = wl > -1e8f && wl > g.y;
        if (onWater) {
            Particle w; w.pos = vec3(g.x, wl + 0.02f, g.z); w.maxLife = 0.35f; w.size0 = 0.03f; w.size1 = 0.22f;
            w.c0 = vec4(0.8f, 0.86f, 0.95f, 0.35f); w.c1 = vec4(0.8f, 0.86f, 0.95f, 0.f); w.frame = PF_RING; w.additive = false;
            particles.spawn(w);
            continue;
        }
        for (int k = 0; k < 3; k++) {
            Particle s;
            float a = r.range(0, TAU);
            s.pos = g + vec3(0, 0.03f, 0);
            s.vel = vec3(std::cos(a) * r.range(0.4f, 0.9f), r.range(0.9f, 1.6f), std::sin(a) * r.range(0.4f, 0.9f));
            s.gravity = 9.81f; s.maxLife = 0.22f; s.size0 = s.size1 = 0.018f; s.stretch = 0.015f;
            s.c0 = vec4(0.85f, 0.9f, 1.f, 0.55f); s.c1 = vec4(0.85f, 0.9f, 1.f, 0.f); s.frame = PF_RAIN; s.additive = false;
            particles.spawn(s);
        }
    }
    int nSnow = (int)(snowAmt * 60 * dt * 10 * budget);
    for (int i = 0; i < nSnow; i++) {
        Particle p;
        vec2 d = r.inDisk() * 26.f;
        p.pos = c + vec3(d.x, r.range(6.f, 14.f), d.y);
        p.vel = vec3(env.windDir.x * 2.f * env.windStrength, -1.6f, env.windDir.y * 2.f * env.windStrength);
        p.maxLife = 7.f; p.size0 = p.size1 = r.range(0.07f, 0.14f); p.turbulence = 1.2f;
        p.c0 = vec4(1, 1, 1, 0.9f); p.c1 = vec4(1, 1, 1, 0.6f); p.frame = PF_SNOW; p.additive = false; p.rotVel = r.range(-2.f, 2.f);
        p.groundKill = groundAt(p.pos.x, p.pos.z);
        particles.spawn(p);
    }
    // 2.0.0 sandstorm: grit racing across the ground with the wind, in thick sheets
    int nSand = (int)(sandAmt * 120 * dt * 10 * budget);
    for (int i = 0; i < nSand; i++) {
        Particle p;
        vec2 d = r.inDisk() * 28.f;
        p.pos = c + vec3(d.x, r.range(0.3f, 7.f), d.y);
        vec2 w = env.windDir * (14.f + 8.f * env.windStrength);
        p.vel = vec3(w.x, r.range(-0.3f, 0.5f), w.y);
        p.maxLife = 1.6f; p.size0 = r.range(0.05f, 0.12f); p.size1 = 0.f; p.turbulence = 2.2f; p.stretch = 2.6f;
        p.c0 = vec4(0.85f, 0.62f, 0.34f, 0.55f); p.c1 = vec4(0.85f, 0.62f, 0.34f, 0.f); p.frame = PF_DUST; p.additive = false;
        particles.spawn(p);
    }
    // Amber Sands, calm weather: a faint blown-sand veil off the dune crests, wind-driven
    if (sandAmt < 0.1f && regionW[R_AMBER] > 0.4f && r.f() < (dt * budget) * 10.f * regionW[R_AMBER] * (1.f - env.night)) {
        Particle p;
        vec2 d = r.inDisk() * 22.f;
        p.pos = vec3(c.x + d.x, groundAt(c.x + d.x, c.z + d.y) + r.range(0.1f, 1.2f), c.z + d.y);
        vec2 w = env.windDir * (4.f + 5.f * env.windStrength);
        p.vel = vec3(w.x, r.range(0.1f, 0.6f), w.y); p.turbulence = 0.9f;
        p.maxLife = 2.4f; p.size0 = 0.07f; p.size1 = 0.02f; p.stretch = 1.8f;
        p.c0 = vec4(0.92f, 0.74f, 0.46f, 0.4f); p.c1 = vec4(0.92f, 0.74f, 0.46f, 0.f); p.frame = PF_DUST; p.additive = false;
        particles.spawn(p);
    }
    // 2.2.1 the celestial portal: sparks spiralling into the vortex, motes rising from the dais
    if (!dungeon.active && player != NullEntity) {
        static const Poi* portals[2] = {nullptr, nullptr}; static bool looked = false;
        if (!looked) { looked = true; portals[0] = world.poiById("portal_ether"); portals[1] = world.poiById("portal_stars"); }   // 3.2.0 + the Star Observatory
        for (const Poi* portal : portals)
        if (portal && distXZ(pc().pos, vec3(portal->pos.x, 0, portal->pos.y)) < 90.f) {
            float gy = world.height(portal->pos.x, portal->pos.y) + 0.86f + 4.4f;
            vec3 ctr(portal->pos.x, gy, portal->pos.y);
            int n = (int)(dt * 140.f * budget);
            for (int i = 0; i < n; i++) {
                Particle p;
                float a = r.range(0, TAU), rr = r.range(2.6f, 3.6f);
                vec3 off(std::cos(a) * rr, std::sin(a) * rr, r.range(-0.2f, 0.2f));
                p.pos = ctr + off;
                vec3 tang(-std::sin(a), std::cos(a), 0);
                p.vel = tang * r.range(3.f, 5.f) - off * 0.9f;
                p.drag = 0.4f; p.maxLife = r.range(0.7f, 1.2f); p.size0 = r.range(0.05f, 0.1f); p.size1 = 0.f; p.stretch = 0.5f;
                vec3 col = r.chance(0.5f) ? vec3(0.55f, 0.9f, 1.f) : vec3(0.85f, 0.65f, 1.f);
                p.c0 = vec4(col * 3.f, 1); p.c1 = vec4(col, 0); p.frame = PF_DOT; p.additive = true;
                particles.spawn(p);
            }
            if (r.f() < dt * 25.f * budget) {
                Particle p; p.pos = vec3(portal->pos.x + r.range(-4.5f, 4.5f), gy - 4.3f, portal->pos.y + r.range(-4.5f, 4.5f));
                p.vel = vec3(0, r.range(0.6f, 1.6f), 0); p.turbulence = 0.6f; p.maxLife = 3.f; p.size0 = 0.06f; p.size1 = 0.f;
                p.c0 = vec4(1.6f, 1.9f, 3.f, 1); p.c1 = vec4(0.5f, 0.6f, 1.f, 0); p.frame = PF_DOT; p.additive = true;
                particles.spawn(p);
            }
        }
    }
    int nAsh = (int)(ashAmt * 30 * dt * 10 * budget);
    for (int i = 0; i < nAsh; i++) {
        Particle p;
        vec2 d = r.inDisk() * 24.f;
        p.pos = c + vec3(d.x, r.range(0.5f, 10.f), d.y);
        p.vel = vec3(r.range(-0.5f, 0.5f), r.range(-0.3f, 0.6f), r.range(-0.5f, 0.5f));
        p.maxLife = 5.f; p.size0 = r.range(0.04f, 0.09f); p.size1 = 0.f; p.turbulence = 0.8f;
        bool ember = r.chance(0.3f);
        p.c0 = ember ? vec4(3.f, 1.2f, 0.3f, 1.f) : vec4(0.3f, 0.28f, 0.27f, 0.8f); p.c1 = ember ? vec4(1, 0.3f, 0.05f, 0) : vec4(0.3f, 0.28f, 0.27f, 0);
        p.frame = ember ? PF_EMBER : PF_DUST; p.additive = ember;
        particles.spawn(p);
    }
    // region life: leaves, fireflies at night, pollen, butterflies, magic motes
    float spawnRate = dt * budget;
    if (r.f() < spawnRate * 6.f * (regionW[R_GREENHAVEN] + regionW[R_ELDERWOOD]) * (1 - env.night)) {
        Particle p;
        vec2 d = r.inDisk() * 20.f;
        p.pos = c + vec3(d.x, r.range(4.f, 9.f), d.y);
        p.vel = vec3(env.windDir.x * 1.2f, -0.8f, env.windDir.y * 1.2f);
        p.maxLife = 7; p.size0 = p.size1 = 0.16f; p.turbulence = 2.f; p.rot = r.range(0, TAU); p.rotVel = r.range(-3.f, 3.f);
        vec3 lc = regionW[R_ELDERWOOD] > 0.5f ? srgb(0x5ab89a) : (r.chance(0.5f) ? srgb(0xc8a040) : srgb(0x7a9a3a));
        p.c0 = vec4(lc * 2.f, 1); p.c1 = vec4(lc * 2.f, 0); p.frame = PF_LEAF; p.additive = false;
        p.groundKill = groundAt(p.pos.x, p.pos.z);
        particles.spawn(p);
    }
    // 1.13.0 golden dust and pollen floating in the light under the trees (daytime)
    if (r.f() < spawnRate * 22.f * forestHaze * (1 - env.night) * (1 - rainAmt)) {
        Particle p;
        vec2 d = r.inDisk() * 14.f;
        float gy = groundAt(c.x + d.x, c.z + d.y);
        p.pos = vec3(c.x + d.x, gy + r.range(0.6f, 4.5f), c.z + d.y);
        p.vel = vec3(env.windDir.x * 0.15f, r.range(-0.05f, 0.12f), env.windDir.y * 0.15f); p.turbulence = 0.35f;
        p.maxLife = r.range(5.f, 9.f); p.size0 = p.size1 = r.range(0.025f, 0.05f);
        vec3 mc = vec3(1.f, 0.88f, 0.6f) * (0.6f + 0.6f * env.sunIntensity / 3.f);
        p.c0 = vec4(mc * 2.2f, 0.9f); p.c1 = vec4(mc, 0); p.frame = PF_DOT; p.additive = true;
        particles.spawn(p);
    }
    float fireflies = env.night * (regionW[R_GREENHAVEN] * 0.6f + regionW[R_ELDERWOOD] + regionW[R_SHADOWS] * 0.7f);
    if (r.f() < spawnRate * 14.f * fireflies) {
        Particle p;
        vec2 d = r.inDisk() * 18.f;
        float gy = groundAt(c.x + d.x, c.z + d.y);
        p.pos = vec3(c.x + d.x, gy + r.range(0.4f, 2.5f), c.z + d.y);
        p.vel = vec3(0); p.turbulence = 1.5f; p.maxLife = r.range(3.f, 6.f); p.size0 = 0.09f; p.size1 = 0.05f;
        vec3 fc = regionW[R_SHADOWS] > 0.5f ? vec3(0.6f, 1.f, 0.4f) : (regionW[R_ELDERWOOD] > 0.5f ? vec3(0.4f, 1.f, 0.9f) : vec3(1.f, 0.9f, 0.4f));
        p.c0 = vec4(fc * 4.f, 1); p.c1 = vec4(fc * 2.f, 0); p.frame = PF_DOT;
        particles.spawn(p);
    }
    if (r.f() < spawnRate * 3.f * (1 - env.night) * (regionW[R_GREENHAVEN] + regionW[R_ELDERWOOD] * 0.5f) * (1 - rainAmt)) {
        // butterflies (two flapping dots read as wings at this scale)
        Particle p;
        vec2 d = r.inDisk() * 15.f;
        float gy = groundAt(c.x + d.x, c.z + d.y);
        p.pos = vec3(c.x + d.x, gy + r.range(0.5f, 1.8f), c.z + d.y);
        p.vel = vec3(r.range(-1, 1), 0.2f, r.range(-1, 1)); p.turbulence = 4.f; p.maxLife = 6; p.size0 = p.size1 = 0.14f;
        static const uint32_t bc[] = {0xffe060, 0xffffff, 0x60b0ff, 0xff8a40};
        vec3 col = srgb(bc[r.irange(0, 3)]);
        p.c0 = vec4(col * 1.6f, 1); p.c1 = vec4(col, 0); p.frame = PF_LEAF; p.additive = false; p.rotVel = 20.f;
        particles.spawn(p);
    }
    // birds: a flock crossing the sky now and then (daytime, outdoors)
    if (!dungeon.active && r.f() < spawnRate * 0.12f * (1 - env.night) * (1 - stormAmt)) {
        vec3 dir = normalize(vec3(r.range(-1, 1), 0, r.range(-1, 1)));
        vec3 start = c - dir * 40.f + vec3(0, r.range(12.f, 18.f), 0);
        int n = r.irange(5, 11);
        for (int i = 0; i < n; i++) {
            Particle b;
            b.pos = start + vec3(r.range(-4, 4), r.range(-1, 1), r.range(-4, 4));
            b.vel = dir * r.range(7.f, 8.f);
            b.maxLife = 12; b.size0 = b.size1 = 0.55f; b.frame = PF_BIRD; b.additive = false;
            b.c0 = vec4(0.12f, 0.1f, 0.1f, 0.9f); b.c1 = b.c0; b.turbulence = 0.6f;
            particles.spawn(b);
        }
        if (r.chance(0.5f)) play("birds", 0.25f, r.range(0.9f, 1.1f));
    }
    // chunk emitters (chimney smoke, fires, sparkles) near the camera
    world.forEachVisibleChunk([&](Chunk& ch) {
        if (distXZ(ch.bounds.center(), c) > 55.f) return;
        for (const vec4& e : ch.emitters) {
            vec3 p{e.x, e.y, e.z};
            int kind = (int)e.w;
            if (distXZ(p, c) > 45.f) continue;
            if (kind == EM_SMOKE) particles.smoke(p, 1.f, dt * (0.6f + env.night * 0.6f));
            else if (kind == EM_FIRE) particles.fire(p, 1.f, dt);
            else if (kind == EM_SPARKLES && r.f() < dt * 8) particles.magicTrail(p + vec3(r.range(-1, 1), r.range(0, 2.5f), r.range(-1, 1)), vec3(0.4f, 0.9f, 1.f), 0.6f);
        }
    });
    if (dungeon.active) for (const vec4& e : dungeon.emitters) {
        vec3 p{e.x, e.y, e.z};
        if (distXZ(p, c) > 40.f) continue;
        if ((int)e.w == EM_FIRE) particles.fire(p, 0.7f, dt);
        else if ((int)e.w == EM_SPARKLES && r.f() < dt * 6) particles.magicTrail(p + vec3(r.range(-1, 1), r.range(0, 2.f), r.range(-1, 1)), elementColor(Element::Arcane), 0.5f);
    }
}

// ============================================================================================ audio director
void Game::updateAudioDirector(float dt) {
    if (player == NullEntity) return;
    if (cine.active) return;   // the cinematic chooses its own music
    Character& p = pc();
    vec3 right = normalize(vec3(std::cos(camYaw), 0, -std::sin(camYaw)));
    audio.setListener(p.pos, right);
    // music: boss > combat > dungeon > region (day/night)
    bool combat = false;
    reg.each<AIBrain, Character>([&](Entity, AIBrain& ai, Character& c) {
        if (!c.dead && (ai.state == AIState::Chase || ai.state == AIState::Attack || ai.state == AIState::Strafe) && dist2XZ(c.pos, p.pos) < 25 * 25) combat = true;
    });
    std::string track;
    if (bossEnt != NullEntity) track = "boss";
    else if (combat) track = "combat";
    else if (dungeon.active) track = dungeon.interior ? "village" : (dungeons()[dungeon.def].theme == 8 || dungeons()[dungeon.def].theme == 10 ? "realm" : "dungeon");   // 2.3.4 the Celestial Realm; 2.4.1 indoors
    else {
        // 2.3.4: the Sands and the Coast have their own scores (they borrowed the Ashen / Greenhaven ones)
        static const char* names[] = {"greenhaven", "frostheim", "ashen", "elderwood", "shadows", "highcrowns", "sands", "coast", "steppe", "jungle", "realm", "highcrowns", "sands"};   // 3.0.0 the Titans: the mountain score   // 2.8.0 Aurora: the ethereal score
        int r = world.regionId(p.pos.x, p.pos.z);
        int vi = world.nearestPoi(p.pos.xz(), 90.f);
        bool village = vi >= 0 && (world.pois[vi].type == PoiType::Village || world.pois[vi].type == PoiType::Monastery || world.pois[vi].type == PoiType::Mill);
        track = env.night > 0.6f ? "night" : (village ? "village" : names[r]);
    }
    // 2.3.4 no ping-pong between tracks: combat music holds 6 s after the last enemy gives up, and a new
    // exploration track (region / village / night border) must stay the right one for 3 s before it starts
    static float combatHold = 0, candT = 0; static std::string cand;
    if (track == "combat") combatHold = 6.f; else combatHold = std::max(0.f, combatHold - dt);
    if (track != "boss" && track != "combat" && combatHold > 0 && !dungeon.active) track = "combat";
    bool urgent = track == "boss" || track == "combat" || audio.currentMusic().empty() || audio.currentMusic() == "title";
    if (track != audio.currentMusic() && !urgent) {
        if (track != cand) { cand = track; candT = 0; }
        candT += dt;
        if (candT < 3.f) track = audio.currentMusic();
    } else { cand.clear(); candT = 0; }
    audio.playMusic(track, track == "combat" || track == "boss" ? 1.5f : 4.f);
    audio.setMusicDuck(dialog.open ? 0.55f : (underwaterView ? 0.4f : 1.f));   // 2.4.0 muffled under water
    // ambience layers
    float forest = regionW[R_GREENHAVEN] * 0.7f + regionW[R_ELDERWOOD] + regionW[R_CORAL] * 0.8f + regionW[R_JUNGLE] * 1.f + regionW[R_STEPPE] * 0.35f;
    bool under = dungeon.active;
    audio.setAmbience("amb_forest_day", under ? 0 : forest * (1 - env.night) * (1 - rainAmt) * 0.8f);
    audio.setAmbience("amb_night", under ? 0 : env.night * (1 - rainAmt) * 0.7f);
    audio.setAmbience("amb_wind", under ? 0 : clampf(0.15f + env.windStrength * 0.6f + regionW[R_FROSTHEIM] * 0.3f + regionW[R_HIGHCROWNS] * 0.4f + regionW[R_AMBER] * 0.35f + regionW[R_STEPPE] * 0.5f + regionW[R_CANYON] * 0.45f + sandAmt * 0.5f, 0, 1) * 0.8f);   // 3.3.0 + canyon wind
    audio.setAmbience("amb_rain", under ? 0 : rainAmt);
    audio.setAmbience("amb_lava", under ? (!dungeon.interior && dungeons()[dungeon.def].theme == 3 ? 0.6f : 0.f) : regionW[R_ASHEN] * 0.6f);
    audio.setAmbience("amb_swamp", under ? 0 : regionW[R_SHADOWS] * 0.6f + regionW[R_JUNGLE] * 0.35f);   // 2.4.0 the jungle croaks too
    audio.setAmbience("amb_cave", under && (!dungeon.interior || dungeon.interiorKind == -2) ? 0.8f : 0.f);   // 3.6.0 + your cellar
    float river = 0;
    if (!under) { vec2 fl; bool lv, fr; float wl = world.waterLevel(p.pos.x, p.pos.z, &lv, &fr, &fl); river = (wl > -1e8f && length(fl) > 0.01f) ? 0.8f : 0.f; }
    audio.setAmbience("amb_river", river);
    int vi = world.nearestPoi(p.pos.xz(), 70.f);
    bool villageNear = !under && vi >= 0 && (world.pois[vi].type == PoiType::Village || world.pois[vi].type == PoiType::Mill || world.pois[vi].type == PoiType::Monastery);
    audio.setAmbience("amb_village", villageNear ? (1 - env.night) * 0.6f : 0.f);
    audio.setAmbience("amb_sea", under ? 0.f : regionW[R_CORAL] * 0.75f);   // 2.3.4 the surf of the Coral Coast
}

}  // namespace lk
