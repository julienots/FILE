// LOST KINGDOM — 1.11.1 goals: vehicle races on the real roads out of Brindlemere (start zone, countdown,
// checkpoints as columns of light, timer, medals, rewards, best times) and challenges that give the open world
// a direction (main goal + milestones rewarded automatically, listed in Journal > Objectifs).
#include "Game.h"
#include <algorithm>
#include <cmath>

namespace lk {

// ------------------------------------------------------------------------------------------ races
const std::vector<RaceDef>& raceDefs() {
    static const std::vector<RaceDef> r = {
        {"r_millbrook", "Sprint de Millbrook", "brindlemere", "millbrook"},
        {"r_fort", "Route du Fort Aldren", "brindlemere", "fort_aldren"},
        {"r_gravemoor", "La grande route de Gravemoor", "brindlemere", "gravemoor"},
    };
    return r;
}

// checkpoints every ~45 m along the road joining the two places, from the edge of the first one to the second
void Game::buildRaces() {
    races.clear();
    for (const RaceDef& d : raceDefs()) {
        RaceTrack t; t.def = (int)races.size();
        const Poi* a = world.poiById(d.from); const Poi* b = world.poiById(d.to);
        if (!a || !b) { races.push_back(t); continue; }
        const Road* best = nullptr; bool rev = false; float bd = 1e9f;
        for (const Road& r : world.roads) {
            if (r.pts.size() < 2) continue;
            float d1 = length(r.pts.front() - a->pos) + length(r.pts.back() - b->pos);
            float d2 = length(r.pts.back() - a->pos) + length(r.pts.front() - b->pos);
            if (d1 < bd) { bd = d1; best = &r; rev = false; }
            if (d2 < bd) { bd = d2; best = &r; rev = true; }
        }
        if (!best || bd > 200.f) { races.push_back(t); continue; }
        std::vector<vec2> pts = best->pts;
        if (rev) std::reverse(pts.begin(), pts.end());
        // walk the polyline
        float acc = 0, step = 45.f;
        bool started = false;
        for (size_t i = 0; i + 1 < pts.size(); i++) {
            vec2 p0 = pts[i], p1 = pts[i + 1];
            float L = length(p1 - p0);
            for (float s = 0; s < L; s += 1.f) {
                vec2 p = lerp(p0, p1, s / L);
                if (!started) {
                    if (length(p - a->pos) > a->radius + 12.f) { started = true; t.start = p; t.startDir = normalize(p1 - p0); acc = 0; }
                    continue;
                }
                acc += 1.f;
                if (length(p - b->pos) < b->radius * 0.8f) { t.cps.push_back(p); i = pts.size(); break; }
                if (acc >= step) { acc = 0; t.cps.push_back(p); }
            }
        }
        float len = 0; vec2 prev = t.start;
        for (vec2 c : t.cps) { len += length(c - prev); prev = c; }
        t.length = len;
        // medal times from the road length (the car tops out at 25 m/s; turns and slopes cost time)
        t.gold = len / 17.f; t.silver = len / 13.5f; t.bronze = len / 10.f;
        races.push_back(t);
    }
}

void Game::updateRaces(float dt) {
    if (races.empty() && world.poiById("brindlemere")) buildRaces();
    if (race.active < 0) {
        if (driving < 0) return;
        const VehicleState& v = vehicles[driving];
        for (int i = 0; i < (int)races.size(); i++) {
            const RaceTrack& t = races[i];
            if (t.cps.empty()) continue;
            if (length(v.pos.xz() - t.start) < 9.f) {
                if (std::fabs(v.speed) < 1.5f) { race.hold += dt; if (race.hold > 1.2f) { race = RaceState(); race.active = i; race.countdown = 3.f; play("ui_open", 0.8f); } }
                else { race.hold = 0; if (toastHint.empty()) toastHint = "Arrêtez-vous dans le cercle pour lancer : " + raceDefs()[t.def].name; }
                return;
            }
        }
        race.hold = 0;
        return;
    }
    RaceTrack& t = races[race.active];
    if (driving < 0) { notify("Course abandonnée.", vec3(1.f, 0.6f, 0.5f), "i_mount"); race = RaceState(); return; }
    VehicleState& v = vehicles[driving];
    if (race.countdown > 0) {
        float before = race.countdown;
        race.countdown -= dt;
        v.speed = 0;   // held on the line
        if (std::ceil(before) != std::ceil(race.countdown)) play(race.countdown > 0 ? "ui_click" : "levelup", 0.9f, race.countdown > 0 ? 1.f : 1.3f);
        return;
    }
    race.time += dt;
    if (race.next < (int)t.cps.size() && length(v.pos.xz() - t.cps[race.next]) < 11.f) {
        race.next++;
        play("pickup", 0.7f, 1.2f + 0.02f * race.next);
        particles.magicBurst(v.pos + vec3(0, 1.5f, 0), vec3(0.5f, 0.9f, 1.f), 1.2f, 20);
    }
    if (race.next >= (int)t.cps.size()) {
        // finish: medal, reward for each medal won the first time, best time
        const RaceDef& d = raceDefs()[t.def];
        int medal = race.time <= t.gold ? 3 : race.time <= t.silver ? 2 : race.time <= t.bronze ? 1 : 0;
        float& best = raceBest[d.id];
        bool record = best <= 0 || race.time < best;
        if (record) best = race.time;
        static const char* names[] = {"sans médaille", "bronze", "argent", "or"};
        static const int gold_[] = {0, 80, 150, 300};
        int reward = 0;
        for (int m = 1; m <= medal; m++) { std::string f = "race_" + d.id + "_" + std::to_string(m); if (!storyFlags.count(f)) { storyFlags.insert(f); reward += gold_[m]; } }
        gold += reward; if (reward) { gainXp(reward / 2); play("coin", 0.9f); }
        char b[200];
        snprintf(b, sizeof b, "%s : %d:%04.1f (%s)%s%s", d.name.c_str(), (int)(race.time / 60), std::fmod(race.time, 60.f), names[medal], record ? " · record !" : "", reward ? (" · +" + std::to_string(reward) + " or").c_str() : "");
        notify(b, medal == 3 ? vec3(1.f, 0.85f, 0.3f) : vec3(0.85f, 0.9f, 1.f), "i_star");
        raceResultT = 6.f; raceResult = b;
        if (medal == 3) particles.levelUp(v.pos);
        race = RaceState();
        return;
    }
    if (race.time > t.bronze * 1.6f) { notify("Trop lent : course abandonnée.", vec3(1.f, 0.6f, 0.5f), "i_mount"); race = RaceState(); }
}

// light columns at the start zones and the next checkpoint (particles + a point light)
void Game::raceFx(float dt, std::vector<std::pair<float, PointLight>>& lights) {
    if (dungeon.active) return;
    auto column = [&](vec2 p, vec3 col, float rate, float r) {
        float gy = groundAt(p.x, p.y);
        for (float k = particles.rng.f(); k < rate * dt; k += 1.f) {
            float a = particles.rng.range(0, TAU);
            Particle q; q.pos = vec3(p.x + std::cos(a) * r, gy + particles.rng.range(0.f, 0.4f), p.y + std::sin(a) * r);
            q.vel = vec3(0, particles.rng.range(4.f, 7.f), 0); q.maxLife = 1.2f; q.size0 = 0.35f; q.size1 = 0.08f;
            q.c0 = vec4(col * 1.6f, 0.9f); q.c1 = vec4(col, 0.f); q.frame = PF_SPARK; q.additive = true; q.stretch = 0.25f;
            particles.spawn(q);
        }
        PointLight pl; pl.pos = vec3(p.x, gy + 2.f, p.y); pl.radius = 9.f; pl.color = col * 2.5f; pl.flicker = 0.2f;
        lights.push_back({0.f, pl});
    };
    if (race.active >= 0) {
        const RaceTrack& t = races[race.active];
        if (race.next < (int)t.cps.size()) column(t.cps[race.next], race.next + 1 == (int)t.cps.size() ? vec3(1.f, 0.8f, 0.3f) : vec3(0.4f, 0.85f, 1.f), 90.f, 4.f);
        if (race.next + 1 < (int)t.cps.size()) column(t.cps[race.next + 1], vec3(0.3f, 0.6f, 0.9f), 25.f, 3.f);
    } else if (driving >= 0) {
        for (const RaceTrack& t : races) if (!t.cps.empty() && distXZ(vec3(t.start.x, 0, t.start.y), camFocus) < 120.f) column(t.start, vec3(0.5f, 1.f, 0.6f), 40.f, 8.f);
    }
}

void Game::drawRaceHud() {
    UI& u = ui;
    float W_ = u.W;
    if (raceResultT > 0) {
        raceResultT -= 1.f / 30.f;
        u.text(raceResult, W_ / 2, 150, 24, rgba(0xf4d58a), F_SERIF, A_CENTER, W_ * 0.7f, true);
    }
    if (race.active < 0) return;
    const RaceTrack& t = races[race.active];
    const RaceDef& d = raceDefs()[t.def];
    Rect p{W_ / 2 - 260, 84, 520, 92};
    u.nine("panel_plain", p, vec4(1, 1, 1, 0.9f), 0.6f);
    char b[160];
    if (race.countdown > 0) snprintf(b, sizeof b, "%d", (int)std::ceil(race.countdown));
    else snprintf(b, sizeof b, "%d:%04.1f", (int)(race.time / 60), std::fmod(race.time, 60.f));
    u.text(d.name, p.x + 16, p.y + 10, 18, rgba(0xb0a48c), F_SERIF);
    u.text(b, p.x + p.w / 2, p.y + 34, race.countdown > 0 ? 44 : 34, race.countdown > 0 ? rgba(0xf4d58a) : rgba(0xf4e7c4), F_BOLD, A_CENTER, 0, true);
    snprintf(b, sizeof b, "Point %d/%zu", std::min(race.next + 1, (int)t.cps.size()), t.cps.size());
    u.text(b, p.x + 16, p.y + 60, 17, rgba(0xc8bca4), F_BODY);
    snprintf(b, sizeof b, "Or %d:%02d · Argent %d:%02d · Bronze %d:%02d", (int)t.gold / 60, (int)t.gold % 60, (int)t.silver / 60, (int)t.silver % 60, (int)t.bronze / 60, (int)t.bronze % 60);
    u.text(b, p.x + p.w - 16, p.y + 60, 15, rgba(0xc8bca4), F_BODY, A_RIGHT);
}

// ------------------------------------------------------------------------------------------ challenges
const std::vector<ChallengeDef>& challenges() {
    static const std::vector<ChallengeDef> c = {
        {"c_level5", "Premiers pas", "Atteindre le niveau 5.", 5, 100},
        {"c_level15", "Vétéran", "Atteindre le niveau 15.", 15, 400},
        {"c_explore10", "Explorateur", "Découvrir 10 lieux.", 10, 150},
        {"c_explore25", "Cartographe", "Découvrir 25 lieux.", 25, 400},
        {"c_kills50", "Chasseur", "Vaincre 50 ennemis.", 50, 150},
        {"c_kills250", "Fléau des monstres", "Vaincre 250 ennemis.", 250, 500},
        {"c_boss1", "Tueur de Gardien", "Vaincre un Gardien ou un boss.", 1, 300},
        {"c_shards3", "La moitié d'une couronne", "Réunir 3 fragments de la Couronne.", 3, 500},
        {"c_domain", "Seigneur", "Fonder votre domaine.", 1, 100},
        {"c_build6", "Bâtisseur", "Construire 6 bâtiments au domaine.", 6, 250},
        {"c_people6", "Chef de village", "Accueillir 6 habitants au domaine.", 6, 250},
        {"c_guard3", "Capitaine", "Recruter 3 soldats.", 3, 250},
        {"c_herd4", "Éleveur", "Élever 4 animaux au domaine.", 4, 150},
        {"c_nights3", "Survivant", "Survivre à 3 nuits de traque.", 3, 300},
        {"c_race", "Pilote", "Terminer une course.", 1, 80},
        {"c_racegold", "As du volant", "Décrocher l'or dans une course.", 1, 300},
        {"c_heroes", "La roue complète", "Débloquer les trois héros.", 3, 400},
        {"c_lore15", "Chroniqueur", "Trouver 15 pages de Chroniques.", 15, 200},
        {"c_quests10", "Au service du royaume", "Terminer 10 quêtes.", 10, 300},
    };
    return c;
}

int Game::challengeProgress(const ChallengeDef& c) const {
    const std::string& id = c.id;
    int kills = 0; for (auto& b : bestiary) kills += b.second;
    if (id == "c_level5" || id == "c_level15") return level;
    if (id == "c_explore10" || id == "c_explore25") return (int)discovered.size();
    if (id == "c_kills50" || id == "c_kills250") return kills;
    if (id == "c_boss1") return (int)defeatedBosses.size();
    if (id == "c_shards3") return countItem("crown_shard");
    if (id == "c_domain") return base.founded ? 1 : 0;
    if (id == "c_build6") return (int)base.built.size();
    if (id == "c_people6") return base.residents;
    if (id == "c_guard3") return base.soldiers;
    if (id == "c_herd4") return base.pigs + base.horses;
    if (id == "c_nights3") return huntsSurvived;
    if (id == "c_race") return (int)raceBest.size();
    if (id == "c_racegold") { int n = 0; for (auto& d : raceDefs()) if (storyFlags.count("race_" + d.id + "_3")) n++; return n; }
    if (id == "c_heroes") return (int)unlockedHeroes.size() + 1;
    if (id == "c_lore15") return (int)loreFound.size();
    if (id == "c_quests10") return (int)questsDone.size();
    return 0;
}

void Game::updateChallenges(float dt) {
    challengeT -= dt;
    if (challengeT > 0) return;
    challengeT = 1.f;
    for (const ChallengeDef& c : challenges()) {
        std::string f = "ch_" + c.id;
        if (storyFlags.count(f) || challengeProgress(c) < c.goal) continue;
        storyFlags.insert(f);
        gold += c.gold; gainXp(c.gold / 2);
        play("quest", 0.8f, 1.1f);
        notify("Défi accompli : " + c.name + " (+" + std::to_string(c.gold) + " or)", vec3(1.f, 0.85f, 0.35f), "i_star");
        return;   // one at a time
    }
}

void Game::drawObjectives(Rect c) {
    UI& u = ui;
    const vec4 GOLDT = rgba(0xf4d58a), DIM = rgba(0xb0a48c), PALE = rgba(0xf4e7c4);
    // the main goal of the game
    u.text("Le but", c.x, c.y - 4, 26, GOLDT, F_TITLE);
    int shards = countItem("crown_shard");
    std::string main = "Retrouvez les six fragments de la Couronne-Mère, gardés par les six Gardiens corrompus, et découvrez qui vous êtes vraiment.";
    u.text(main, c.x, c.y + 34, 18, PALE, F_BODY, A_LEFT, c.w * 0.46f);
    char b[200];
    snprintf(b, sizeof b, "Fragments : %d / 6", shards);
    u.text(b, c.x, c.y + 104, 20, GOLDT, F_BOLD);
    for (int i = 0; i < 6; i++) u.circle(vec2(c.x + 200 + i * 30, c.y + 116), 10, i < shards ? vec4(1.f, 0.82f, 0.35f, 1) : vec4(0.3f, 0.27f, 0.22f, 1), 14);
    std::string next = "—";
    for (auto& q : questLog) if (!q.done && quests()[q.idx].type == QuestType::Main) { const QuestDef& d = quests()[q.idx]; next = d.title + " : " + (q.step < (int)d.steps.size() ? d.steps[q.step].desc : ""); break; }
    u.text("Prochaine étape : " + next, c.x, c.y + 140, 17, DIM, F_BODY, A_LEFT, c.w * 0.46f);
    float y = c.y + 200;
    u.text("À faire aussi", c.x, y, 22, GOLDT, F_SERIF); y += 34;
    static const char* ideas[] = {
        "· Fonder et agrandir votre domaine (onglet Domaine).",
        "· Courses : la voiture et la moto sont garées à l'est de Brindlemere ; arrêtez-vous dans un cercle vert pour lancer une course.",
        "· Survivre aux traqueurs de la nuit, ou rentrer à temps dans un village.",
        "· Quêtes des villages, contrats de chasse, donjons et Gardiens.",
        "· Débloquer Kaede et Astra dans la roue des héros."};
    for (const char* s : ideas) { u.text(s, c.x, y, 16, DIM, F_BODY, A_LEFT, c.w * 0.46f); y += u.textHeight(s, 16, F_BODY, c.w * 0.46f) + 6; }
    // challenges
    Rect R{c.x + c.w * 0.5f, c.y - 4, c.w * 0.5f, c.h + 4};
    int done = 0; for (auto& ch : challenges()) if (storyFlags.count("ch_" + ch.id)) done++;
    snprintf(b, sizeof b, "Défis · %d / %zu", done, challenges().size());
    u.text(b, R.x, R.y, 22, GOLDT, F_SERIF);
    Rect L{R.x, R.y + 36, R.w, R.h - 36};
    u.pushClip(L);
    float rowH = 58, so = u.scrollArea("challenges", L, challenges().size() * rowH);
    for (int i = 0; i < (int)challenges().size(); i++) {
        const ChallengeDef& ch = challenges()[i];
        bool ok = storyFlags.count("ch_" + ch.id) > 0;
        int pr = std::min(challengeProgress(ch), ch.goal);
        Rect r{L.x, L.y + i * rowH - so, L.w - 8, rowH - 6};
        u.nine("panel_plain", r, vec4(1, 1, 1, ok ? 0.55f : 0.85f), 0.5f);
        if (ok) u.sprite("i_check", {r.x + 8, r.y + 12, 28, 28});
        u.text(ch.name, r.x + 44, r.y + 6, 18, ok ? rgba(0x9ae08a) : PALE, F_BOLD);
        u.text(ch.desc, r.x + 44, r.y + 30, 15, DIM, F_BODY, A_LEFT, r.w - 200);
        snprintf(b, sizeof b, "%d/%d · %d or", pr, ch.goal, ch.gold);
        u.text(b, r.x + r.w - 12, r.y + 18, 15, ok ? rgba(0x9ae08a) : GOLDT, F_BOLD, A_RIGHT);
        u.rect({r.x + 44, r.y + r.h - 6, (r.w - 56) * pr / std::max(1, ch.goal), 3}, ok ? vec4(0.6f, 0.9f, 0.5f, 0.9f) : vec4(1.f, 0.8f, 0.35f, 0.9f));
    }
    u.popClip();
}

}  // namespace lk
