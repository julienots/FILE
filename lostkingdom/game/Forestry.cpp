// LOST KINGDOM — 3.6.0 felling trees. A swing at a tree in front of you (no enemy targeted) bites into it: wood
// chips fly, it creaks. An axe fells it in 3 blows, a woodcutter's axe in 2, any other weapon in 5. The tree then
// tips over away from you, slowly at first, crashes in a burst of dust and leaves, and the chunk is rebuilt with
// a stump in its place. You get wood (more from big trees), sometimes resin, apples (oaks), dates (palms) or a
// bird's nest. Felled trees grow back after a day of game time (saved with the game).
#include "Game.h"
#include "Props.h"

namespace lk {

static bool isAxe(WeaponType w) { return w == WeaponType::Axe || w == WeaponType::WoodAxe; }

bool Game::tryChopTree(Character& c) {
    if (dungeon.active) return false;
    vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
    vec3 front = c.pos + fwd * 0.9f;
    const TreeRef* best = nullptr; float bd = 1e9f;
    world.forEachVisibleChunk([&](Chunk& ch) {
        if (ch.lod != 0 || distXZ(ch.bounds.center(), c.pos) > CHUNK * 1.2f) return;
        for (const TreeRef& t : ch.trees) {
            float d = distXZ(t.pos, front);
            float reach = 1.1f + 0.35f * t.scale;
            if (d < reach && d < bd && std::fabs(t.pos.y - c.pos.y) < 2.5f && !world.isFelled(t.uid)) { bd = d; best = &t; }
        }
    });
    if (!best) return false;
    const TreeRef t = *best;
    WeaponType wt = c.vis.wt;
    int need = wt == WeaponType::WoodAxe ? 2 : (isAxe(wt) ? 3 : 5);
    if (t.scale > 1.3f) need++;
    int& hits = treeHits[t.uid];
    hits++;
    vec3 trunk = t.pos + vec3(0, 1.0f, 0);
    vec3 toMe = normalize(vec3(c.pos.x - t.pos.x, 0, c.pos.z - t.pos.z) + vec3(1e-3f, 0, 0));
    particles.sparks(trunk + toMe * 0.3f, toMe + vec3(0, 0.6f, 0), 10, vec3(0.78f, 0.58f, 0.32f));   // wood chips
    particles.dust(trunk, 0.5f, vec3(0.55f, 0.42f, 0.28f));
    play("hit_wood", 0.8f, particles.rng.range(0.85f, 1.05f));
    haptic(25, 120);
    addShake(0.08f);
    if (hits < need) {
        floatText(trunk + vec3(0, 0.6f, 0), std::string(hits == 1 ? "Toc" : "Crac"), vec3(0.95f, 0.8f, 0.55f), 18);
        if (!storyFlags.count("hint_chop")) { storyFlags.insert("hint_chop"); toastHint = isAxe(wt) ? "Frappez l'arbre jusqu'à ce qu'il tombe." : "Une hache abat les arbres bien plus vite."; }
        return true;
    }
    treeHits.erase(t.uid);
    // the falling tree: the same prop, stamped at the origin, drawn on its own while it falls
    ChunkBuild tb;
    stampProp(tb, (PropKind)t.kind, t.var, {0, 0, 0}, t.rot, t.scale, t.tint, false);
    FallingTree f;
    if (!tb.props.v.empty()) { f.props = std::make_shared<Mesh>(); tb.props.upload(*f.props); }
    if (!tb.foliage.v.empty()) { f.foliage = std::make_shared<Mesh>(); tb.foliage.upload(*f.foliage); }
    f.pos = t.pos; f.dir = -toMe; f.t = 0; f.scale = t.scale;
    fallingTrees.push_back(f);
    world.fell(t.uid, (float)gameTime, t.pos.xz());
    play("harvest_wood", 1.f, 0.6f);
    // the harvest
    Rng r((uint64_t)(t.uid * 7 + (uint32_t)gameTime));
    int wood = 2 + (int)std::round(t.scale * 2.f) + r.irange(0, 2);
    addItem("wood", wood);
    if (r.chance(0.3f)) addItem("resin", 1);
    if ((t.kind == P_OAK || t.kind == P_ELDERTREE || t.kind == P_WILLOW) && r.chance(0.12f)) addItem("honey", 1);
    if (t.kind == P_OAK && r.chance(0.5f)) addItem("apple", r.irange(1, 3));
    if ((t.kind == P_DATEPALM || t.kind == P_PALM) && r.chance(0.6f)) addItem(t.kind == P_PALM ? "coco" : "dates", r.irange(1, 2));
    if (r.chance(0.06f)) { addItem("bird_nest", 1); notify("Un nid d'oiseau tombe de l'arbre !", vec3(1.f, 0.9f, 0.6f), "i_star"); }
    gainXp(6 + (int)(t.scale * 4));
    treesFelled++;
    floatText(trunk + vec3(0, 1.2f, 0), "+" + std::to_string(wood) + " bois", vec3(0.95f, 0.8f, 0.5f), 22);
    return true;
}

void Game::updateForestry(float dt) {
    for (FallingTree& f : fallingTrees) {
        float prev = f.t;
        f.t += dt;
        // tips over: slow at first, then faster; a small bounce when it hits the ground
        float fall = std::min(1.f, f.t * f.t * 0.55f);
        f.angle = fall * (PI * 0.5f - 0.08f) + (f.t > 1.35f ? std::sin(std::min(1.f, (f.t - 1.35f) * 4.f) * PI) * 0.07f : 0.f);
        if (prev < 1.35f && f.t >= 1.35f) {   // the crash
            vec3 tip = f.pos + f.dir * 4.f * f.scale;
            for (int k = 0; k < 6; k++) particles.dust(lerp(f.pos, tip, k / 5.f) + vec3(0, 0.3f, 0), 1.3f, vec3(0.55f, 0.5f, 0.4f));
            particles.magicTrail(tip + vec3(0, 0.5f, 0), vec3(0.45f, 0.7f, 0.3f), 0.8f);
            play("land", 0.9f, 0.55f); play("hit_wood", 0.7f, 0.5f);
            addShake(clampf(0.35f - distXZ(f.pos, pc().pos) * 0.02f, 0.05f, 0.35f));
            haptic(60, 200);
        }
    }
    fallingTrees.erase(std::remove_if(fallingTrees.begin(), fallingTrees.end(), [](const FallingTree& f) { return f.t > 4.2f; }), fallingTrees.end());
    // regrowth: once a day of game time (1 h = 60 s)
    regrowT -= dt;
    if (regrowT <= 0) { regrowT = 20.f; world.regrowTrees((float)gameTime, 24.f * 60.f); }
}

void Game::renderFallingTrees() {
    for (const FallingTree& f : fallingTrees) {
        vec3 axis = normalize(cross(vec3(0, 1, 0), f.dir));
        float sink = f.t > 3.2f ? (f.t - 3.2f) * 1.2f : 0.f;   // then it settles into the ground and is gone
        float shrink = f.t > 3.2f ? std::max(0.f, 1.f - (f.t - 3.2f)) : 1.f;
        mat4 m = mat4::trs(f.pos - vec3(0, sink, 0), quat::axisAngle(axis, f.angle), vec3(shrink));
        DrawItem d; d.model = m; d.castShadow = true;
        if (f.props) { d.mesh = f.props.get(); d.flags = 0; renderer.submit(d); }
        if (f.foliage) { d.mesh = f.foliage.get(); d.flags = LIT_WIND; renderer.submit(d); }
    }
}

}  // namespace lk
