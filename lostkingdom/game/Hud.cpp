#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>

namespace lk {

const char* CTL_ICON[CTL_COUNT] = {"attack", "dodge", "jump", "block", "", "", "", "potion", "interact", "mount"};

Rect ctlRect(const Game& g, int id) {
    const ControlLayout& L = g.settings.layout;
    vec2 p = L.pos[id];
    float s = ctlSize(id) * L.scale;
    float x = L.leftHanded ? -p.x : g.ui.W + p.x;
    return {x - s / 2, g.ui.H + p.y - s / 2, s, s};
}

// ============================================================================================ input
// Contextual buttons are hidden (and not touchable) when they have nothing to do.
bool Game::ctlVisible(int id) const {
    if (id == CTL_INTERACT) return interactTarget != NullEntity || !dungeonPrompt.empty() || fishingT >= 0;
    if (id >= CTL_SKILL1 && id <= CTL_SKILL3) return equippedSpells[id - CTL_SKILL1] >= 0;
    if (id == CTL_MOUNT) return !ownedMounts.empty() || riding;
    return true;
}

void Game::updateTouchControls() {
    if (state != GameState::Playing || layoutEdit) return;
    bool modal = menu != Menu::None || dialog.open || cinematicT > 0 || transitionT >= 0;
    if (modal) { joyFinger = -1; camFinger = -1; return; }
    UI& u = ui;
    // ---- buttons
    // A finger belongs to the button it STARTED on (finger.sx/sy), and to only one button: overlapping or
    // adjacent buttons can never fire together. Quick taps whose down+up happen within one frame
    // (common at 30 FPS) are still seen: such a finger has pressed == true and down == false.
    auto hit = [&](int id, const UI::Finger& f) {
        if (!ctlVisible(id)) return false;
        Rect r = ctlRect(*this, id);
        // circular hit area slightly larger than the drawn button (thumbs are imprecise)
        vec2 c{r.x + r.w / 2, r.y + r.h / 2};
        return length(vec2(f.sx, f.sy) - c) < r.w * 0.5f + 10.f;
    };
    // resolve new fingers: nearest visible button wins
    for (auto& f : u.fingers) {
        if (!f.pressed || f.consumed) continue;
        int best = -1; float bd = 1e9f;
        for (int id = 0; id < CTL_COUNT; id++) {
            if (!hit(id, f)) continue;
            Rect r = ctlRect(*this, id);
            float d = length(vec2(f.sx, f.sy) - vec2(r.x + r.w / 2, r.y + r.h / 2)) / r.w;
            if (d < bd) { bd = d; best = id; }
        }
        if (best < 0) continue;
        f.consumed = true;
        btnFlash[best] = 1.f;
        haptic(8, 50);
        switch (best) {
        case CTL_ATTACK: attackFinger = f.id; break;
        case CTL_DODGE: dodgeFinger = f.id; input.dodge = true; break;
        case CTL_BLOCK: blockFinger = f.id; break;
        case CTL_JUMP: input.jump = true; break;
        case CTL_SKILL1: case CTL_SKILL2: case CTL_SKILL3: input.skill[best - CTL_SKILL1] = true; break;
        case CTL_POTION: input.potion = true; break;
        case CTL_INTERACT: input.interact = true; break;
        case CTL_MOUNT: input.mount = true; break;
        }
    }
    auto finger = [&](int id) -> UI::Finger* { if (id < 0) return nullptr; for (auto& f : u.fingers) if (f.id == id) return &f; return nullptr; };
    // attack: tap = light attack, hold = charge, release = heavy / charged
    if (UI::Finger* f = finger(attackFinger)) {
        f->consumed = true;
        btnHold[CTL_ATTACK] = f->t;
        if (f->down) { input.attackHeld = true; input.attackHoldT = f->t; }
        else { input.attackRelease = true; input.attackHoldT = f->t; attackFinger = -1; btnHold[CTL_ATTACK] = 0; }
    } else { attackFinger = -1; btnHold[CTL_ATTACK] = 0; }
    // dodge: tap = roll, hold = sprint
    if (UI::Finger* f = finger(dodgeFinger)) {
        f->consumed = true;
        if (f->down && f->t > 0.3f) input.sprint = true;
        if (!f->down) dodgeFinger = -1;
    } else dodgeFinger = -1;
    // block: held while the finger stays down (a quick tap still opens the parry window for one frame)
    if (UI::Finger* f = finger(blockFinger)) {
        f->consumed = true;
        input.block = true;
        if (!f->down) blockFinger = -1;
    } else blockFinger = -1;
    // top-right shortcuts
    Rect bMenu{u.W - 70, 14, 56, 56}, bMap{u.W - 132, 14, 56, 56}, bBag{u.W - 194, 14, 56, 56};
    Rect mini{u.W - 196, 80, 176, 176};
    // ---- joystick (left 45%) and camera (right side)
    for (auto& f : u.fingers) {
        if (f.consumed) continue;
        if (bMenu.contains(f.sx, f.sy) || bMap.contains(f.sx, f.sy) || bBag.contains(f.sx, f.sy) || mini.contains(f.sx, f.sy)) continue;
        bool left = settings.layout.leftHanded ? f.sx > u.W * 0.55f : f.sx < u.W * 0.45f;
        if (f.pressed && left && joyFinger < 0) { joyFinger = f.id; joyOrigin = vec2(f.x, f.y); }
        else if (f.pressed && !left) {
            if (camFinger < 0) camFinger = f.id;
        }
    }
    bool joyAlive = false, camAlive = false;
    std::vector<UI::Finger*> camFingers;
    for (auto& f : u.fingers) {
        if (f.id == joyFinger && f.down) {
            joyAlive = true;
            joyPos = vec2(f.x, f.y);
            vec2 d = joyPos - joyOrigin;
            float R = 70.f * settings.layout.scale;
            if (length(d) > R * 1.4f) joyOrigin = joyPos - normalize(d) * R * 1.4f;  // follow the thumb
            vec2 m = d / R;
            if (length(m) > 1) m = normalize(m);
            if (length(m) < settings.joystickDeadzone) m = vec2(0, 0);
            input.move = vec2(m.x, -m.y);
            if (length(m) > 0.98f && f.t > 1.5f) input.sprint = true;
        }
        if (!f.consumed && f.id != joyFinger && f.down) camFingers.push_back(&f);
        if (f.id == camFinger) camAlive = f.down || f.released;
    }
    if (!joyAlive) joyFinger = -1;
    // camera rotation (one finger drag) and pinch zoom (two fingers)
    static std::map<int, vec2> lastPos;
    if (camFingers.size() >= 2) {
        float dd = length(vec2(camFingers[0]->x, camFingers[0]->y) - vec2(camFingers[1]->x, camFingers[1]->y));
        if (pinchDist > 0) input.pinch += (dd - pinchDist);
        pinchDist = dd;
    } else pinchDist = 0;
    for (auto* f : camFingers) {
        auto it = lastPos.find(f->id);
        if (it != lastPos.end() && camFingers.size() == 1) input.camDrag.x += -(f->x - it->second.x);
        lastPos[f->id] = vec2(f->x, f->y);
    }
    for (auto it = lastPos.begin(); it != lastPos.end();) { bool alive = false; for (auto& f : u.fingers) if (f.id == it->first && f.down) alive = true; if (!alive) it = lastPos.erase(it); else ++it; }
    // tap on an enemy to lock on / tap empty ground to release
    for (auto& f : u.fingers) {
        if (!f.released || f.consumed || f.t > 0.3f || length(vec2(f.x - f.sx, f.y - f.sy)) > 12) continue;
        if (f.id == joyFinger) continue;
        Entity best = NullEntity; float bd = 70.f;
        reg.each<Character>([&](Entity e, Character& c) {
            if (c.team != Team::Enemy || c.dead) return;
            bool on;
            vec2 sp = worldToScreen(c.pos + vec3(0, c.height * 0.5f, 0), &on);
            float d = length(sp - vec2(f.x, f.y));
            if (on && d < bd) { bd = d; best = e; }
        });
        lockTarget = best;
        if (best != NullEntity) { play("ui_click", 0.4f); haptic(10, 60); }
    }
    if (!camAlive) camFinger = -1;
    if (u.button(bMenu)) { menu = Menu::Pause; play("ui_open", 0.6f); }
    if (u.button(bMap) || u.button(mini)) { menu = Menu::Map; play("ui_open", 0.6f); mapPan = vec2(0, 0); }
    if (u.button(bBag)) { menu = Menu::Inventory; play("ui_open", 0.6f); }
}

// ============================================================================================ HUD
static void drawStatusIcons(UI& u, const Character& c, float x, float y) {
    struct S { float v; const char* icon; vec4 col; };
    S st[] = {{c.status.burn, "i_sp_fireball", vec4(1, 1, 1, 1)}, {c.status.poison, "i_sp_roots", vec4(1, 1, 1, 1)}, {c.status.freeze + c.status.slow, "i_sp_frostnova", vec4(1, 1, 1, 1)},
              {c.status.stun, "i_sp_thunder", vec4(1, 1, 1, 1)}, {c.status.shield > 0 ? 1.f : 0.f, "i_sp_radiantshield", vec4(1, 1, 1, 1)}, {c.status.regen, "i_sp_regrowth", vec4(1, 1, 1, 1)},
              {c.status.strength, "i_attack", vec4(1, 0.8f, 0.5f, 1)}, {c.status.wellFed, "i_heart", vec4(1, 1, 1, 1)}, {c.status.frostRes, "i_potion_mana", vec4(1, 1, 1, 1)}};
    for (auto& s : st) if (s.v > 0) { u.sprite(s.icon, {x, y, 30, 30}, s.col); x += 34; }
}

void Game::drawMinimap(Rect r) {
    UI& u = ui;
    Character& p = pc();
    vec2 c{r.x + r.w / 2, r.y + r.h / 2};
    float rad = r.w / 2 - 14;
    if (!mapTex && world.mapReady) {
        glGenTextures(1, &mapTex);
        glBindTexture(GL_TEXTURE_2D, mapTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, world.mapRes, world.mapRes, 0, GL_RGBA, GL_UNSIGNED_BYTE, world.mapImage.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    u.circle(c, rad + 4, vec4(0.06f, 0.05f, 0.04f, 0.9f), 40);
    const float viewM = 140.f;  // metres shown from centre to edge
    // Minimap convention: camera forward (-sin camYaw, -cos camYaw) points up, camera right points right.
    // World offset d (x, z) -> screen offset rotate(d, camYaw). The map image uses the inverse rotation.
    float ang = camYaw + PI;
    if (dungeon.active) {
        // dungeon minimap: tiles
        u.circle(c, rad, vec4(0.12f, 0.1f, 0.12f, 0.95f), 40);
        float s = rad / 40.f;
        for (int z = 0; z < dungeon.H; z++) for (int x = 0; x < dungeon.W; x++) {
            TileType t = dungeon.tiles[z * dungeon.W + x];
            if (t == TileType::Void) continue;
            vec2 wpos = vec2(dungeon.origin.x + (x + 0.5f) * 4.f, dungeon.origin.z + (z + 0.5f) * 4.f) - p.pos.xz();
            vec2 sp = minimapProject(wpos, camYaw) * s;
            if (length(sp) > rad - 4) continue;
            u.rect({c.x + sp.x - 2 * s, c.y + sp.y - 2 * s, 4 * s, 4 * s}, t == TileType::Spikes ? vec4(0.6f, 0.3f, 0.2f, 0.9f) : vec4(0.55f, 0.5f, 0.45f, 0.9f));
        }
    } else if (mapTex) {
        vec2 uc = p.pos.xz() / WORLD_SIZE;
        u.imageCircle(mapTex, c, rad, uc, viewM / WORLD_SIZE, -ang + PI, vec4(1, 1, 1, 1));
    }
    auto toMini = [&](vec3 wp, bool clampEdge, bool* inside) {
        vec2 d = wp.xz() - p.pos.xz();
        vec2 sp = minimapProject(d, camYaw) * (rad / viewM);
        float l = length(sp);
        *inside = l < rad - 8;
        if (clampEdge && l > rad - 10) sp = sp * ((rad - 10) / l);
        return c + sp;
    };
    bool in;
    if (!dungeon.active) {
        for (auto& P : world.pois) {
            if (!discovered.count(P.id)) continue;
            vec2 m = toMini(vec3(P.pos.x, 0, P.pos.y), false, &in);
            if (!in) continue;
            const char* ic = P.type == PoiType::Village || P.type == PoiType::Mill ? "i_village" : P.type == PoiType::Castle ? "i_castle" : (P.type == PoiType::Cave || P.type == PoiType::Mine || P.type == PoiType::Graveyard) ? "i_cave" : P.type == PoiType::Waystone ? "i_waypoint" : P.type == PoiType::BossArena ? "i_boss" : P.type == PoiType::Monastery || P.type == PoiType::Tower ? "i_tower" : "i_star";
            u.sprite(ic, {m.x - 11, m.y - 11, 22, 22}, vec4(1, 1, 1, 0.95f));
        }
    }
    reg.each<Character>([&](Entity e, Character& o) {
        if (o.dead || e == player) return;
        vec2 m = toMini(o.pos, false, &in);
        if (!in) return;
        if (o.team == Team::Enemy) u.circle(m, o.bossDef >= 0 ? 6.f : 3.5f, vec4(0.95f, 0.25f, 0.2f, 1), 10);
        else if (o.npcDef >= 0) u.circle(m, 3.5f, vec4(1.f, 0.85f, 0.4f, 1), 10);
        else if (o.team == Team::Mount) u.sprite("i_mount", {m.x - 9, m.y - 9, 18, 18});
    });
    bool hasMarker;
    vec3 qm = questMarker(&hasMarker);
    if (hasMarker && !dungeon.active) {
        vec2 m = toMini(qm, true, &in);
        float pulse = 1.f + 0.15f * std::sin(realTime * 5);
        u.sprite("i_quest_marker", {m.x - 12 * pulse, m.y - 16 * pulse, 24 * pulse, 32 * pulse});
    }
    // arrow sprite points up; heading (sin yaw, cos yaw) -> screen rotate(heading, camYaw)
    u.spriteRot("i_player_arrow", c, vec2(24, 24), minimapArrowAngle(p.yaw, camYaw), vec4(1, 1, 1, 1));
    u.sprite("minimap_frame", r);
    // north indicator
    // north indicator (world -z) on the rim
    {
        vec2 n = rotate(vec2(0, -1), camYaw);
        vec2 np = c + n * (rad - 2);
        u.circle(np, 11, vec4(0.08f, 0.06f, 0.05f, 0.9f), 16);
        u.text("N", np.x, np.y - 9, 16, rgba(0xf4d080), F_BOLD, A_CENTER, 0, false);
    }
    // clock
    char b[32]; snprintf(b, sizeof b, "%02d:%02d", (int)timeOfDay, (int)(std::fmod(timeOfDay, 1.f) * 60));
    u.sprite(env.night > 0.5f ? "i_night" : "i_day", {r.x + 4, r.y + r.h - 34, 28, 28});
    u.text(b, r.x + 36, r.y + r.h - 30, 18, rgba(0xf4e7c4), F_BOLD, A_LEFT, 0, true);
}

void Game::drawHud(float dt) {
    if (player == NullEntity) return;
    UI& u = ui;
    Character& p = pc();
    float W_ = u.W, H_ = u.H;
    // ---- cinematic letterbox
    float lb = cinematicT > 0 ? std::min(1.f, cinematicT * 2.f) : 0.f;
    if (lb > 0) {
        u.rect({0, 0, W_, 70 * lb}, vec4(0, 0, 0, 1));
        u.rect({0, H_ - 70 * lb, W_, 70 * lb}, vec4(0, 0, 0, 1));
        if (!cinematicText.empty()) u.text(cinematicText, W_ / 2, H_ - 58, 24, rgba(0xf4e7c4, lb), F_BODY, A_CENTER, W_ * 0.75f, true);
        if (bossEnt != NullEntity) {
            Character* b = reg.get<Character>(bossEnt);
            if (b) {
                const BossDef& bd = bosses()[b->bossDef];
                float a = saturate((4.f - cinematicT) * 1.5f) * lb;
                u.text(bd.name, W_ / 2, H_ * 0.28f, 54, rgba(0xf6dc9a, a), F_TITLE, A_CENTER, 0, true);
                u.text(bd.title, W_ / 2, H_ * 0.28f + 66, 24, rgba(0xd8c8a8, a), F_SERIF, A_CENTER, 0, true);
            }
        }
        return;
    }
    // ---- name tags, health bars, quest markers above characters
    reg.each<Character>([&](Entity e, Character& c) {
        if (e == player || c.dead || !c.vis.visible) return;
        if (c.npcDef >= 0) { NpcBrain* nb = reg.get<NpcBrain>(e); if (nb && nb->indoors) return; }
        float d = distXZ(c.pos, p.pos);
        if (d > 26) return;
        bool on;
        vec2 sp = worldToScreen(c.pos + vec3(0, c.height + 0.45f * c.vis.scale, 0), &on);
        if (!on || sp.x < -50 || sp.x > W_ + 50) return;
        float a = saturate((26 - d) / 6);
        if (c.team == Team::Enemy && c.bossDef < 0 && (c.hp < c.maxHp || e == lockTarget)) {
            Rect br{sp.x - 42, sp.y, 84, 12};
            u.rect({br.x - 1, br.y - 1, br.w + 2, br.h + 2}, vec4(0, 0, 0, 0.6f * a));
            u.rect({br.x, br.y, br.w * saturate(c.hp / c.maxHp), br.h}, vec4(c.elite ? 0.95f : 0.85f, c.elite ? 0.55f : 0.18f, 0.12f, a));
            if (e == lockTarget || c.elite) u.text(c.name + " · niv. " + std::to_string(c.level), sp.x, sp.y - 24, 17, rgba(c.elite ? 0xf2a03a : 0xf0e0d0, a), F_BOLD, A_CENTER, 0, true);
        }
        if (c.npcDef >= 0) {
            const NpcDef& n = npcs()[c.npcDef];
            if (d < 12) u.text(n.name, sp.x, sp.y - 6, 18, rgba(0xf8ecd0, a), F_BOLD, A_CENTER, 0, true);
            // quest available / turn-in marker
            bool offer = false, turnin = false;
            for (auto& q : quests()) if (q.giver == n.id && !questActive(q.id) && !questsDone.count(q.id) && (q.requires_.empty() || questsDone.count(q.requires_))) offer = true;
            for (auto& s : questLog) { if (s.done) continue; const QuestDef& q = quests()[s.idx]; if (s.step < (int)q.steps.size() && (q.steps[s.step].target == n.id || (q.steps[s.step].kind == StepKind::Deliver && q.giver == n.id))) turnin = true; }
            float bob = std::sin(realTime * 3) * 4;
            if (turnin) u.text("?", sp.x, sp.y - 52 + bob, 40, rgba(0x9ae0ff, a), F_TITLE, A_CENTER, 0, true);
            else if (offer) u.text("!", sp.x, sp.y - 52 + bob, 40, rgba(0xf8d050, a), F_TITLE, A_CENTER, 0, true);
        }
    });
    // floating combat text
    for (auto& f : floatTexts) {
        bool on;
        vec2 sp = worldToScreen(f.pos, &on);
        if (!on) continue;
        float a = 1.f - saturate((f.t - 0.7f) / 0.5f);
        float s = f.size * (1.f + 0.4f * std::max(0.f, 0.15f - f.t) / 0.15f);
        u.text(f.text, sp.x, sp.y, s, vec4(f.color, a), F_BOLD, A_CENTER, 0, true);
    }
    // ---- player frame (top-left)
    u.sprite("round", {14, 12, 92, 92});
    u.sprite("i_character", {34, 30, 52, 52}, rgba(0xf4e7c4));
    u.circle({96, 92}, 17, rgba(0x2a1e16), 16);
    u.ring({96, 92}, 15, 18, 0, TAU, rgba(0xc9a45c), 20);
    u.text(std::to_string(level), 96, 80, 20, rgba(0xf6dc9a), F_BOLD, A_CENTER, 0, false);
    static float ghostHp = 1;
    float hpf = p.hp / p.maxHp;
    ghostHp = hpf < ghostHp ? damp(ghostHp, hpf, 2.5f, dt) : hpf;
    u.bar({108, 20, 300, 30}, hpf, vec4(0.85f, 0.18f, 0.14f, 1), ghostHp, vec4(1.f, 0.85f, 0.6f, 0.6f));
    char b[64]; snprintf(b, sizeof b, "%d / %d", (int)std::ceil(p.hp), (int)p.maxHp);
    u.text(b, 258, 24, 17, rgba(0xfff4e0), F_BOLD, A_CENTER, 0, true);
    if (p.status.shield > 0) u.bar({108, 20, 300 * saturate(p.status.shield / p.maxHp), 30}, 1.f, vec4(1.f, 0.9f, 0.5f, 0.6f));
    u.bar({108, 50, 240, 22}, mp / maxMp, vec4(0.2f, 0.45f, 0.95f, 1));
    u.bar({108, 72, 220, 20}, stamina / maxStamina, stamina < maxStamina * 0.25f ? vec4(0.95f, 0.6f, 0.2f, 1) : vec4(0.55f, 0.85f, 0.3f, 1));
    drawStatusIcons(u, p, 112, 96);
    // xp line
    float xpN = (float)xp / std::max(1.f, 100.f * std::pow((float)level, 1.55f));
    u.rect({0, H_ - 5, W_, 5}, vec4(0, 0, 0, 0.5f));
    u.rect({0, H_ - 5, W_ * saturate(xpN), 5}, rgba(0xc9a45c, 0.9f));
    // gold
    u.sprite("i_coin", {16, 134, 28, 28});
    u.text(std::to_string(gold), 50, 136, 20, rgba(0xf6dc9a), F_BOLD, A_LEFT, 0, true);
    if (skillPoints > 0) { u.sprite("i_skills", {100, 134, 28, 28}, rgba(0xf8d050, 0.7f + 0.3f * std::sin(realTime * 4))); u.text(std::to_string(skillPoints), 132, 136, 20, rgba(0xf8d050), F_BOLD, A_LEFT, 0, true); }

    // ---- top-right: shortcuts & minimap
    u.iconButton({W_ - 70, 14, 56, 56}, "menu");
    u.iconButton({W_ - 132, 14, 56, 56}, "map");
    u.iconButton({W_ - 194, 14, 56, 56}, "inventory");
    drawMinimap({W_ - 196, 80, 176, 176});

    // ---- quest tracker (top centre)
    std::string obj = questObjectiveText();
    if (!obj.empty() && trackedQuest >= 0) {
        const QuestDef& q = quests()[questLog[trackedQuest].idx];
        float w = std::min(520.f, std::max(u.textWidth(obj, 19) + 90, u.textWidth(q.title, 20, F_SERIF) + 90));
        Rect r{W_ / 2 - w / 2, 10, w, 66};
        u.nine("panel_plain", r, vec4(1, 1, 1, 0.82f), 0.6f);
        u.text(q.title, r.x + 52, r.y + 8, 19, rgba(q.type == QuestType::Main ? 0xf6dc9a : 0xe8dcc0), F_SERIF, A_LEFT, 0, false);
        u.text(obj, r.x + 52, r.y + 34, 18, rgba(0xd8ccb4), F_BODY, A_LEFT, 0, false);
        bool has;
        vec3 m = questMarker(&has);
        if (has && !dungeon.active) {
            float bearing = std::atan2(m.x - p.pos.x, m.z - p.pos.z);
            u.spriteRot("i_player_arrow", {r.x + 26, r.y + 33}, vec2(28, 28), minimapArrowAngle(bearing, camYaw), rgba(0xf8d050));
            char db[32]; snprintf(db, sizeof db, "%dm", (int)distXZ(m, p.pos));
            u.text(db, r.x + r.w - 12, r.y + 8, 15, rgba(0xb8ac94), F_BOLD, A_RIGHT, 0, false);
            // on-screen quest beacon
            bool on;
            vec2 sp = worldToScreen(m + vec3(0, 6, 0), &on);
            if (on && sp.x > 0 && sp.x < W_ && sp.y > 0 && sp.y < H_ && distXZ(m, p.pos) > 15) u.sprite("i_quest_marker", {sp.x - 12, sp.y - 30, 24, 32}, vec4(1, 1, 1, 0.85f));
        }
    }
    // ---- boss bar
    if (bossEnt != NullEntity && reg.valid(bossEnt)) {
        Character* bc = reg.get<Character>(bossEnt);
        if (bc && !bc->dead) {
            float w = std::min(700.f, W_ * 0.6f);
            Rect r{W_ / 2 - w / 2, 92, w, 34};
            u.text(bc->name, W_ / 2, r.y - 4 - 22, 24, rgba(0xf6dc9a), F_SERIF, A_CENTER, 0, true);
            static float ghost = 1;
            float f = bc->hp / bc->maxHp;
            ghost = f < ghost ? damp(ghost, f, 1.8f, dt) : f;
            u.bar(r, f, vec4(0.75f, 0.1f, 0.12f, 1), ghost, vec4(1, 0.8f, 0.5f, 0.6f));
            for (float th : bosses()[bc->bossDef].phases) u.rect({r.x + 8 + (r.w - 16) * th - 1, r.y + 6, 3, r.h - 12}, rgba(0xf6dc9a, 0.8f));
        }
    }
    // ---- region banner
    if (regionBannerT > 0) {
        float a = std::min(1.f, regionBannerT) * std::min(1.f, (5.f - regionBannerT) * 2.f);
        u.sprite("divider", {W_ / 2 - 230, H_ * 0.3f + 62, 460, 22}, vec4(1, 1, 1, a));
        u.text(regionBanner, W_ / 2, H_ * 0.3f, 56, rgba(0xf6e2b0, a), F_TITLE, A_CENTER, 0, true);
        u.text(regionBannerSub, W_ / 2, H_ * 0.3f + 92, 24, rgba(0xe0d4bc, a), F_SERIF, A_CENTER, 0, true);
    }
    if (levelUpT > 0) {
        float a = std::min(1.f, levelUpT);
        u.text("NIVEAU " + std::to_string(level), W_ / 2, H_ * 0.2f, 50, rgba(0xf8d050, a), F_TITLE, A_CENTER, 0, true);
    }
    // ---- touch controls
    float op = settings.layout.opacity;
    // joystick
    vec2 jc = joyFinger >= 0 ? joyOrigin : vec2(settings.layout.leftHanded ? W_ - 170 : 170, H_ - 170);
    float R = 70.f * settings.layout.scale;
    u.sprite("joy_base", {jc.x - R * 1.35f, jc.y - R * 1.35f, R * 2.7f, R * 2.7f}, vec4(1, 1, 1, joyFinger >= 0 ? op : op * 0.45f));
    vec2 kp = joyFinger >= 0 ? jc + vec2(input.move.x, -input.move.y) * R : jc;
    u.sprite("joy_knob", {kp.x - R * 0.55f, kp.y - R * 0.55f, R * 1.1f, R * 1.1f}, vec4(1, 1, 1, op));
    auto ctl = [&](int id, const std::string& icon, bool enabled, float cd01, const std::string& badge, vec4 tint, const char* base) {
        if (!ctlVisible(id)) return;
        Rect r = ctlRect(*this, id);
        bool held = btnFlash[id] > 0 || (id == CTL_ATTACK && attackFinger >= 0) || (id == CTL_DODGE && dodgeFinger >= 0) || (id == CTL_BLOCK && blockFinger >= 0);
        btnFlash[id] = std::max(0.f, btnFlash[id] - dt * 5.f);
        float s = held ? 0.92f : 1.f;
        Rect rr{r.x + r.w * (1 - s) / 2, r.y + r.h * (1 - s) / 2, r.w * s, r.h * s};
        u.sprite(held ? std::string(base) + "_hi" : base, rr, vec4(1, 1, 1, enabled ? op : op * 0.5f));
        Rect ir = rr.inset(rr.w * (icon.rfind("sp_", 0) == 0 ? 0.12f : 0.25f));
        u.sprite("i_" + icon, ir, enabled ? tint : vec4(0.5f, 0.5f, 0.5f, 0.6f));
        if (cd01 > 0) u.ring({rr.x + rr.w / 2, rr.y + rr.h / 2}, 0, rr.w * 0.42f, -PI / 2, -PI / 2 + TAU * cd01, vec4(0, 0, 0, 0.6f), 28);
        if (!badge.empty()) { u.circle({rr.x + rr.w * 0.82f, rr.y + rr.h * 0.18f}, 13, rgba(0x2a1e16), 14); u.text(badge, rr.x + rr.w * 0.82f, rr.y + rr.h * 0.18f - 11, 16, rgba(0xf6dc9a), F_BOLD, A_CENTER); }
    };
    ctl(CTL_ATTACK, CTL_ICON[CTL_ATTACK], !riding, 0, "", vec4(1, 1, 1, 1), "round_red");
    if (input.attackHeld && input.attackHoldT > 0.25f) {
        Rect r = ctlRect(*this, CTL_ATTACK);
        float f = saturate((input.attackHoldT - 0.25f) / 0.65f);
        u.ring({r.x + r.w / 2, r.y + r.h / 2}, r.w * 0.5f, r.w * 0.56f, -PI / 2, -PI / 2 + TAU * f, f >= 1 ? rgba(0xf8a030) : rgba(0xf6dc9a), 32);
    }
    ctl(CTL_DODGE, CTL_ICON[CTL_DODGE], stamina > 10, 0, "", vec4(1, 1, 1, 1), "round");
    ctl(CTL_JUMP, CTL_ICON[CTL_JUMP], true, 0, "", vec4(1, 1, 1, 1), "round");
    ctl(CTL_BLOCK, CTL_ICON[CTL_BLOCK], true, 0, "", vec4(1, 1, 1, 1), "round");
    for (int i = 0; i < 3; i++) {
        int si = equippedSpells[i];
        if (si < 0) continue;  // empty slot: hidden (assign spells in the Skills menu)
        const SpellDef& s = spells()[si];
        ctl(CTL_SKILL1 + i, s.icon.substr(2), mp >= s.mana, spellCD[si] / s.cooldown, "", vec4(1, 1, 1, 1), "round");
    }
    int pots = countItem("potion_minor") + countItem("potion_heal") + countItem("potion_great");
    ctl(CTL_POTION, "potion", pots > 0 && potionCD <= 0, potionCD / 1.2f, std::to_string(pots), vec4(1, 1, 1, 1), "round");
    ctl(CTL_MOUNT, "mount", !ownedMounts.empty(), 0, "", vec4(1, 1, 1, 1), "round");
    // contextual interaction button with label
    std::string label;
    if (fishingT >= 0) label = fishBite > 0 ? "Ferrer !" : "Ranger la canne";
    else if (!dungeonPrompt.empty()) label = dungeonPrompt;
    else if (interactTarget != NullEntity && reg.valid(interactTarget)) {
        Interact* it = reg.get<Interact>(interactTarget);
        if (it) {
            label = it->text;
            if (it->kind == InterKind::Npc) { Character* c = reg.get<Character>(interactTarget); if (c) label = "Parler à " + c->name; }
        }
    }
    if (!label.empty()) {
        ctl(CTL_INTERACT, fishingT >= 0 ? "fishing" : "interact", true, 0, "", rgba(0xf8e0a0), "round_hi");
        Rect r = ctlRect(*this, CTL_INTERACT);
        float tw = u.textWidth(label, 19) + 30;
        Rect lr{r.x + r.w / 2 - tw - r.w * 0.45f, r.y + r.h / 2 - 18, tw, 36};
        u.nine("panel_plain", lr, vec4(1, 1, 1, 0.85f), 0.4f);
        u.text(label, lr.x + lr.w / 2, lr.y + 7, 19, rgba(0xf4e7c4), F_BODY, A_CENTER);
    }
    // hints
    if (!toastHint.empty()) {
        hintT = hintT <= 0 ? 7.f : hintT;
        hintT -= dt;
        float a = std::min(1.f, hintT);
        float tw = std::min(W_ * 0.6f, u.textWidth(toastHint, 20) + 40);
        Rect r{W_ / 2 - tw / 2, H_ * 0.62f, tw, u.textHeight(toastHint, 20, F_BODY, tw - 30) + 22};
        u.nine("parchment", r, vec4(1, 1, 1, 0.9f * a), 0.5f);
        u.text(toastHint, W_ / 2, r.y + 10, 20, rgba(0x3a2a1a, a), F_BODY, A_CENTER, tw - 30);
        if (hintT <= 0) toastHint.clear();
    }
    if (riding && reg.valid(mountEnt)) u.text("Saut : descendre · Esquive (maintenir) : galop", W_ / 2, H_ - 40, 17, rgba(0xe8dcc0, 0.7f), F_BODY, A_CENTER, 0, true);
    if (p.dead && deathT > 0.5f) {
        float a = saturate((deathT - 0.5f) / 1.5f);
        u.rect({0, 0, W_, H_}, vec4(0.15f, 0, 0, 0.5f * a));
        u.text("Vous êtes tombé", W_ / 2, H_ * 0.38f, 60, rgba(0xd04030, a), F_TITLE, A_CENTER, 0, true);
    }
}

// ============================================================================================ notifications
void Game::drawNotifications(float dt) {
    UI& u = ui;
    if (menu != Menu::None || dialog.open) { for (auto& n : notes) n.t = std::min(n.t, 0.5f); return; }
    float y = state == GameState::Playing ? 176 : 20;
    for (auto& n : notes) {
        n.t += dt;
        float a = std::min(1.f, n.t * 5) * std::min(1.f, (4.5f - n.t));
        if (a <= 0) continue;
        float slide = std::max(0.f, 1 - n.t * 6) * 60;
        float tw = u.textWidth(n.text, 19) + (n.icon.empty() ? 30 : 64);
        Rect r{14 - slide, y, tw, 40};
        u.nine("panel_plain", r, vec4(1, 1, 1, 0.8f * a), 0.4f);
        float x = r.x + 14;
        if (!n.icon.empty() && u.hasRegion(n.icon.rfind("i_", 0) == 0 ? n.icon : "i_" + n.icon)) { u.sprite(n.icon.rfind("i_", 0) == 0 ? n.icon : "i_" + n.icon, {x, r.y + 6, 28, 28}, vec4(1, 1, 1, a)); x += 34; }
        u.text(n.text, x, r.y + 9, 19, vec4(n.color, a), F_BODY, A_LEFT, 0, false);
        y += 46;
    }
    while (!notes.empty() && notes.front().t > 4.5f) notes.pop_front();
}

}  // namespace lk
