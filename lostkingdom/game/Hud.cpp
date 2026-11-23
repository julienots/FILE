#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>

namespace lk {

const char* CTL_ICON[CTL_COUNT] = {"attack", "dodge", "jump", "block", "", "", "", "potion", "interact", "mount", ""};

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
    // 2.3.3 fewer buttons: run = joystick pushed to the edge (no button), the mount button only near your horse
    // (jump gets you off, the garage calls it), no combat buttons in the saddle, no potion button with no potion
    if (id == CTL_SPRINT) return false;
    if (id == CTL_INTERACT) return interactTarget != NullEntity || !dungeonPrompt.empty() || fishingT >= 0;
    if (id == CTL_MOUNT) return mountNearby && !riding && driving < 0 && !dungeon.active;
    if (riding && (id == CTL_ATTACK || id == CTL_BLOCK || (id >= CTL_SKILL1 && id <= CTL_SKILL3))) return false;
    if (id >= CTL_SKILL1 && id <= CTL_SKILL3) return heroHasPowers() || equippedSpells[id - CTL_SKILL1] >= 0;
    if (id == CTL_POTION) return countItem("potion_minor") + countItem("potion_heal") + countItem("potion_great") > 0;
    return true;
}

void Game::updateTouchControls() {
    if (state != GameState::Playing || layoutEdit) return;
    bool modal = menu != Menu::None || dialog.open || cinematicT > 0 || transitionT >= 0 || cine.active || endingChoice;
    if (modal) { joyFinger = -1; camFinger = -1; return; }
    UI& u = ui;
    mountNearby = false;
    if (!riding && reg.valid(mountEnt)) if (Character* m = reg.get<Character>(mountEnt)) mountNearby = distXZ(m->pos, pc().pos) < 25.f;
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
        if (!f.pressed || f.consumed || f.blocked) continue;
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
        case CTL_SPRINT: sprintLock = !sprintLock; notify(sprintLock ? "Course activée" : "Marche", vec3(0.9f, 0.9f, 0.7f), "i_check"); break;
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
        // 1.11.2 keep sprinting and slide the same thumb onto Saut: it jumps (once per slide), the sprint goes on
        if (f->down && ctlVisible(CTL_JUMP)) {
            Rect jr = ctlRect(*this, CTL_JUMP);
            bool over = length(vec2(f->x, f->y) - vec2(jr.x + jr.w / 2, jr.y + jr.h / 2)) < jr.w * 0.5f + 8.f;
            if (over && !dodgeSlideJump) { input.jump = true; btnFlash[CTL_JUMP] = 1.f; haptic(8, 50); }
            dodgeSlideJump = over;
        }
        if (!f->down) { dodgeFinger = -1; dodgeSlideJump = false; }
    } else dodgeFinger = -1;
    {   // 3.2.0 a finger kept on Saut (the feather cape glides)
        input.jumpHeld = false;
        if (ctlVisible(CTL_JUMP)) {
            Rect jr = ctlRect(*this, CTL_JUMP);
            for (auto& f : ui.fingers) if (f.down && length(vec2(f.x, f.y) - vec2(jr.x + jr.w / 2, jr.y + jr.h / 2)) < jr.w * 0.5f + 12.f) input.jumpHeld = true;
        }
    }
    // block: held while the finger stays down (a quick tap still opens the parry window for one frame)
    if (UI::Finger* f = finger(blockFinger)) {
        f->consumed = true;
        input.block = true;
        if (!f->down) blockFinger = -1;
    } else blockFinger = -1;
    // top-right shortcuts
    Rect bMenu{u.W - 70, 14, 56, 56}, bMap{u.W - 132, 14, 56, 56}, bBag{u.W - 194, 14, 56, 56};
    Rect mini{u.W - 196, 80, 176, 176};
    // 2.3.2 the garage sits in the top bar (left of the bag), its list drops down left of the minimap: far from the action buttons
    Rect bGarage{u.W - 256, 14, 56, 56};
    int garageRows = 1;   // the horse, + 3.4.0 the vehicles already earned
    for (int i = 0; i < (int)vehicles.size(); i++) if (vehicleUnlocked(i)) garageRows++;
    Rect garagePanel{u.W - 436, 76, 224, 44.f + garageRows * 46.f};
    bool garageShown = driving < 0 && !dungeon.active;
    // 2.5.1 dig mode one tap away (top bar, left of the garage) when you carry a pickaxe or a shovel
    Rect bDig{u.W - 318, 14, 56, 56};
    bool digShown = !dungeon.active && driving < 0 && !riding && (countItem("tool_pickaxe") > 0 || countItem("tool_shovel") > 0);
    // ---- joystick (left 45%) and camera (right side)
    for (auto& f : u.fingers) {
        if (f.consumed || f.blocked) continue;
        if (bMenu.contains(f.sx, f.sy) || bMap.contains(f.sx, f.sy) || bBag.contains(f.sx, f.sy) || mini.contains(f.sx, f.sy)) continue;
        if ((garageShown && bGarage.contains(f.sx, f.sy)) || (garageOpen && garagePanel.contains(f.sx, f.sy))) continue;
        if (digShown && bDig.contains(f.sx, f.sy)) continue;
        if (Rect{14, 14, 108, 108}.contains(f.sx, f.sy)) continue;   // portrait (hero wheel)
        if (buildMode && Rect{ui.W * 0.5f - 330, 84, 660, 128}.contains(f.sx, f.sy)) continue;   // build panel (domain)
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
            // 1.11.2 joystick pushed to the edge for half a second = sprint (was 1.5 s after the touch began)
            float tdt = clampf(realTime - lastTouchT, 0.f, 0.1f);
            joyEdgeT = length(m) > 0.85f ? joyEdgeT + tdt : 0.f;
            if (joyEdgeT > 0.12f) input.sprint = true;   // 2.3.3: the run button is gone, pushing to the edge runs almost at once
        }
        if (!f.consumed && !f.blocked && f.id != joyFinger && f.down) camFingers.push_back(&f);
        if (f.id == camFinger) camAlive = f.down || f.released;
    }
    if (!joyAlive) { joyFinger = -1; joyEdgeT = 0; }
    lastTouchT = realTime;
    // camera rotation (one finger drag) and pinch zoom (two fingers)
    static std::map<int, vec2> lastPos;
    if (camFingers.size() >= 2) {
        float dd = length(vec2(camFingers[0]->x, camFingers[0]->y) - vec2(camFingers[1]->x, camFingers[1]->y));
        if (pinchDist > 0) input.pinch += (dd - pinchDist);
        pinchDist = dd;
    } else pinchDist = 0;
    for (auto* f : camFingers) {
        auto it = lastPos.find(f->id);
        if (it != lastPos.end() && camFingers.size() == 1) {
            // 1.10.2: free orbit - one drag turns AND tilts the camera together, following the finger in both
            // directions (the 1.6.1 axis lock is gone). A tiny start threshold keeps taps from nudging it.
            vec2 d{-(f->x - it->second.x), f->y - it->second.y};
            CamDragAxis& ax = camDragAxis[f->id];
            if (ax.axis < 0) {
                ax.acc += d;
                if (length(ax.acc) > 6.f) { ax.axis = 2; d = ax.acc; }
                else d = vec2(0, 0);
            }
            if (ax.axis >= 0) input.camDrag += d;
        }
        lastPos[f->id] = vec2(f->x, f->y);
    }
    for (auto it = lastPos.begin(); it != lastPos.end();) { bool alive = false; for (auto& f : u.fingers) if (f.id == it->first && f.down) alive = true; if (!alive) it = lastPos.erase(it); else ++it; }
    for (auto it = camDragAxis.begin(); it != camDragAxis.end();) { if (!lastPos.count(it->first)) it = camDragAxis.erase(it); else ++it; }
    // tap on an enemy to lock on / tap empty ground to release; double tap in the camera area: reset the camera
    for (auto& f : u.fingers) {
        if (!f.released || f.consumed || f.blocked || f.t > 0.3f || length(vec2(f.x - f.sx, f.y - f.sy)) > 12) continue;
        if (f.id == joyFinger) continue;
        if (realTime - lastCamTapT < 0.4f && length(vec2(f.x, f.y) - lastCamTapPos) < 70.f) { input.camReset = true; lastCamTapT = -10.f; }
        else { lastCamTapT = realTime; lastCamTapPos = vec2(f.x, f.y); }
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
    if (u.button(Rect{14, 14, 108, 108})) { menu = Menu::HeroWheel; play("ui_open", 0.6f); }   // portrait: hero wheel
    if (u.button(bMap) || u.button(mini)) { menu = Menu::Map; play("ui_open", 0.6f); mapPan = vec2(0, 0); }
    if (garageShown) {
        u.nine("button_dark", bGarage, vec4(1, 1, 1, garageOpen ? 1.f : 0.85f), 0.6f);
        u.sprite("i_mount", bGarage.inset(10), vec4(0.55f, 0.85f, 1.f, 1.f));
        u.tag(bGarage, "Garage");
        if (u.button(bGarage)) { garageOpen = !garageOpen; play("ui_click", 0.4f); }
        if (garageOpen) {
            u.nine("panel_plain", garagePanel, vec4(1, 1, 1, 0.94f), 0.5f);
            u.text("Appeler", garagePanel.x + 12, garagePanel.y + 8, 16, rgba(0xf4d58a), F_BOLD);
            int row = 0;
            for (int i = 0; i < (int)vehicles.size(); i++) {
                if (!vehicleUnlocked(i)) continue;   // 3.4.0 earned in the story
                Rect r{garagePanel.x + 12, garagePanel.y + 36 + row * 46.f, garagePanel.w - 24, 40};
                const VehicleDef& vd = vehicleDefs()[vehicles[i].def];
                if (u.fancyButton(r, vd.fly ? "Vaisseau volant" : (vd.moto ? "Moto" : "Voiture"), true, "button", 18)) { summonVehicle(i); garageOpen = false; }
                row++;
            }
            Rect r{garagePanel.x + 12, garagePanel.y + 36 + row * 46.f, garagePanel.w - 24, 40};
            if (u.fancyButton(r, riding ? "Cheval (en selle)" : "Cheval", !riding, "button", 18)) { summonHorse(); garageOpen = false; }
        }
    } else garageOpen = false;
    if (digShown) {
        u.nine(digMode ? "button" : "button_dark", bDig, vec4(1, 1, 1, digMode ? 1.f : 0.85f), 0.6f);
        u.sprite("i_pickaxe", bDig.inset(10), digMode ? vec4(1.f, 0.9f, 0.6f, 1.f) : vec4(1, 1, 1, 0.95f));
        u.tag(bDig, "Creuser");
        if (u.button(bDig)) {
            digMode = !digMode; play("ui_click", 0.4f);
            notify(digMode ? "Mode creusage : touchez la pioche (bouton épée) pour creuser ou miner devant vous. Maintenez pour lancer une bombe." : "Mode creusage terminé.", vec3(0.85f, 0.75f, 0.5f), "i_pickaxe");
        }
    } else if (dungeon.active || riding || driving >= 0) digMode = false;
    if (u.button(bBag)) { menu = Menu::Bag; selItem = -1; play("ui_open", 0.6f); }   // 2.2.0 the quick bag (the full journal is one tap away)
}


// ============================================================================================ tutorial (2.1.0)
struct TutStepDef { const char* title; const char* text; int ctl; };   // ctl: button to highlight (-1 joystick, -2 camera area, -3 none)
static const TutStepDef TUT[] = {
    {"Marcher", "Posez le pouce à gauche et glissez : le joystick vous fait avancer.", -1},
    {"Tourner la caméra", "Glissez un doigt à droite de l'écran pour tourner et incliner la caméra. Pincez pour zoomer.", -2},
    {"Courir", "Poussez le joystick jusqu'au bord : vous courez (un cercle doré l'indique). Relâchez un peu pour marcher.", -1},
    {"Sauter", "Touchez le bouton Saut pour franchir un obstacle ou escalader un rebord.", CTL_JUMP},
    {"Esquiver", "Touchez Esquive pour rouler : vous êtes invulnérable un court instant. Maintenez-le pour sprinter.", CTL_DODGE},
    {"Attaquer", "Touchez le bouton épée pour frapper. Maintenez-le pour charger une attaque puissante.", CTL_ATTACK},
    {"Parler", "Allez parler à Maëlle, la doyenne de Brindlemere (suivez la flèche de quête). Touchez-la quand le bouton Interagir apparaît.", -3},
};
constexpr int TUT_N = 7;

void Game::startTutorial() {
    if (storyFlags.count("tutorial_done") || player == NullEntity) return;
    tutStep = 0; tutT = 0; tutOkT = 0; tutAcc = 0; tutStart = pc().pos;
}

void Game::updateTutorial(float dt) {
    if (tutStep < 0 || state != GameState::Playing || cine.active || dialog.open || player == NullEntity) return;
    tutT += dt;
    if (tutOkT > 0) {   // step done: show the check for a moment, then the next
        tutOkT -= dt;
        if (tutOkT <= 0) { tutStep++; tutT = 0; tutAcc = 0; tutStart = pc().pos; if (tutStep >= TUT_N) { tutStep = -1; storyFlags.insert("tutorial_done"); notify("Tutoriel terminé. Bonne aventure !", vec3(0.7f, 1.f, 0.7f), "i_check"); } }
        return;
    }
    bool done = false;
    switch (tutStep) {
    case 0: done = distXZ(pc().pos, tutStart) > 6.f; break;
    case 1: tutAcc += length(input.camDrag) + std::fabs(input.pinch); done = tutAcc > 260.f; break;
    case 2: if (input.sprint && length(input.move) > 0.3f) tutAcc += dt; done = tutAcc > 1.2f; break;
    case 3: done = input.jump; break;
    case 4: done = input.dodge; break;
    case 5: done = input.attackRelease || input.attack; break;
    case 6: done = talkedTo.count("maelle") > 0 || questsDone.count("m1") > 0; break;
    }
    if (done) { tutOkT = 0.9f; play("quest", 0.5f, 1.3f); haptic(15, 80); }
}

void Game::drawTutorial(float dt) {
    (void)dt;
    if (tutStep < 0 || tutStep >= TUT_N || state != GameState::Playing || cine.active || dialog.open || menu != Menu::None || loadGate) return;
    UI& u = ui;
    const TutStepDef& d = TUT[tutStep];
    bool ok = tutOkT > 0;
    float pw = std::min(u.W * 0.56f, 640.f), ph = 118.f;
    Rect r{u.W / 2 - pw / 2, 84.f, pw, ph};
    u.nine("panel_plain", r, vec4(1, 1, 1, 0.92f), 0.55f);
    char head[96]; snprintf(head, sizeof head, "Tutoriel  %d / %d  ·  %s", tutStep + 1, TUT_N, d.title);
    u.text(head, r.x + 18, r.y + 12, 21, ok ? rgba(0x9ae88a) : rgba(0xf4d58a), F_BOLD);
    u.text(ok ? "Bien joué !" : d.text, r.x + 18, r.y + 42, 17, rgba(0xf4e7c4), F_BODY, A_LEFT, pw - 150);
    for (int i = 0; i < TUT_N; i++) u.circle({r.x + 22 + i * 18.f, r.y + ph - 14}, i == tutStep ? 5.f : 3.5f, i < tutStep ? rgba(0x9ae88a) : (i == tutStep ? rgba(0xf4d58a) : rgba(0x8a8070, 0.7f)), 10);
    Rect skip{r.x + r.w - 128, r.y + r.h - 44, 116, 34};
    u.nine("button_dark", skip, vec4(1, 1, 1, 0.9f), 0.6f);
    u.text("Passer", skip.x + skip.w / 2, skip.y + 7, 17, rgba(0xf4e7c4), F_SERIF, A_CENTER);
    if (u.button(skip)) { tutStep = -1; storyFlags.insert("tutorial_done"); return; }
    // pulsing ring around what to use
    float pulse = 0.5f + 0.5f * std::sin(realTime * 5.f);
    vec4 col = ok ? vec4(0.6f, 1.f, 0.55f, 0.9f) : vec4(1.f, 0.88f, 0.4f, 0.55f + 0.4f * pulse);
    if (d.ctl >= 0) {
        Rect b = ctlRect(*this, d.ctl);
        vec2 c{b.x + b.w / 2, b.y + b.h / 2};
        u.ring(c, b.w * (0.53f + 0.05f * pulse), b.w * (0.58f + 0.05f * pulse), 0, TAU, col, 40);
    } else if (d.ctl == -1) {
        vec2 jc{settings.layout.leftHanded ? u.W - 170 : 170, u.H - 170};
        float R = 70.f * settings.layout.scale;
        u.ring(jc, R * (1.5f + 0.06f * pulse), R * (1.58f + 0.06f * pulse), 0, TAU, col, 48);
    } else if (d.ctl == -2) {
        vec2 c{u.W * 0.72f, u.H * 0.45f};
        u.text("‹  glisser  ›", c.x, c.y + 8 * pulse, 30, col, F_BOLD, A_CENTER, 0, true);
    }
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
        if (e == coop.partner) return;
        if (o.team == Team::Enemy) u.circle(m, o.bossDef >= 0 ? 6.f : 3.5f, vec4(0.95f, 0.25f, 0.2f, 1), 10);
        else if (o.npcDef >= 0) u.circle(m, 3.5f, vec4(1.f, 0.85f, 0.4f, 1), 10);
        else if (o.team == Team::Mount) u.sprite("i_mount", {m.x - 9, m.y - 9, 18, 18});
    });
    // 2.2.0 personal pins (cyan diamonds, pinned to the rim when far)
    if (!dungeon.active) for (vec2 pin : mapPins) { vec2 m = toMini(vec3(pin.x, 0, pin.y), true, &in); u.circle(m, 7.f, vec4(0.03f, 0.08f, 0.12f, 0.9f), 4); u.circle(m, 5.f, vec4(0.35f, 0.9f, 1.f, 1.f), 4); }
    // 1.11.1 races: next checkpoint (pinned), start circles while driving
    if (!dungeon.active && race.active >= 0 && race.next < (int)races[race.active].cps.size()) {
        vec2 cp = races[race.active].cps[race.next];
        vec2 m = toMini(vec3(cp.x, 0, cp.y), true, &in);
        u.circle(m, 9.f + 2.f * std::sin(realTime * 8.f), vec4(0.4f, 0.85f, 1.f, 0.95f), 16);
    } else if (!dungeon.active && driving >= 0) {
        for (const RaceTrack& t : races) if (!t.cps.empty()) { vec2 m = toMini(vec3(t.start.x, 0, t.start.y), false, &in); if (in) u.circle(m, 8.f, vec4(0.5f, 1.f, 0.6f, 0.95f), 14); }
    }
    // 1.13.0 the other player: a blue arrow, pinned to the edge when far
    if (coop.connected && reg.valid(coop.partner)) {
        Character& o = *reg.get<Character>(coop.partner);
        vec2 m = toMini(o.pos, true, &in);
        u.circle(m, 8.f, vec4(0.05f, 0.1f, 0.2f, 0.9f), 14);
        u.circle(m, 6.f, o.dead ? vec4(0.5f, 0.5f, 0.6f, 1) : vec4(0.35f, 0.75f, 1.f, 1), 14);
    }
    // 1.11.1 vehicles: always findable (pinned to the edge of the minimap when far away)
    if (!dungeon.active)
        for (int i = 0; i < (int)vehicles.size(); i++) {
            if (i == driving) continue;
            vec2 m = toMini(vehicles[i].pos, true, &in);
            vec4 col = vehicleDefs()[vehicles[i].def].moto ? vec4(0.55f, 0.85f, 1.f, 1) : vec4(0.4f, 0.65f, 1.f, 1);
            u.circle(m, 11.f, vec4(0.05f, 0.08f, 0.15f, 0.85f), 16);
            u.sprite("i_mount", {m.x - 9, m.y - 9, 18, 18}, col);
        }
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
    if (cine.active) { drawCinematicShots(); return; }   // no HUD during story cinematics
    if (endingChoice) return;                              // 1.12.0 nor under the ending choice
    drawBuildHud();
    drawRaceHud();
    drawCoopHud();
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
    float hx = 50 + u.textWidth(std::to_string(gold), 20, F_BOLD) + 18;
    if (shards > 0) {   // 3.2.0 star shards, the currency of the Wonders
        u.sprite("i_crystal", {hx, 134, 26, 26}, rgba(0xb8a0ff));
        u.text(std::to_string(shards), hx + 30, 136, 20, rgba(0xc8b8ff), F_BOLD, A_LEFT, 0, true);
        hx += 30 + u.textWidth(std::to_string(shards), 20, F_BOLD) + 18;
    }
    hx = std::max(hx, 100.f);
    if (skillPoints > 0) { u.sprite("i_skills", {hx, 134, 28, 28}, rgba(0xf8d050, 0.7f + 0.3f * std::sin(realTime * 4))); u.text(std::to_string(skillPoints), hx + 32, 136, 20, rgba(0xf8d050), F_BOLD, A_LEFT, 0, true); }

    // ---- top-right: shortcuts & minimap
    // drawn only: their taps are handled in updateTouchControls (before the simulation). Using iconButton here
    // made the HUD swallow taps meant for the journal's close button, which sits on top of the menu icon.
    u.iconDraw({W_ - 70, 14, 56, 56}, "menu");
    u.iconDraw({W_ - 132, 14, 56, 56}, "map");
    u.iconDraw({W_ - 194, 14, 56, 56}, "inventory");
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
    if (joyFinger >= 0 && input.sprint && !riding) {   // 2.3.3 running: a golden rim on the stick (replaces the run button)
        u.ring(jc, R * 1.3f, R * 1.42f, 0, TAU, vec4(1.f, 0.82f, 0.4f, 0.85f * op), 48);
        u.text("COURSE", jc.x, jc.y - R * 1.75f, 15, rgba(0xf8e0a0), F_BOLD, A_CENTER, 0, true);
    } else if (joyFinger < 0 && tutStep == 2) u.text("Poussez au bord pour courir", jc.x, jc.y - R * 1.75f, 15, rgba(0xf8e0a0), F_BOLD, A_CENTER, 0, true);
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
    ctl(CTL_ATTACK, digMode ? "pickaxe" : CTL_ICON[CTL_ATTACK], !riding, 0, countItem("bomb") > 0 && digMode ? std::to_string(countItem("bomb")) : "", vec4(1, 1, 1, 1), digMode ? "round" : "round_red");
    if (digMode) { Rect r = ctlRect(*this, CTL_ATTACK); u.text("CREUSER", r.x + r.w / 2, r.y + r.h - 4, 15, rgba(0xf4e0b0), F_BOLD, A_CENTER, 0, true); }   // 2.4.0
    if (input.attackHeld && input.attackHoldT > 0.25f) {
        Rect r = ctlRect(*this, CTL_ATTACK);
        float f = saturate((input.attackHoldT - 0.25f) / 0.65f);
        u.ring({r.x + r.w / 2, r.y + r.h / 2}, r.w * 0.5f, r.w * 0.56f, -PI / 2, -PI / 2 + TAU * f, f >= 1 ? rgba(0xf8a030) : rgba(0xf6dc9a), 32);
    }
    {   // 2.4.0 in the water the dodge button dives / surfaces
        bool sw = player != NullEntity && pc().swimming;
        ctl(CTL_DODGE, sw ? (diving ? "jump" : "chevron_down") : CTL_ICON[CTL_DODGE], sw || stamina > 10, 0, "", sw ? vec4(0.6f, 0.9f, 1.f, 1.f) : vec4(1, 1, 1, 1), "round");
    }
    ctl(CTL_JUMP, CTL_ICON[CTL_JUMP], true, 0, "", vec4(1, 1, 1, 1), "round");
    ctl(CTL_BLOCK, CTL_ICON[CTL_BLOCK], true, 0, "", vec4(1, 1, 1, 1), "round");
    if (heroHasPowers()) {
        for (int i = 0; i < 3; i++) {
            const HeroPowerDef& pw = hero().powers[i];
            ctl(CTL_SKILL1 + i, pw.icon.substr(2), stamina >= pw.stamina && mp >= pw.mana, heroCD[i] / std::max(0.1f, pw.cooldown), "", vec4(hero().color, 1), "round");
        }
    } else
    for (int i = 0; i < 3; i++) {
        int si = equippedSpells[i];
        if (si < 0) continue;  // empty slot: hidden (assign spells in the Skills menu)
        const SpellDef& s = spells()[si];
        ctl(CTL_SKILL1 + i, s.icon.substr(2), mp >= s.mana, spellCD[si] / s.cooldown, "", vec4(1, 1, 1, 1), "round");
    }
    int pots = countItem("potion_minor") + countItem("potion_heal") + countItem("potion_great");
    ctl(CTL_POTION, "potion", pots > 0 && potionCD <= 0, potionCD / 1.2f, std::to_string(pots), vec4(1, 1, 1, 1), "round");
    ctl(CTL_MOUNT, "mount", true, 0, "", vec4(1, 1, 1, 1), "round");
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
    // 2.4.0 under water: a blue-green veil with darker edges, and the air gauge (bubbles) while diving
    if (underwaterView && player != NullEntity) {
        u.rect({0, 0, u.W, u.H}, vec4(0.05f, 0.28f, 0.34f, 0.38f));
        u.sprite("glow", {-u.W * 0.25f, -u.H * 0.25f, u.W * 1.5f, u.H * 1.5f}, vec4(0.f, 0.05f, 0.08f, 0.0f));
        for (int i = 0; i < 4; i++) u.rect({0, u.H - (i + 1) * 40.f, u.W, 40.f}, vec4(0.01f, 0.06f, 0.09f, 0.08f));
    }
    if (player != NullEntity && (diving || breath < 0.999f)) {
        Rect br{u.W * 0.5f - 120, u.H - 150, 240, 26};
        int n = 10, full = (int)std::ceil(breath * n - 0.01f);
        for (int i = 0; i < n; i++) {
            vec2 cpos{br.x + 12 + i * 24.f, br.y + 13};
            bool on = i < full;
            u.circle(cpos, 9.f, vec4(0.02f, 0.08f, 0.12f, 0.6f), 12);
            if (on) { u.circle(cpos, 7.f, vec4(0.65f, 0.9f, 1.f, breath < 0.25f ? 0.6f + 0.4f * std::sin(realTime * 10.f) : 0.95f), 12); u.circle(cpos + vec2(-2, -2), 2.f, vec4(1, 1, 1, 0.9f), 6); }
        }
        u.text(diving ? "Air  ·  Saut ou Esquive pour remonter" : "Air", br.x + br.w / 2, br.y - 22, 15, rgba(0xcfefff), F_BOLD, A_CENTER, 0, true);
    }
    drawPinsHud();
    drawTreasureHud();   // 3.2.0 compass, "item obtained"
    drawWorldEventHud();
    drawSpeedometer();
    drawTutorial(dt);
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
    if (menu != Menu::None || dialog.open || cine.active) { for (auto& n : notes) n.t = std::min(n.t, 0.5f); return; }   // shown after menus / cinematics
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
