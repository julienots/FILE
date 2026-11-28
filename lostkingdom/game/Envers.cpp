// LOST KINGDOM — 3.10.0 the Amber ("l'Ambre"), the playable side of the multiverse.
//
// Malgrath wanted eternity. In one reflection of the kingdom he got it: Aldric hesitated a second too long, the
// mage laid his hand on the six jewels and said "let nothing end". Since then nothing has ended there. The sun hangs
// at the same low hour, the dust does not settle, the people still walk, made of amber, without knowing why.
//
// Design (references: Titanfall 2's "Effect and Cause" — two perfectly aligned versions of one place and an instant
// shift; Zelda "A Link Between Worlds" — cracks lead to the mirror world; Metroid Prime 2 — the dark world wears you
// down): the Amber IS the same land, so the hero comes back exactly where he stands. Rifts in the ground, where the
// fragments of the breach fell, open the way. Inside, time is frozen at the golden hour, the living are gone and the
// "Figés" (frozen ones) wander; six memories caught in the amber tell what happened there. Eternity slowly claims
// the hero (a gauge); when it is full the Amber throws him back. He can also leave at any moment.
#include "Game.h"
#include "../engine/core/Platform.h"

namespace lk {

static const vec3 AMBER_GOLD{1.f, 0.72f, 0.28f};

void Game::initAmber() {
    for (Entity e : amberSpots) if (reg.valid(e)) reg.destroy(e);
    amberSpots.clear(); amberRifts.clear(); amberMemories.clear();
    auto onLand = [&](vec2 p) {
        float wl = world.waterLevel(p.x, p.y);
        return wl < -1e8f || world.height(p.x, p.y) > wl + 0.4f;
    };
    auto place = [&](vec2 p) -> bool {
        for (int k = 0; k < 12; k++) {
            vec2 q = p + vec2(std::cos(k * 2.3f), std::sin(k * 2.3f)) * (k * 4.f);
            if (q.x < 20 || q.y < 20 || q.x > WORLD_SIZE - 20 || q.y > WORLD_SIZE - 20 || !onLand(q)) continue;
            amberRifts.push_back(vec3(q.x, world.groundHeight(q.x, q.y), q.y));
            return true;
        }
        return false;
    };
    // the first rift: where the breach tore open in the prologue, a short walk from the shore where the hero wakes
    const Poi* brind = world.poiById("brindlemere");
    vec2 B = brind ? brind->pos : vec2(1480, 1620);
    place(B + vec2(70, 90));
    // then one beside each waystone: every land has a crack where a fragment fell
    for (const Poi& p : world.pois) if (p.type == PoiType::Waystone) place(p.pos + vec2(16, 13));
    // six memories, each at a different rift, a little away from it (they are only there in the Amber)
    for (int i = 0; i < 6 && i < (int)amberRifts.size(); i++) {
        int ri = (i * 5) % (int)amberRifts.size();
        if (i == 0) ri = 0;
        vec2 o = vec2(std::cos(i * 1.7f + 0.4f), std::sin(i * 1.7f + 0.4f)) * 22.f;
        vec2 q = amberRifts[ri].xz() + o;
        if (!onLand(q)) q = amberRifts[ri].xz() - o;
        amberMemories.push_back(vec3(q.x, world.groundHeight(q.x, q.y), q.y));
    }
    for (int i = 0; i < (int)amberRifts.size(); i++) {
        Entity e = reg.create();
        reg.add<AmberSpot>(e, AmberSpot{0, i, amberRifts[i]});
        Interact it; it.kind = InterKind::Amber; it.radius = 2.6f; it.text = "Traverser la faille";
        reg.add<Interact>(e, it);
        amberSpots.push_back(e);
    }
    for (int i = 0; i < (int)amberMemories.size(); i++) {
        Entity e = reg.create();
        reg.add<AmberSpot>(e, AmberSpot{1, i, amberMemories[i]});
        Interact it; it.kind = InterKind::Amber; it.radius = 2.2f; it.text = "Toucher le souvenir figé";
        reg.add<Interact>(e, it);
        amberSpots.push_back(e);
    }
    AGR_LOG("amber: %zu rifts, %zu memories", amberRifts.size(), amberMemories.size());
}

void Game::onAmberSpot(Entity e) {
    AmberSpot* s = reg.get<AmberSpot>(e);
    if (!s) return;
    if (s->kind == 0) { if (amber) exitAmber(false); else enterAmber(s->idx); return; }
    std::string id = "ambre_" + std::to_string(s->idx + 1);
    if (loreFound.count(id)) return;
    unlockLore(id, true);
    Character& c = pc();
    particles.magicBurst(s->pos + vec3(0, 1.2f, 0), AMBER_GOLD, 1.4f, 36);
    particles.shockwave(s->pos, AMBER_GOLD, 3.f);
    play("magic_burst", 0.8f);
    gainXp(60);
    int found = 0; for (int i = 1; i <= 6; i++) if (loreFound.count("ambre_" + std::to_string(i))) found++;
    char b[96]; snprintf(b, sizeof b, "Souvenir d'ambre %d / 6 : lisez-le dans le Journal (Chroniques)", found);
    notify(b, AMBER_GOLD, "i_star");
    amberEternity = std::max(0.f, amberEternity - 25.f);   // a memory set free gives the hero back a little time
    if (found == 6) { notify("Les six souvenirs sont libres. Tu sais maintenant qui t'attendait sur la grève.", AMBER_GOLD, "i_star"); gainXp(400); }
    (void)c;
}

void Game::enterAmber(int rift) {
    if (amber || player == NullEntity || dungeon.active || riding || driving >= 0) {
        if (riding || driving >= 0) notify("Descendez de votre monture pour traverser la faille.", vec3(1, 0.85f, 0.6f), "i_lock");
        return;
    }
    amber = true; amberEternity = 0; amberEchoT = 2.5f; amberShiftT = 0; amberHeraldSpawned = false;
    Character& c = pc();
    particles.shockwave(c.pos, AMBER_GOLD, 6.f);
    particles.magicBurst(c.pos + vec3(0, 1.f, 0), AMBER_GOLD, 2.f, 60);
    play("boss_intro", 0.7f); play("magic_burst", 0.9f);
    addShake(0.35f); haptic(120, 200);
    lightningFlash = std::max(lightningFlash, 0.6f);
    regionBanner = "L'Ambre"; regionBannerSub = "Le royaume où rien ne finit"; regionBannerT = 4.f;
    if (!storyFlags.count("amber_seen")) {
        storyFlags.insert("amber_seen");
        unlockLore("ambre_0", true);
        toastHint = "Ici le temps s'est arrêté. La jauge d'éternité monte : quand elle est pleine, l'Ambre te rejette. Bouton « Revenir » pour rentrer quand tu veux.";
        hintT = 0;
    }
    AGR_LOG("amber: entered by rift %d", rift);
}

void Game::exitAmber(bool forced) {
    if (!amber) return;
    amber = false; amberShiftT = 0;
    reg.each<AmberEcho>([&](Entity e, AmberEcho&) { reg.destroyLater(e); });
    reg.each<Character>([&](Entity, Character& o) { o.vis.visible = true; });
    if (player != NullEntity) {
        Character& c = pc();
        particles.shockwave(c.pos, vec3(0.8f, 0.9f, 1.f), 5.f);
        play("magic_burst", 0.8f);
        addShake(forced ? 0.5f : 0.25f);
        if (forced) { c.hp = std::max(1.f, c.hp - c.maxHp * 0.15f); c.vis.anim.setState(BodyState::Downed, 0.1f); c.hitStun = 1.2f; notify("L'Ambre t'a rejeté. Le temps reprend son cours.", AMBER_GOLD, "i_lock"); }
    }
    regionBannerT = 0;
    lastRegion = -1;   // the land's own banner comes back
    AGR_LOG("amber: left (%s)", forced ? "thrown out" : "by choice");
}

void Game::updateAmber(float dt) {
    if (amberRifts.empty() && player != NullEntity) initAmber();
    amberK = damp(amberK, amber ? 1.f : 0.f, 2.2f, dt);
    if (amberShiftT >= 0) { amberShiftT += dt; if (amberShiftT > 1.2f) amberShiftT = -1; }
    worldTint = lerp(vec3(1.f), vec3(1.1f, 0.86f, 0.56f), amberK);
    if (player == NullEntity) return;
    Character& c = pc();
    if (amber && (dungeon.active || c.dead)) exitAmber(false);
    // ---- the rifts: a jagged line of light in the ground, embers rising from it (both worlds); a hint nearby
    amberRiftNear = false;
    for (int i = 0; i < (int)amberRifts.size(); i++) {
        vec3 r = amberRifts[i];
        float d = distXZ(r, camFocus);
        if (d > 70.f) continue;
        if (distXZ(r, c.pos) < 14.f) amberRiftNear = true;
        float rate = d < 30.f ? 26.f : 9.f;
        Rng rg((uint32_t)i * 7919u);
        float ang = rg.range(0.f, PI);
        vec3 ax{std::cos(ang), 0, std::sin(ang)}, sd{-ax.z, 0, ax.x};
        if (particles.rng.f() < dt * rate) {
            float u = particles.rng.range(-1.f, 1.f);
            vec3 p = r + ax * (u * 2.4f) + sd * (std::sin(u * 7.f + i) * 0.35f) + vec3(0, 0.08f, 0);
            particles.magicTrail(p + vec3(0, particles.rng.range(0.f, 1.8f), 0), amber ? vec3(0.75f, 0.9f, 1.f) : AMBER_GOLD, 0.5f);
        }
        // the rift itself: a jagged tear standing in the air, a small brother of the breach in the sky (taller than the
        // grass, readable from afar), breathing, with a wider dim halo behind the white-hot seam
        if (d < 60.f) {
            const int N = 9;
            vec3 pts[N];
            float breathe = 1.f + 0.06f * std::sin(gameTime * 1.7f + i);
            for (int k = 0; k < N; k++) {
                float u = k / float(N - 1);
                float zig = (k % 2 ? 1.f : -1.f) * rg.range(0.12f, 0.38f) * std::sin(u * PI);
                pts[k] = r + vec3(0, 0.25f + u * 3.4f * breathe, 0) + ax * (zig + (u - 0.5f) * 0.5f);
            }
            vec3 cc = amber ? vec3(0.75f, 0.9f, 1.f) : AMBER_GOLD;
            float flick = 0.8f + 0.2f * std::sin(gameTime * 11.f + i * 3.f);
            for (int k = 0; k < N - 1; k++) {
                float w = std::sin((k + 0.5f) / (N - 1) * PI);
                vfxBeam(pts[k], pts[k + 1], lerp(cc, vec3(1.f), 0.12f) * flick * 0.75f, 0.025f + 0.05f * w, 0.05f, k == N / 2 ? 0.9f : 0.f);   // one light per rift, not one per segment
                if (k % 2 == 0) vfxBeam(pts[k], pts[k + 1], cc * 0.22f, 0.1f + 0.14f * w, 0.05f, 0.f);
            }
            if (particles.rng.f() < dt * 6.f) particles.sparks(pts[particles.rng.irange(1, N - 2)], vec3(particles.rng.range(-1.f, 1.f), 0.5f, particles.rng.range(-1.f, 1.f)), 3, cc);
        }
    }
    if (amberRiftNear && !amber && !storyFlags.count("amber_hint")) {
        storyFlags.insert("amber_hint");
        notify("Une faille d'ambre : le ciel s'est déchiré ici. Approchez et touchez-la.", AMBER_GOLD, "i_star");
    }
    if (!amber) return;
    // ---- inside the Amber: the hour does not move, eternity rises, the living are gone, the Figés wander
    amberEternity += dt * (100.f / 150.f);   // two and a half minutes
    if (amberEternity >= 100.f) { exitAmber(true); return; }
    if (amberEternity > 80.f && amberEternity - dt * (100.f / 150.f) <= 80.f) notify("L'éternité te gagne. Reviens vite.", AMBER_GOLD, "i_lock");
    reg.each<Character>([&](Entity e, Character& o) {
        if (e == player || e == mountEnt || reg.get<AmberEcho>(e)) return;
        o.vis.visible = false;   // they live in the other world
    });
    // dust and embers hang in the air, motionless: a few new motes around the hero each frame
    if (particles.rng.f() < dt * 30.f) {
        vec3 p = c.pos + vec3(particles.rng.range(-14.f, 14.f), particles.rng.range(0.3f, 6.f), particles.rng.range(-14.f, 14.f));
        particles.magicTrail(p, vec3(1.f, 0.8f, 0.45f), 0.25f);
    }
    // memories: a slow golden heart beating in the amber, visible from afar
    for (int i = 0; i < (int)amberMemories.size(); i++) {
        if (loreFound.count("ambre_" + std::to_string(i + 1))) continue;
        vec3 m = amberMemories[i];
        if (distXZ(m, c.pos) > 90.f) continue;
        float beat = 0.5f + 0.5f * std::sin(gameTime * 2.4f + i);
        if (particles.rng.f() < dt * 14.f) particles.magicTrail(m + vec3(particles.rng.range(-0.4f, 0.4f), 1.1f + particles.rng.range(-0.4f, 0.4f), particles.rng.range(-0.4f, 0.4f)), vec3(1.f, 0.75f, 0.3f), 0.6f + beat * 0.5f);
        if (particles.rng.f() < dt * 1.5f) vfxPillar(m, vec3(1.f, 0.7f, 0.25f), 0.25f, 9.f, 0.7f);
        // the memory: a slowly turning heart of amber floating at chest height, beating
        if (particles.rng.f() < dt * 3.f) vfxSphere(m + vec3(0, 1.3f + 0.12f * std::sin(gameTime * 1.3f + i), 0), vec3(1.f, 0.8f, 0.4f), vec3(1.f, 0.55f, 0.15f), 0.32f + 0.06f * beat, 0.4f, 1);
    }
    // ---- 3.10.4 the Herald: once three memories are free, the guardian of the forge door wakes on the next crossing
    {
        int freed = 0; for (int i = 1; i <= 6; i++) if (loreFound.count("ambre_" + std::to_string(i))) freed++;
        if (!amberHeraldSpawned && freed >= 3 && !storyFlags.count("amber_herald_dead") && amberEternity > 8.f && !cine.active) {
            int gi = enemyIndex("stone_golem");
            float a = particles.rng.range(0.f, TAU);
            vec3 p = c.pos + vec3(std::cos(a) * 16.f, 0, std::sin(a) * 16.f);
            p.y = groundAt(p.x, p.z);
            if (gi >= 0) {
                amberHeraldSpawned = true;
                Entity e = spawnEnemy(gi, p, true, 0);
                if (Character* o = reg.get<Character>(e)) {
                    o->name = "Le Héraut figé";
                    o->vis.tint = vec3(1.8f, 1.3f, 0.6f); o->vis.emissiveBoost = 0.85f; o->vis.rim = 1.4f; o->vis.dissolveColor = AMBER_GOLD;
                    o->vis.scale *= 1.45f; o->radius *= 1.4f; o->height *= 1.45f;
                    o->level = std::max(level + 2, 5);
                    o->maxHp = o->hp = 260.f + level * 70.f;
                    o->atk = 9.f + level * 2.6f;
                    o->yaw = std::atan2(c.pos.x - p.x, c.pos.z - p.z);
                }
                reg.add<AmberEcho>(e, AmberEcho{});
                reg.add<AmberHerald>(e, AmberHerald{});
                lockTarget = e;
                particles.shockwave(p, AMBER_GOLD, 8.f);
                particles.magicBurst(p + vec3(0, 2.f, 0), AMBER_GOLD, 2.5f, 70);
                play("golem", 1.f, 0.8f); play("boss_intro", 0.8f); addShake(0.6f); haptic(150, 230);
                regionBanner = "Le Héraut figé"; regionBannerSub = "Gardien de la porte de la forge"; regionBannerT = 3.5f;
                amberEternity = std::max(0.f, amberEternity - 40.f);   // the fight gets its time
                AGR_LOG("amber: the Herald wakes (freed %d)", freed);
            }
        }
    }
    // the Figés: echoes of the creatures of this land, frozen in amber, stronger, worth more
    int echoes = 0;
    reg.each<AmberEcho, Character>([&](Entity, AmberEcho&, Character& o) { if (!o.dead) echoes++; });
    amberEchoT -= dt;
    if (amberEchoT <= 0 && echoes < 4 && !cine.active) {
        amberEchoT = 8.f;
        int region = world.regionId(c.pos.x, c.pos.z);
        std::vector<int> pool;
        const auto& E = enemies();
        for (int i = 0; i < (int)E.size(); i++)
            if (E[i].region == region && E[i].behavior != Behavior::Boss && !E[i].hunter && E[i].spawnWeight > 0) pool.push_back(i);
        if (pool.empty()) for (int i = 0; i < (int)E.size(); i++) if (E[i].behavior != Behavior::Boss && !E[i].hunter && E[i].level <= std::max(2, level)) pool.push_back(i);
        if (!pool.empty()) {
            float a = particles.rng.range(0.f, TAU), r = particles.rng.range(14.f, 22.f);
            vec3 p = c.pos + vec3(std::cos(a) * r, 0, std::sin(a) * r);
            p.y = groundAt(p.x, p.z);
            float wl = waterAt(p.x, p.z);
            if (wl < -1e8f || p.y > wl + 0.3f) {
                Entity e = spawnEnemy(pool[particles.rng.irange(0, (int)pool.size() - 1)], p, false, 0);
                if (Character* o = reg.get<Character>(e)) {
                    o->name = "Figé · " + o->name;
                    o->vis.tint = vec3(1.35f, 0.98f, 0.5f); o->vis.emissiveBoost = 0.35f; o->vis.rim = 1.f;
                    o->vis.dissolveColor = AMBER_GOLD;
                    o->maxHp *= 1.35f; o->hp = o->maxHp; o->level += 2;
                }
                reg.add<AmberEcho>(e, AmberEcho{});
                particles.magicBurst(p + vec3(0, 1.f, 0), AMBER_GOLD, 1.2f, 24);
            }
        }
    }
}

// the Amber's light: the sun stopped at the golden hour, a honey sky over a deep blue zenith, warm haze, the breach
// wide open overhead, a grade that warms the highlights and keeps the shadows cool (blended by amberK)
void Game::applyAmberEnvironment() {
    float k = amberK;
    if (k < 0.002f) return;
    auto L = [&](vec3 a, vec3 b) { return lerp(a, b, k); };
    auto Lf = [&](float a, float b) { return a + (b - a) * k; };
    env.sunDir = normalize(L(env.sunDir, normalize(vec3(-0.55f, 0.2f, -0.8f))));
    env.sunColor = L(env.sunColor, srgb(0xffb060));
    env.sunIntensity = Lf(env.sunIntensity, 2.5f);
    env.skyTop = L(env.skyTop, srgb(0x1f3f5c));
    env.skyHorizon = L(env.skyHorizon, srgb(0xf2a548));
    env.skyAmbient = L(env.skyAmbient, srgb(0x8a7a6a) * 0.85f);
    env.groundAmbient = L(env.groundAmbient, srgb(0x6a4a24) * 0.7f);
    env.fogColor = L(env.fogColor, srgb(0xd8955a));
    env.fogDensity = Lf(env.fogDensity, 0.009f);
    env.cloudCover = Lf(env.cloudCover, 0.25f);
    env.night = Lf(env.night, 0.f);
    env.aurora = Lf(env.aurora, 0.f);
    env.rainbow = Lf(env.rainbow, 0.f);
    env.wetness = Lf(env.wetness, 0.f);
    env.saturation = Lf(env.saturation, 0.92f);
    env.contrast = Lf(env.contrast, 1.16f);
    env.temperature = Lf(env.temperature, 0.3f);
    env.grain = Lf(env.grain, 0.035f);
    env.vignette = Lf(env.vignette, 0.5f);
    env.bloomIntensity = Lf(env.bloomIntensity, 0.11f);
    env.shafts = Lf(env.shafts, 0.8f);
    env.rift = Lf(env.rift, 1.f);
    env.riftFlash = std::max(env.riftFlash, 0.15f * k);
    if (amberShiftT >= 0) env.exposure *= 1.f + 0.8f * std::max(0.f, 1.f - amberShiftT / 0.5f);   // the white flash of the shift
}

void Game::drawAmberHud() {
    if (!amber && amberK < 0.02f) return;
    UI& u = ui;
    float W_ = u.W, a = saturate(amberK * 1.5f);
    if (!amber) return;
    // the eternity gauge: an amber band under the quest tracker
    Rect bar{W_ / 2 - 150, 66, 300, 14};
    u.rect({bar.x - 2, bar.y - 2, bar.w + 4, bar.h + 4}, vec4(0, 0, 0, 0.55f * a));
    float f = saturate(amberEternity / 100.f);
    vec4 col = f > 0.8f ? vec4(1.f, 0.45f + 0.2f * std::sin(realTime * 10.f), 0.2f, a) : vec4(1.f, 0.72f, 0.28f, a);
    u.rect({bar.x, bar.y, bar.w * f, bar.h}, col);
    u.text("Éternité", bar.x - 10, bar.y - 3, 17, rgba(0xf6dc9a, a), F_BOLD, A_RIGHT, 0, true);
    int found = 0; for (int i = 1; i <= 6; i++) if (loreFound.count("ambre_" + std::to_string(i))) found++;
    u.text("Souvenirs " + std::to_string(found) + "/6", bar.x + bar.w + 10, bar.y - 3, 17, rgba(0xf6dc9a, a), F_BOLD, A_LEFT, 0, true);
    Rect back{W_ / 2 - 95, 92, 190, 50};
    u.nine("button_dark", back, vec4(1.f, 0.85f, 0.6f, 0.92f * a), 0.7f);
    u.text("Revenir", back.x + back.w / 2, back.y + 13, 21, rgba(0xf6dc9a, a), F_SERIF, A_CENTER);
    u.tag(back, "Revenir");
    if (u.button(back)) exitAmber(false);
}

}  // namespace lk
