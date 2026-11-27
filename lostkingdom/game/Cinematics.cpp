// LOST KINGDOM — multi-shot cinematics (1.7.0): camera moves through the real world, narration with a
// typewriter, speakers, per-shot time of day / weather / fog tint, letterbox, tap to advance, skip button.
// The prologue ("La Chute des Couronnes") plays when a new game starts and can be replayed from the Journal.
#include "Game.h"
#include "../engine/core/Platform.h"

namespace lk {

// Nearest point of open sea from `from`, and the shore just before it (the prologue's storm shot looks at the sea).
static bool nearestSea(const World& w, vec2 from, vec2& sea, vec2& shore) {
    for (float r = 120.f; r < 2200.f; r += 60.f)
        for (int k = 0; k < 32; k++) {
            float a = k * TAU / 32;
            vec2 d{std::cos(a), std::sin(a)}, p = from + d * r;
            if (p.x < 10 || p.y < 10 || p.x > WORLD_SIZE - 10 || p.y > WORLD_SIZE - 10) continue;
            bool lava = false, frozen = false;
            float wl = w.waterLevel(p.x, p.y, &lava, &frozen);
            if (wl < -1e8f || lava || frozen || wl > SEA_LEVEL + 0.5f || w.height(p.x, p.y) > wl - 2.f) continue;
            sea = p;
            shore = p;
            for (float b = 0; b < r; b += 6.f) { vec2 q = p - d * b; if (w.height(q.x, q.y) > wl + 1.f) { shore = q; break; } }
            return true;
        }
    return false;
}

// --------------------------------------------------------------------------------------------- prologue
// Positions are world coordinates (the map is 3072 m wide); heights are relative to the terrain at the point.
static std::vector<CineShot> prologueShots(Game& g) {
    std::vector<CineShot> s;
    const World& w = g.world;
    auto at = [&](float x, float z, float up) { return vec3(x, w.height(x, z) + up, z); };
    const Poi* brind = w.poiById("brindlemere");
    vec2 beach = brind ? brind->pos + vec2(20.f, 60.f) : vec2(1500, 1680);
    const Poi* hs = w.poiById("highspire");
    const Poi* mv = w.poiById("morneval");
    const Poi* ab = w.poiById("abyss");
    const Poi* rg = w.poiById("root_glade");
    vec2 H = hs ? hs->pos : vec2(2350, 820), M = mv ? mv->pos : vec2(1520, 2690), A = ab ? ab->pos : vec2(1080, 2840), R = rg ? rg->pos : vec2(640, 1860);
    vec2 B = brind ? brind->pos : vec2(1480, 1620);

    CineShot c;
    // 1. the kingdom from the sky at dawn
    c = CineShot(); c.dur = 7.f; c.tod = 6.4f; c.weather = Game::W_CLEAR; c.music = "title";
    c.fogMul = 0.15f;
    c.from = at(B.x + 300, B.y + 330, 120); c.to = at(B.x + 210, B.y + 240, 100); c.lookFrom = at(B.x, B.y, 0); c.lookTo = at(B.x - 40, B.y - 60, 0);
    c.text = "Avant la Chute, il y eut un roi.";
    s.push_back(c);
    // 2. Highspire, the crown peaks in gold
    c = CineShot(); c.dur = 8.f; c.tod = 7.2f; c.weather = Game::W_CLEAR;
    c.from = at(H.x - 140, H.y + 120, 45); c.to = at(H.x - 90, H.y + 70, 60); c.lookFrom = at(H.x, H.y, 20); c.lookTo = at(H.x + 40, H.y - 30, 35);
    c.text = "Le Haut-Roi Aldric unit les six royaumes sous une seule couronne : la Couronne-Mère. Six joyaux, six peuples, une seule paix.";
    s.push_back(c);
    // 3. Brindlemere alive
    c = CineShot(); c.dur = 7.f; c.tod = 10.5f; c.weather = Game::W_CLEAR;
    c.from = at(B.x + 60, B.y + 50, 22); c.to = at(B.x + 25, B.y + 35, 14); c.lookFrom = at(B.x, B.y, 3); c.lookTo = at(B.x - 10, B.y, 2);
    c.text = "Les champs étaient lourds de blé. Les routes, sûres. On disait que la couronne chantait, la nuit.";
    s.push_back(c);
    // 3b. 3.9.5 the mammoths of Frostheim on their migration
    {
        // on the frozen lake: open ice, nothing hides the herd (the camera and the herd stand on the ice, not the lake bed)
        const Poi* fl = w.poiById("frozen_lake");
        vec2 F = fl ? fl->pos : vec2(1180, 360);
        auto at = [&](float x, float z, float up) { bool lava = false, frozen = false; float wl = w.waterLevel(x, z, &lava, &frozen); float y = w.height(x, z); if (frozen && wl > y) y = wl; return vec3(x, y + up, z); };
        c = CineShot(); c.dur = 8.f; c.tod = 8.6f; c.weather = Game::W_CLEAR; c.fogMul = 0.25f;
        c.herd = 4; c.herdAt = at(F.x, F.y, 0); c.herdDir = vec3(1, 0, 0.25f); c.herdSpeed = 1.6f;
        c.from = at(F.x - 10, F.y + 24, 6.5f); c.to = at(F.x + 6, F.y + 18, 4.f); c.lookFrom = at(F.x - 4, F.y, 2.5f); c.lookTo = at(F.x + 8, F.y, 2.8f);
        c.text = "Au nord, les mammouths de Frostheim marchaient vers le sud, comme chaque printemps depuis l'aube du monde.";
        s.push_back(c);
    }
    // 4. Morneval at night: the betrayal
    c = CineShot(); c.dur = 8.f; c.tod = 21.3f; c.weather = Game::W_CLOUDY; c.fogTint = vec3(0.55f, 0.35f, 0.8f); c.fogMul = 0.45f; c.music = "shadows";
    c.from = at(M.x + 110, M.y - 120, 30); c.to = at(M.x + 70, M.y - 80, 22); c.lookFrom = at(M.x, M.y, 10); c.lookTo = at(M.x, M.y, 16);
    c.text = "Mais Malgrath, son propre mage, voulut ce qu'aucun homme ne doit posséder : l'éternité.";
    s.push_back(c);
    // 5. the Abyss opens (storm, lightning, violet)
    c = CineShot(); c.dur = 8.f; c.tod = 21.8f; c.weather = Game::W_STORM; c.fogTint = vec3(0.6f, 0.25f, 0.85f); c.fogMul = 0.4f; c.shake = 0.25f; c.sfx = "thunder";
    c.from = at(A.x - 80, A.y - 90, 26); c.to = at(A.x - 35, A.y - 40, 14); c.lookFrom = at(A.x, A.y, 4); c.lookTo = at(A.x, A.y, 0);
    c.text = "Sous Morneval, il ouvrit l'Abîme. Ce qui en sortit n'avait pas de nom. Seulement une faim.";
    c.magic = vec3(0.7f, 0.3f, 1.f);
    s.push_back(c);
    // 6. the sacrifice: the crown breaks into six (golden light in the Guardian's glade)
    c = CineShot(); c.dur = 9.f; c.tod = 18.4f; c.weather = Game::W_CLEAR; c.music = "title";
    c.from = at(R.x + 60, R.y + 60, 18); c.to = at(R.x + 30, R.y + 25, 9); c.lookFrom = at(R.x, R.y, 4); c.lookTo = at(R.x, R.y, 6);
    c.text = "Pour sceller l'Abîme, Aldric brisa la Couronne en six fragments, confiés à six Gardiens. Puis il lia sa propre âme au sceau.";
    c.magic = vec3(1.f, 0.85f, 0.4f);
    s.push_back(c);
    // 7a. 3.9.5 three centuries later: the sky tears open (the multiverse breach), mammoths flee, fragments fall
    for (CineShot& o : s) o.riftFrom = o.riftTo = 0.f;   // before this night, the sky was whole
    {
        vec2 P = B + vec2(70, 90);
        const vec3 R{0.596f, 0.298f, -0.745f};   // the breach's direction (sky shader)
        vec2 Rh = normalize(vec2(R.x, R.z)), side = perp(Rh);
        vec3 eye = at(P.x, P.y, 5);
        c = CineShot(); c.dur = 10.f; c.tod = 22.2f; c.weather = Game::W_STORM; c.fogTint = vec3(0.3f, 0.42f, 0.55f); c.fogMul = 0.35f;
        c.shake = 0.3f; c.sfx = "thunder"; c.music = "shadows"; c.fov = 50.f * DEG;
        c.riftFrom = 0.f; c.riftTo = 1.f; c.meteors = true;
        c.herd = 3; c.herdSpeed = 6.5f; c.herdAt = at(P.x + Rh.x * 34 - side.x * 30, P.y + Rh.y * 34 - side.y * 30, 0); c.herdDir = vec3(side.x, 0, side.y);
        c.from = eye; c.to = eye + vec3(-Rh.x, 0, -Rh.y) * 6.f + vec3(0, 1.5f, 0);
        c.lookFrom = eye + vec3(Rh.x, 0, Rh.y) * 50.f + vec3(0, 3, 0); c.lookTo = eye + vec3(Rh.x, 0, Rh.y) * 50.f + vec3(0, 9, 0);
        c.text = "Trois cents ans ont passé. Le sceau s'est fissuré… et cette nuit-là, le ciel lui-même se déchira.";
        s.push_back(c);
        // 7b. the other worlds look through
        c = CineShot(); c.dur = 9.f; c.tod = 22.4f; c.weather = Game::W_STORM; c.fogTint = vec3(0.3f, 0.42f, 0.55f); c.fogMul = 0.3f;
        c.shake = 0.12f; c.meteors = true; c.fov = 30.f * DEG; c.magic = vec3(1.f, 0.72f, 0.3f);
        vec3 e2 = at(P.x - Rh.x * 20, P.y - Rh.y * 20, 3);
        c.from = e2; c.to = e2 + vec3(side.x, 0, side.y) * 8.f + vec3(0, 3, 0);
        c.lookFrom = e2 + vec3(Rh.x, 0, Rh.y) * 45.f + vec3(0, 11, 0); c.lookTo = e2 + vec3(Rh.x, 0, Rh.y) * 45.f + vec3(0, 14.5f, 0);
        c.text = "Par la brèche, d'autres mondes regardaient le nôtre. Des éclats d'univers inconnus tombaient comme des étoiles.";
        s.push_back(c);
    }
    // 7. the sea storm
    c = CineShot(); c.dur = 8.f; c.tod = 22.f; c.weather = Game::W_STORM; c.shake = 0.15f; c.sfx = "thunder"; c.music = "night";
    vec2 sea, shore;
    if (nearestSea(w, beach, sea, shore)) {
        vec2 out = normalize(sea - shore), side = perp(out);
        vec3 sh = at(shore.x, shore.y, 0);
        c.from = sh - vec3(out.x, 0, out.y) * 30.f + vec3(side.x, 0, side.y) * 20.f + vec3(0, 12, 0);
        c.to = sh - vec3(out.x, 0, out.y) * 12.f + vec3(0, 6, 0);
        c.lookFrom = vec3(sea.x + out.x * 120.f, SEA_LEVEL, sea.y + out.y * 120.f); c.lookTo = vec3(shore.x + out.x * 20.f, SEA_LEVEL, shore.y + out.y * 20.f);
    } else {
        c.from = at(beach.x + 40, beach.y + 120, 14); c.to = at(beach.x + 20, beach.y + 70, 8); c.lookFrom = at(beach.x, beach.y + 200, 0); c.lookTo = at(beach.x, beach.y + 20, 0);
    }
    c.text = "Le multivers s'était ouvert. Les Gardiens murmuraient dans leur sommeil. Et sur la grève, la mer rejeta un naufragé.";
    s.push_back(c);
    // 8. dawn on the shore: the voice
    c = CineShot(); c.dur = 6.5f; c.tod = 6.1f; c.weather = Game::W_CLOUDY; c.fogTint = vec3(0.75f, 0.7f, 0.85f); c.fogMul = 0.6f; c.music = "";
    c.heroDown = true; c.sfx = "boss_intro";
    c.from = at(beach.x + 5, beach.y - 6, 3.5f); c.to = at(beach.x + 2.5f, beach.y - 3.5f, 1.6f); c.lookFrom = at(beach.x, beach.y, 0.4f); c.lookTo = at(beach.x, beach.y, 0.3f);
    c.speaker = "Une voix"; c.speakerColor = vec3(0.78f, 0.55f, 1.f);
    c.text = "« Tu es revenu… »";
    s.push_back(c);
    c = CineShot(); c.dur = 5.f; c.tod = 6.15f; c.weather = Game::W_CLOUDY; c.fogTint = vec3(0.75f, 0.7f, 0.85f); c.fogMul = 0.6f;
    c.heroDown = true; c.wristGlow = true;
    c.from = at(beach.x + 2.5f, beach.y - 3.5f, 1.6f); c.to = at(beach.x + 1.2f, beach.y - 1.8f, 0.9f); c.lookFrom = at(beach.x, beach.y, 0.3f); c.lookTo = at(beach.x, beach.y, 0.35f);
    c.speaker = "Une voix"; c.speakerColor = vec3(0.78f, 0.55f, 1.f);
    c.text = "« …trop tard. »";
    s.push_back(c);
    // 9. the hero gets up; the mark burns on his wrist
    c = CineShot(); c.dur = 6.f; c.tod = 6.3f; c.weather = Game::W_CLEAR; c.music = "greenhaven"; c.heroRise = true; c.wristGlow = true;
    c.from = at(beach.x + 4, beach.y - 4, 1.8f); c.to = at(beach.x + 5, beach.y - 6, 2.6f); c.lookFrom = at(beach.x, beach.y, 1.f); c.lookTo = at(beach.x, beach.y, 1.3f);
    c.text = "Sans nom. Sans souvenirs. Seulement une marque en forme de couronne, qui brûle au poignet.";
    c.title = "PROLOGUE"; c.subtitle = "Un réveil sur la grève";
    s.push_back(c);
    return s;
}

void Game::startPrologue() {
    startShots(prologueShots(*this), "prologue");
}

void Game::startShots(std::vector<CineShot> shots, const std::string& id) {
    if (shots.empty()) return;
    cine.shots = std::move(shots);
    cine.id = id;
    cine.index = 0; cine.t = 0; cine.reveal = 0; cine.active = true; cine.fade = 1.f;
    cine.savedTod = timeOfDay; cine.savedWeather = weather;
    cine.savedMusic = audio.currentMusic();
    beginShot();
    AGR_LOG("cinematic '%s': %zu shots", id.c_str(), cine.shots.size());
}

// 3.9.5 fragments of other worlds falling from the breach during the prologue
struct CineMeteor { vec3 a, b; float t = 0, dur = 1.f; vec3 col; };
static std::vector<CineMeteor> gMeteors;

static void clearHerd(Game& g) {
    for (Entity e : g.cineHerd) if (g.reg.valid(e)) g.reg.destroyLater(e);
    g.cineHerd.clear();
}

float Game::cinematicRift(float* flash) const {
    if (flash) *flash = 0;
    if (!cine.active || cine.index >= (int)cine.shots.size()) return 1.f;
    const CineShot& c = cine.shots[cine.index];
    if (c.riftFrom < 0) return 1.f;
    if (c.riftFrom == c.riftTo) return c.riftFrom;
    // it tears in jolts: three surges with a pause between them
    float u = saturate((cine.t - 1.2f) / std::max(1.f, c.dur - 3.f));
    float j = u + 0.08f * std::sin(u * 3.f * TAU) * (1.f - u);
    float v = c.riftFrom + (c.riftTo - c.riftFrom) * easeInOut(saturate(j));
    if (flash) *flash = std::max(0.f, std::sin(u * 3.f * TAU)) * (1.f - u * 0.6f) * (u > 0.f ? 1.f : 0.f);
    return v;
}

void Game::beginShot() {
    const CineShot& c = cine.shots[cine.index];
    clearHerd(*this);
    gMeteors.clear();
    for (int i = 0; i < c.herd; i++) {
        vec3 d = normalize(c.herdDir);
        vec3 side{-d.z, 0, d.x};
        float fwdOff = -i * 9.f + particles.rng.range(-2.f, 2.f), sideOff = (i % 2 ? 1.f : -1.f) * (2.f + i * 1.5f);
        vec3 p = c.herdAt + d * fwdOff + side * sideOff;
        p.y = groundAt(p.x, p.z);
        Entity e = spawnAnimal(9, p);
        if (Character* ch = reg.get<Character>(e)) { ch->yaw = std::atan2(d.x, d.z); if (i == c.herd - 1 && c.herd > 2) ch->vis.scale *= 0.6f; }   // a calf at the back
        if (AnimalBrain* ab = reg.get<AnimalBrain>(e)) { ab->forceSpeed = c.herdSpeed * (i == c.herd - 1 && c.herd > 2 ? 1.1f : 1.f); ab->dest = p + d * 200.f; ab->home = ab->dest; }
        cineHerd.push_back(e);
    }
    cine.t = 0; cine.reveal = 0;
    if (c.tod >= 0) timeOfDay = c.tod;
    if (c.weather >= 0) { weather = weatherNext = c.weather; weatherBlend = 1; weatherTimer = 999; }
    if (c.music != "-") audio.playMusic(c.music, 2.f);
    if (!c.sfx.empty()) play(c.sfx, 0.9f);
    camFocus = c.lookFrom;   // the world streams around what the camera looks at
    cine.waitLoad = 0.0001f; cine.loadReady = 0; // wait for it before fading in
    if (player != NullEntity) {
        Character& p = pc();
        if (c.heroDown) { p.vis.anim.setState(BodyState::Downed, 0.05f); p.vel = vec3(0); }
        if (c.heroRise) p.vis.anim.setState(BodyState::Locomotion, 1.6f);
    }
}

void Game::endCinematicShots(bool skipped) {
    if (!cine.active) return;
    std::string id = cine.id;
    cine.active = false;
    clearHerd(*this); gMeteors.clear();
    timeOfDay = cine.id == "prologue" ? 7.5f : cine.savedTod;
    weather = weatherNext = cine.id == "prologue" ? W_CLEAR : cine.savedWeather; weatherBlend = 1; weatherTimer = 120;
    if (player != NullEntity) {
        Character& p = pc();
        p.vis.anim.setState(BodyState::Locomotion, 0.3f);
        camFocus = p.pos + vec3(0, 1.1f, 0);
        camYaw = p.yaw + PI;
    }
    camCineBlend = 0;
    if (id == "prologue") {
        storyFlags.insert("prologue_seen");
        for (const char* l : {"l1", "l2", "l3"}) unlockLore(l);
        audio.playMusic("greenhaven", 3.f);
        toastHint.clear();
        if (!autoplay) startTutorial();   // 2.1.0 the tutorial replaces the single line of hints
        else { toastHint = "Joystick à gauche pour marcher · Bouton épée : attaquer (maintenir pour charger) · Glisser à droite pour tourner la caméra."; hintT = 0; }
    }
    AGR_LOG("cinematic '%s' %s", id.c_str(), skipped ? "skipped" : "finished");
    if (replayingChapter) replayingChapter = false;   // watched again from the Objectives tab: no consequences
    else if (id != "prologue") onChapterEnd(id);
}

void Game::updateCinematicShots(float dt) {
    if (!cine.active) return;
    if (cine.index >= (int)cine.shots.size()) { endCinematicShots(false); return; }
    const CineShot& c = cine.shots[cine.index];
    // hold on black until the scenery of the shot is streamed in (max 4 s, so a slow phone never blocks)
    if (cine.waitLoad > 0) {
        cine.waitLoad += dt;
        // 2.1.0: wait for the whole picture (close detail and the mid-distance terrain), up to 10 s, with a progress bar
        float ready = std::min(world.areaReady(c.lookFrom, 90.f), world.areaReady(c.lookFrom, 200.f) * 1.02f);
        cine.loadReady = std::max(cine.loadReady, ready);
        cine.fade = 1.f;
        camFocus = c.lookFrom;
        if (ready < 0.995f && cine.waitLoad < 10.f) return;
        cine.waitLoad = 0; cine.loadReady = 0;
    }
    cine.t += dt;
    if (cine.t > 0.6f) cine.reveal += dt * 38.f;
    // fade: dip to black between shots
    float fin = saturate((cine.t - 0.3f) / 0.8f), fout = saturate((c.dur - cine.t) / 0.5f);   // black for 0.3 s: the world streams in
    cine.fade = 1.f - std::min(fin, cine.index + 1 < (int)cine.shots.size() ? fout : 1.f);
    camFocus = lerp(c.lookFrom, c.lookTo, easeInOut(saturate(cine.t / c.dur)));
    if (c.shake > 0) shake = std::max(shake, c.shake * (0.5f + 0.5f * std::sin(cine.t * 3.f)));
    if (c.sfx == "thunder" && std::fmod(cine.t, 3.1f) < dt) { lightningFlash = 1.f; play("thunder", 0.6f, 0.9f + 0.2f * particles.rng.f()); }
    if (length(c.magic) > 0 && particles.rng.f() < dt * 18) {
        vec3 p = c.lookTo + vec3(particles.rng.range(-6.f, 6.f), particles.rng.range(0.f, 6.f), particles.rng.range(-6.f, 6.f));
        particles.magicTrail(p, c.magic, 1.4f);
    }
    if (c.meteors && cine.waitLoad <= 0) {
        // fragments fall from the breach towards the land ahead of the camera, burn through the sky and burst on the ground
        vec3 eye, look; float fv;
        if (cinematicCamera(eye, look, fv) && particles.rng.f() < dt * 2.2f && gMeteors.size() < 5) {
            const vec3 R{0.596f, 0.298f, -0.745f};
            vec2 Rh = normalize(vec2(R.x, R.z)), sd = perp(Rh);
            vec2 g2 = eye.xz() + Rh * particles.rng.range(60.f, 200.f) + sd * particles.rng.range(-110.f, 110.f);
            CineMeteor m;
            m.b = vec3(g2.x, groundAt(g2.x, g2.y), g2.y);
            m.a = eye + R * 420.f + vec3(particles.rng.range(-60.f, 60.f), particles.rng.range(-20.f, 40.f), particles.rng.range(-60.f, 60.f));
            m.dur = particles.rng.range(0.9f, 1.5f);
            static const vec3 cols[] = {{1.f, 0.72f, 0.28f}, {1.f, 0.9f, 0.65f}, {1.f, 0.45f, 0.15f}, {0.35f, 0.75f, 0.8f}};   // 3.10.0 amber
            m.col = cols[particles.rng.irange(0, 3)];
            gMeteors.push_back(m);
        }
        for (size_t i = 0; i < gMeteors.size();) {
            CineMeteor& m = gMeteors[i];
            m.t += dt;
            float u = saturate(m.t / m.dur);
            vec3 dir = normalize(m.b - m.a), head = lerp(m.a, m.b, u * u * (2.f - u));
            vfxBeam(head - dir * 28.f, head, m.col, 0.9f, 0.06f, 0.6f);
            if (particles.rng.f() < 0.6f) particles.magicTrail(head, m.col, 2.2f);
            if (u >= 1.f) {
                particles.explosion(m.b, 3.2f, m.col);
                particles.dust(m.b, 4.f);
                lightningFlash = std::max(lightningFlash, 0.35f);
                float dd = length(m.b - eye);
                play("thunder", clampf(60.f / std::max(dd, 20.f), 0.15f, 0.7f), 1.4f + particles.rng.f() * 0.3f);
                if (dd < 120.f) shake = std::max(shake, 0.35f);
                gMeteors.erase(gMeteors.begin() + i);
                continue;
            }
            ++i;
        }
    }
    if (c.wristGlow && player != NullEntity && particles.rng.f() < dt * 25) {
        Character& p = pc();
        vec3 hand = p.pos + vec3(0, 0.25f, 0);
        if (p.vis.mesh) hand = mat4::trs(p.pos, quat::axisAngle({0, 1, 0}, p.yaw), vec3(p.vis.scale)).transformPoint(p.vis.pw.wpos[H_HANDL]);
        particles.magicTrail(hand, vec3(1.f, 0.8f, 0.35f), 0.35f);
    }
    if (player != NullEntity) { Character& p = pc(); p.invuln = std::max(p.invuln, 0.5f); p.vel.x = p.vel.z = 0; }
    // input: tap = finish the line / next shot; the skip button ends everything
    bool tapped = false, skip = false;
    Rect skipBtn{ui.W - 170, 18, 150, 48};
    for (auto& f : ui.fingers) {
        if (!f.released || f.consumed) continue;
        f.consumed = true;
        if (skipBtn.contains(f.x, f.y) && skipBtn.contains(f.sx, f.sy)) skip = true; else tapped = true;
    }
    if (skip) { endCinematicShots(true); return; }
    bool lineDone = cine.reveal >= (float)c.text.size();
    if (tapped) { if (!lineDone) cine.reveal = 1e9f; else cine.t = std::max(cine.t, c.dur); }
    if (cine.t >= c.dur) {
        cine.index++;
        if (cine.index >= (int)cine.shots.size()) { endCinematicShots(false); return; }
        beginShot();
    }
}

bool Game::cinematicCamera(vec3& eye, vec3& look, float& fov) const {
    if (!cine.active || cine.index >= (int)cine.shots.size()) return false;
    const CineShot& c = cine.shots[cine.index];
    float u = easeInOut(saturate(cine.t / c.dur));
    eye = lerp(c.from, c.to, u);
    look = lerp(c.lookFrom, c.lookTo, u);
    fov = c.fov;
    return true;
}

void Game::applyCinematicEnvironment() {
    if (!cine.active || cine.index >= (int)cine.shots.size()) return;
    const CineShot& c = cine.shots[cine.index];
    env.fogDensity *= c.fogMul;
    if (length(c.fogTint) > 0) { env.fogColor = lerp(env.fogColor, c.fogTint * (0.25f + 0.5f * (1 - env.night)), 0.45f); env.saturation *= 0.92f; }
    env.tiltShift = 0.f;
    env.vignette = 0.5f;
}

void Game::drawCinematicShots() {
    if (!cine.active || cine.index >= (int)cine.shots.size()) return;
    UI& u = ui;
    const CineShot& c = cine.shots[cine.index];
    float W_ = u.W, H_ = u.H;
    // dip to black, letterbox
    if (cine.fade > 0.01f) u.rect({0, 0, W_, H_}, vec4(0, 0, 0, cine.fade));
    if (cine.waitLoad > 0) { drawLoadOverlay(W_, H_, cine.loadReady, 0.f, cine.waitLoad); return; }
    float bar = 92;
    u.rect({0, 0, W_, bar}, vec4(0, 0, 0, 1));
    u.rect({0, H_ - bar - 40, W_, bar + 40}, vec4(0, 0, 0, 1));
    // narration (typewriter), speaker above in colour
    size_t n = std::min(c.text.size(), (size_t)std::max(0.f, cine.reveal));
    while (n < c.text.size() && (c.text[n] & 0xC0) == 0x80) n++;   // keep UTF-8 intact
    float ty = H_ - bar - 18;
    if (!c.speaker.empty()) u.text(c.speaker, W_ / 2, ty - 6, 18, vec4(c.speakerColor, 0.9f), F_BOLD, A_CENTER);
    u.text(c.text.substr(0, n), W_ / 2, ty + 18, c.speaker.empty() ? 25 : 30, c.speaker.empty() ? rgba(0xf4e7c4) : vec4(c.speakerColor, 1), c.speaker.empty() ? F_SERIF : F_TITLE, A_CENTER, W_ * 0.78f, true);
    // title card (last part of the shot)
    if (!c.title.empty()) {
        float a = saturate((cine.t - 1.0f) * 1.2f) * saturate((c.dur - cine.t) * 2.f + 0.3f);
        u.text(c.title, W_ / 2, H_ * 0.30f, 74, rgba(0xf6dc9a, a), F_TITLE, A_CENTER, 0, true);
        u.sprite("divider", {W_ / 2 - 240, H_ * 0.30f + 92, 480, 24}, vec4(1, 1, 1, a));
        u.text(c.subtitle, W_ / 2, H_ * 0.30f + 122, 30, rgba(0xe8dcc0, a), F_SERIF, A_CENTER, 0, true);
    }
    // skip button + progress dots
    Rect skipBtn{W_ - 170, 18, 150, 48};
    u.nine("button_dark", skipBtn, vec4(1, 1, 1, 0.85f), 0.7f);
    u.text("Passer  ›", skipBtn.x + skipBtn.w / 2, skipBtn.y + 13, 19, rgba(0xf4e7c4), F_SERIF, A_CENTER);
    u.tag(skipBtn, "Passer");
    for (int i = 0; i < (int)cine.shots.size(); i++)
        u.circle({W_ / 2 + (i - (cine.shots.size() - 1) * 0.5f) * 18.f, bar * 0.5f}, i == cine.index ? 5.f : 3.f, rgba(0xf6dc9a, i <= cine.index ? 0.9f : 0.3f), 10);
}

// 2.1.0 loading screen: title, a progress bar and a tip. alpha > 0 also dims what is behind (the black is drawn by the caller otherwise)
void Game::drawLoadOverlay(float W_, float H_, float progress, float alpha, float t) {
    UI& u = ui;
    if (alpha > 0.f) u.rect({0, 0, W_, H_}, vec4(0.02f, 0.015f, 0.01f, alpha));
    float a = alpha > 0.f ? alpha : 1.f;
    u.text("LOST KINGDOM", W_ / 2, H_ * 0.33f, 54, rgba(0xf6dc9a, a), F_TITLE, A_CENTER, 0, true);
    u.sprite("divider", {W_ / 2 - 200, H_ * 0.33f + 70, 400, 22}, vec4(1, 1, 1, a));
    float bw = std::min(W_ * 0.5f, 480.f), bx = W_ / 2 - bw / 2, by = H_ * 0.33f + 118;
    u.rect({bx - 3, by - 3, bw + 6, 20}, vec4(0.3f, 0.22f, 0.12f, 0.9f * a));
    u.rect({bx, by, bw, 14}, vec4(0.08f, 0.06f, 0.04f, 0.95f * a));
    float p = saturate(std::max(progress, 0.03f + 0.03f * std::sin(t * 4.f)));
    u.rect({bx, by, bw * p, 14}, vec4(0.96f, 0.78f, 0.35f, a));
    char b[64]; snprintf(b, sizeof b, "Chargement du monde…  %d %%", (int)(saturate(progress) * 100.f));
    u.text(b, W_ / 2, by + 28, 20, rgba(0xe8dcc0, a), F_BODY, A_CENTER);
    static const char* tips[] = {
        "Astuce : touchez deux fois la zone de caméra pour la recentrer.",
        "Astuce : le bouton « Courir » verrouille la course ; touchez-le de nouveau pour marcher.",
        "Astuce : les pierres de passage enregistrent votre point de retour et permettent le voyage rapide.",
        "Astuce : une esquive lancée juste avant un coup vous rend invulnérable.",
        "Astuce : à Port-Azur et à Solmar, les marchands vendent de quoi affronter l'Acte II.",
        "Astuce : touchez un ennemi pour le verrouiller, touchez le sol pour le relâcher.",
    };
    int ti = ((int)(t / 4.f) + loadTip) % 6;
    u.text(tips[ti], W_ / 2, by + 74, 18, rgba(0xb8aa90, a), F_BODY, A_CENTER, std::min(W_ * 0.8f, 700.f));
}

void Game::unlockLore(const std::string& id, bool announce) {
    if (loreFound.count(id)) return;
    for (const auto& l : lore()) if (l.id == id) {
        loreFound.insert(id);
        if (announce) notify("Chronique : " + l.title, vec3(0.9f, 0.8f, 1.f), "i_quest");
        return;
    }
}

}  // namespace lk
