// Character preview: lineup of procedural characters with animations -> screenshots.
#include "../engine/core/App.h"
#include "../engine/gfx/Renderer.h"
#include "../game/Characters.h"
using namespace agr; using namespace lk;
struct Actor { Arch arch; Mesh mesh, weapon; Animator anim; PoseWorld pw; vec3 pos; float scale = 1, speed = 0; ClipId clip = C_COUNT; float clipEvery = 0; WeaponType wt = WeaponType::None; int stance = 0; };
struct Prev : App {
    Renderer r; Mesh ground; std::vector<Actor> actors; int W, H, f = 0; float t = 0;
    void add(Arch a, const MeshBuilder& mb, vec3 pos, float scale, float speed, ClipId c = C_COUNT, WeaponType wt = WeaponType::None) {
        actors.emplace_back(); Actor& ac = actors.back(); ac.arch = a; mb.upload(ac.mesh); ac.pos = pos; ac.scale = scale; ac.speed = speed; ac.clip = c; ac.wt = wt;
        setupAnimator(ac.anim, a); ac.anim.speed = speed; ac.anim.stance = stanceFor(wt);
        if (wt != WeaponType::None) { MeshBuilder w; buildWeapon(w, wt, 3, hexColor(0x6ff0ff)); w.upload(ac.weapon); }
    }
    bool init(int w, int h) override {
        W = w; H = h; r.width = W; r.height = H;
        if (!r.init(GraphicsSettings::forProfile(QualityProfile::Ultra))) return false;
        MeshBuilder g; g.plane({0, 0, 0}, {30, 30}, Mat(hexColor(0x6a8a4a), 0.9f)); g.upload(ground);
        HumanoidLook hero; hero.body = Body::Plate; hero.cape = true; hero.hair = Hair::Short; hero.c1 = hexColor(0x2a4f7a); hero.metal = hexColor(0xb8bcc4);
        MeshBuilder m; buildHumanoid(m, hero); add(Arch::Humanoid, m, {-6, 0, 0}, 1, 0, C_COUNT, WeaponType::Sword);
        m.clear(); buildHumanoid(m, hero); add(Arch::Humanoid, m, {-4, 0, 0}, 1, 4.6f, C_COUNT, WeaponType::Sword);
        m.clear(); buildHumanoid(m, hero); add(Arch::Humanoid, m, {-2, 0, 0}, 1, 0, C_1H_A3, WeaponType::Sword);
        HumanoidLook vill; vill.female = true; vill.body = Body::Dress; vill.hair = Hair::Braids; vill.hairCol = hexColor(0xc89a5a); vill.c1 = hexColor(0x8e4a3a);
        m.clear(); buildHumanoid(m, vill); add(Arch::Humanoid, m, {0, 0, 0}, 0.95f, 1.6f);
        HumanoidLook gob; gob.species = Species::Goblin; gob.skin = hexColor(0x6a8a3a); gob.headSize = 1.35f; gob.body = Body::Rags; gob.hair = Hair::None; gob.c1 = hexColor(0x6a4a2a);
        m.clear(); buildHumanoid(m, gob); add(Arch::Humanoid, m, {2, 0, 0}, 0.7f, 0, C_COUNT, WeaponType::Dagger);
        HumanoidLook sk; sk.species = Species::Skeleton; sk.body = Body::Bones; sk.emissiveEyes = 1; sk.glow = hexColor(0x6ff0ff);
        m.clear(); buildHumanoid(m, sk); add(Arch::Humanoid, m, {4, 0, 0}, 1, 1.5f, C_COUNT, WeaponType::Axe);
        HumanoidLook tr; tr.species = Species::Troll; tr.skin = hexColor(0x6a7a7a); tr.bulk = 1.7f; tr.body = Body::Rags; tr.hair = Hair::Mohawk; tr.c1 = hexColor(0x5a4a3a);
        m.clear(); buildHumanoid(m, tr); add(Arch::Humanoid, m, {6.5f, 0, 0}, 1.6f, 0, C_B_SLAM);
        HumanoidLook go; go.species = Species::Golem; go.bulk = 1.6f; go.glow = hexColor(0x40ffd0);
        m.clear(); buildHumanoid(m, go); add(Arch::Humanoid, m, {-6, 0, 3.5f}, 1.5f, 1.5f);
        CreatureLook wolf; wolf.c1 = hexColor(0x6a6a70); wolf.c2 = hexColor(0xb8b4ac); wolf.mane = true;
        m.clear(); buildQuadruped(m, wolf); add(Arch::Quadruped, m, {-2.5f, 0, 3.5f}, 1, 6.f);
        CreatureLook horse; horse.kind = 3; horse.c1 = hexColor(0x6a4a30); horse.c2 = hexColor(0x8a6a4a); horse.saddle = true; horse.girth = 1.1f;
        m.clear(); buildQuadruped(m, horse); add(Arch::Quadruped, m, {0.5f, 0, 3.5f}, 2.0f, 1.3f);
        CreatureLook sp; sp.c1 = hexColor(0x2a2028); sp.c2 = hexColor(0xc02020); sp.c3 = hexColor(0x141014); sp.glow = hexColor(0xff3030);
        m.clear(); buildSpider(m, sp); add(Arch::Spider, m, {3.5f, 0, 3.5f}, 1.4f, 2.f);
        CreatureLook dr; dr.c1 = hexColor(0x3a3634); dr.c2 = hexColor(0x8a5a3a); dr.c3 = hexColor(0x7a2a1a); dr.glow = hexColor(0xff8020);
        m.clear(); buildDragon(m, dr); add(Arch::Dragon, m, {7.5f, 0, 4.5f}, 3.f, 0, C_D_ROAR);
        return true;
    }
    void frame(float dt) override {
        t += dt; f++;
        Camera c; c.target = {1, 1.2f, 2}; c.pos = c.target + vec3(0, 9, 16); c.fovY = 40 * DEG; c.update((float)W / H);
        Environment e; e.time = t; e.sunDir = normalize(vec3(0.4f, 0.8f, 0.5f)); e.sunIntensity = 3.f; e.sunColor = srgb(0xfff1d8);
        e.skyAmbient = srgb(0x8fa8c8) * 0.9f; e.groundAmbient = srgb(0x6a5a40) * 0.6f; e.fogColor = srgb(0xa9bfd0); e.fogDensity = 0.002f;
        r.begin(c, e);
        DrawItem gd; gd.mesh = &ground; r.submit(gd);
        for (auto& a : actors) {
            if (a.clip != C_COUNT && (a.anim.actionDone())) a.anim.play(&clip(a.clip), 0.5f);
            a.anim.update(dt);
            computeWorld(*a.anim.skeleton, a.anim.pose, a.pw);
            mat4 model = mat4::trs(a.pos, quat(), vec3(a.scale));
            DrawItem d; d.mesh = &a.mesh; d.model = model; d.flags = LIT_SKINNED; d.bones = a.pw.skin[0].m; d.boneCount = a.anim.skeleton->size(); d.outline = true; d.fx.z = 0.6f;
            r.submit(d);
            if (a.weapon.valid()) { DrawItem w; w.mesh = &a.weapon; int b = a.wt == WeaponType::Bow ? H_PROPL : H_PROPR; w.model = model * a.pw.world[b]; r.submit(w); }
        }
        r.render(nullptr);
        if (f == 15) platform::requestScreenshot("build/chars.png");
    }
    void resize(int, int) override {} void touch(const TouchEvent&) override {} bool back() override { return false; }
    void pause() override {} void resume() override {} void contextLost() override {} void shutdown() override { r.shutdown(); }
    void renderAudio(float*, int, int) override {}
};
App* agr::createApp() { return new Prev(); }
