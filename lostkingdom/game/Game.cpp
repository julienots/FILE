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
    initClips();
    world.grassDensity = gs.grassDensity;
    world.detailedFoliage = settings.profile <= QualityProfile::Medium;
    if (!gpuReady) {
        world.buildPlan();
        world.startWorkers(std::max(1, std::min(3, platform::cpuCount() - 1)));
        std::thread([this] { world.generateMapImage(); }).detach();
        loadAssets();
    }
    buildSharedMeshes();
    gpuReady = true;
    fog.assign(128 * 128, 0);
    state = autoplay ? GameState::Title : GameState::Splash;
    if (autoplay && (autoplayScenario == "splash" || autoplayScenario == "full")) state = GameState::Splash;
    stateT = 0;
    audio.setVolumes(settings.master, settings.music, settings.sfx, settings.ambience);
    AGR_LOG("Lost Kingdom initialised in %.0f ms (%dx%d, profile %s)", (platform::timeSeconds() - t0) * 1000, W, H, profileName(settings.profile));
    return true;
}

void Game::resize(int w, int h) { W = w; H = h; renderer.resize(w, h); }

void Game::pause() {
    audio.setPaused(true);
    if (state == GameState::Playing && player != NullEntity) { saveGame(99); saveSettings(); }  // autosave slot on focus loss
}

void Game::resume() { audio.setPaused(false); }

void Game::contextLost() {
    // All GL objects are invalid: forget them without calling glDelete, rebuild on next init.
    renderer = Renderer();
    ui = UI();
    particles = ParticleSystem();
    meshCache.clear();
    for (auto& kv : importedModels) if (kv.second) { kv.second->mesh.reset(); kv.second->tex = Texture(); }
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
    if (state == GameState::Playing) saveGame(99);
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

void Game::play(const std::string& s, float vol, float pitch) {
    auto it = sfx.find(s);
    if (it != sfx.end()) audio.play(it->second, vol, pitch);
}
void Game::play3D(const std::string& s, vec3 p, float vol, float pitch, float range) {
    auto it = sfx.find(s);
    if (it != sfx.end()) audio.play3D(it->second, p, vol, pitch, range);
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
    if (avg > target * 1.18f && k > 0.55f) {
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
        bool paused = menu != Menu::None && menu != Menu::Death;
        float sdt = paused ? 0.f : dt * timeScale;
        if (hitStop > 0) { hitStop -= dt; sdt *= 0.08f; }
        if (!paused) updatePlaying(sdt);
        PROF(9, renderWorld(sdt));
        PROF(10, { drawHud(dt); if (dialog.open) drawDialog(); drawMenus(dt); });
        break;
    }
    }
    drawNotifications(dt);
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
    if (!pendingScreenshot.empty()) { AGR_LOG("render stats: %d draw calls, %dk triangles (%s)", renderer.stats.drawCalls, renderer.stats.triangles / 1000, pendingScreenshot.c_str()); platform::requestScreenshot(pendingScreenshot); pendingScreenshot.clear(); }
}

void Game::updatePlaying(float dt) {
    gameTime += dt;
    playSeconds += dt;
    // time of day: 1 in-game hour = 60 s
    timeOfDay += dt / 60.f;
    if (timeOfDay >= 24.f) { timeOfDay -= 24.f; day++; }
    gProfFrames++;
    PROF(0, updatePlayer(dt));
    PROF(1, updateCharacters(dt));
    PROF(2, updateProjectiles(dt));
    PROF(3, updateSpawning(dt));
    PROF(4, updateDungeon(dt));
    PROF(5, updateEnvironment(dt));
    PROF(6, updateAmbientFx(dt));
    PROF(7, particles.update(dt));
    PROF(8, updateAudioDirector(dt));
    reg.flush();
    // autosave every 3 minutes (outside combat)
    autosaveT += dt;
    if (autosaveT > 180.f && bossEnt == NullEntity) { autosaveT = 0; saveGame(99); notify("Sauvegarde automatique", vec3(0.7f, 0.85f, 1.f), "i_save"); }
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
    if (dungeon.active && focus.x > WORLD_SIZE) { for (int i = 0; i < R_COUNT; i++) rw.w[i] = i == dungeons()[dungeon.def].region ? 1.f : 0.f; }
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
        default: next = p < 0.55f ? W_CLEAR : p < 0.75f ? W_CLOUDY : W_SNOW; break;
        }
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
    float tCloud = target(wcur, W_CLOUDY) * wb + tRain + tSnow * 0.7f + tStorm;
    if (dungeon.active) tRain = tSnow = tStorm = tAsh = tCloud = 0;
    rainAmt = damp(rainAmt, tRain, 0.5f, dt);
    snowAmt = damp(snowAmt, tSnow, 0.5f, dt);
    fogAmt = damp(fogAmt, saturate(tFog), 0.4f, dt);
    stormAmt = damp(stormAmt, tStorm, 0.5f, dt);
    ashAmt = damp(ashAmt, saturate(tAsh), 0.5f, dt);
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
    float overcast = cloud * 0.55f + fogAmt * 0.25f + ashAmt * 0.3f;
    env.sunIntensity *= (1.f - overcast * 0.75f);
    env.shadowStrength = 1.f - overcast * 0.7f;
    env.skyAmbient = lerp(env.skyAmbient, env.skyAmbient * 0.75f + vec3(0.05f), overcast);
    env.cloudShadows = 0.35f + cloud * 0.4f;
    env.cloudCover = 0.3f + cloud * 0.6f;
    env.fogColor = lerp(fogC, fogC * 0.7f + vec3(0.12f), overcast) * g.fogTint;
    if (ashAmt > 0.01f) env.fogColor = lerp(env.fogColor, srgb(0x5a4a44) * (0.3f + 0.7f * (1 - env.night)), ashAmt * 0.4f);
    if (fogAmt > 0.01f) env.fogColor = lerp(env.fogColor, env.fogColor * 0.75f, fogAmt);
    env.fogDensity = (0.0042f + fogAmt * 0.009f + rainAmt * 0.003f + snowAmt * 0.005f + ashAmt * 0.004f) * g.fogMul;
    env.fogFalloff = 0.035f;
    env.fogBase = focus.y - 2.f;
    env.wetness = damp(env.wetness, saturate(rainAmt * 1.2f), 0.15f, dt);
    env.snowCover = saturate(snowAmt * 0.7f + regionW[R_FROSTHEIM] * 0.15f);
    float wind = 0.35f + rainAmt * 0.3f + stormAmt * 0.6f + snowAmt * 0.3f + regionW[R_HIGHCROWNS] * 0.25f;
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
    env.vignette = 0.32f;
    env.grain = 0.012f;
    env.chromatic = 0;
    // depth of field: tilt-shift diorama in gameplay, real DoF during dialogues & cinematics
    env.tiltShift = state == GameState::Playing ? 0.35f : 0.55f;
    if (state == GameState::Playing) {
        // free camera: zoomed far out = "map" view (fog pushed away, miniature tilt-shift);
        // close-ups and low "sky" shots keep the hero sharp
        env.fogDensity *= lerpf(1.f, 0.4f, camRig.mapK);
        env.tiltShift = lerpf(env.tiltShift, 0.8f, camRig.mapK) * (1.f - 0.7f * camRig.nearK) * (1.f - 0.75f * camRig.lowK);
    }
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
        env.skyAmbient = srgb(0x2a2a3a) * 0.9f; env.groundAmbient = srgb(0x2a2018) * 0.7f;
        int th = dungeons()[dungeon.def].theme;
        env.fogColor = th == 3 ? srgb(0x2a1410) : (th == 4 ? srgb(0x18283a) : (th == 5 ? srgb(0x1a0a24) : srgb(0x141216)));
        env.fogDensity = 0.025f; env.fogBase = -10.f; env.cloudShadows = 0; env.snowCover = 0; env.wetness = 0.2f;
        env.exposure = 1.25f; env.tiltShift = 0.25f;
        env.night = 1.f;  // window/lantern glows on
    }
    audio.setReverb(dungeon.active ? 0.45f : 0.f);
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
        p.maxLife = 0.8f; p.size0 = p.size1 = 0.05f; p.stretch = 3.5f;
        p.c0 = vec4(0.75f, 0.82f, 0.95f, 0.45f); p.c1 = p.c0; p.frame = PF_RAIN; p.additive = false;
        p.groundKill = groundAt(p.pos.x, p.pos.z);
        particles.spawn(p);
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
    else if (dungeon.active) track = "dungeon";
    else {
        static const char* names[] = {"greenhaven", "frostheim", "ashen", "elderwood", "shadows", "highcrowns"};
        int r = world.regionId(p.pos.x, p.pos.z);
        int vi = world.nearestPoi(p.pos.xz(), 90.f);
        bool village = vi >= 0 && (world.pois[vi].type == PoiType::Village || world.pois[vi].type == PoiType::Monastery || world.pois[vi].type == PoiType::Mill);
        track = env.night > 0.6f ? "night" : (village ? "village" : names[r]);
    }
    audio.playMusic(track, track == "combat" || track == "boss" ? 1.5f : 4.f);
    audio.setMusicDuck(dialog.open ? 0.55f : 1.f);
    // ambience layers
    float forest = regionW[R_GREENHAVEN] * 0.7f + regionW[R_ELDERWOOD];
    bool under = dungeon.active;
    audio.setAmbience("amb_forest_day", under ? 0 : forest * (1 - env.night) * (1 - rainAmt) * 0.8f);
    audio.setAmbience("amb_night", under ? 0 : env.night * (1 - rainAmt) * 0.7f);
    audio.setAmbience("amb_wind", under ? 0 : clampf(0.15f + env.windStrength * 0.6f + regionW[R_FROSTHEIM] * 0.3f + regionW[R_HIGHCROWNS] * 0.4f, 0, 1) * 0.8f);
    audio.setAmbience("amb_rain", under ? 0 : rainAmt);
    audio.setAmbience("amb_lava", under ? (dungeons()[dungeon.def].theme == 3 ? 0.6f : 0.f) : regionW[R_ASHEN] * 0.6f);
    audio.setAmbience("amb_swamp", under ? 0 : regionW[R_SHADOWS] * 0.6f);
    audio.setAmbience("amb_cave", under ? 0.8f : 0.f);
    float river = 0;
    if (!under) { vec2 fl; bool lv, fr; float wl = world.waterLevel(p.pos.x, p.pos.z, &lv, &fr, &fl); river = (wl > -1e8f && length(fl) > 0.01f) ? 0.8f : 0.f; }
    audio.setAmbience("amb_river", river);
    int vi = world.nearestPoi(p.pos.xz(), 70.f);
    bool villageNear = !under && vi >= 0 && (world.pois[vi].type == PoiType::Village || world.pois[vi].type == PoiType::Mill || world.pois[vi].type == PoiType::Monastery);
    audio.setAmbience("amb_village", villageNear ? (1 - env.night) * 0.6f : 0.f);
}

}  // namespace lk
