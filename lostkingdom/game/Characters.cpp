#include "Characters.h"
#include "../engine/core/Noise.h"
#include <mutex>

namespace lk {

// ------------------------------------------------------------------------------------------ skeletons
static Skeleton gHum, gQuad, gSpider, gDragon;
static std::once_flag gSkelOnce;

static void buildSkeletons() {
    Skeleton& h = gHum;
    h.add("root", -1, {0, 0.98f, 0});
    h.add("spine", H_ROOT, {0, 0.12f, 0});
    h.add("chest", H_SPINE, {0, 0.2f, 0});
    h.add("neck", H_CHEST, {0, 0.2f, 0});
    h.add("head", H_NECK, {0, 0.1f, 0});
    h.add("shL", H_CHEST, {0.07f, 0.15f, 0});
    h.add("uarmL", H_SHL, {0.12f, 0, 0});
    h.add("larmL", H_UARML, {0.02f, -0.28f, 0});
    h.add("handL", H_LARML, {0.01f, -0.25f, 0});
    h.add("shR", H_CHEST, {-0.07f, 0.15f, 0});
    h.add("uarmR", H_SHR, {-0.12f, 0, 0});
    h.add("larmR", H_UARMR, {-0.02f, -0.28f, 0});
    h.add("handR", H_LARMR, {-0.01f, -0.25f, 0});
    h.add("thighL", H_ROOT, {0.1f, -0.03f, 0});
    h.add("shinL", H_THIGHL, {0, -0.43f, 0});
    h.add("footL", H_SHINL, {0, -0.42f, 0});
    h.add("thighR", H_ROOT, {-0.1f, -0.03f, 0});
    h.add("shinR", H_THIGHR, {0, -0.43f, 0});
    h.add("footR", H_SHINR, {0, -0.42f, 0});
    h.add("propR", H_HANDR, {0, -0.07f, 0.02f});
    h.add("propL", H_HANDL, {0, -0.07f, 0.02f});
    h.finalize();

    // Quadruped: body along +Z, unit size ~ wolf (scaled per creature)
    Skeleton& q = gQuad;
    q.add("root", -1, {0, 0.72f, -0.42f});
    q.add("spine", Q_ROOT, {0, 0.03f, 0.38f});
    q.add("chest", Q_SPINE, {0, 0.0f, 0.36f});
    q.add("neck", Q_CHEST, {0, 0.12f, 0.16f});
    q.add("head", Q_NECK, {0, 0.1f, 0.16f});
    q.add("jaw", Q_HEAD, {0, -0.06f, 0.06f});
    q.add("tail1", Q_ROOT, {0, 0.04f, -0.14f});
    q.add("tail2", Q_TAIL1, {0, -0.02f, -0.2f});
    q.add("tail3", Q_TAIL2, {0, -0.03f, -0.2f});
    q.add("fl1", Q_CHEST, {0.14f, -0.08f, 0.04f});
    q.add("fl2", Q_FL1, {0, -0.3f, 0.02f});
    q.add("fl3", Q_FL2, {0, -0.31f, -0.02f});
    q.add("fr1", Q_CHEST, {-0.14f, -0.08f, 0.04f});
    q.add("fr2", Q_FR1, {0, -0.3f, 0.02f});
    q.add("fr3", Q_FR2, {0, -0.31f, -0.02f});
    q.add("bl1", Q_ROOT, {0.14f, -0.05f, 0.0f});
    q.add("bl2", Q_BL1, {0, -0.32f, -0.05f});
    q.add("bl3", Q_BL2, {0, -0.33f, 0.04f});
    q.add("br1", Q_ROOT, {-0.14f, -0.05f, 0.0f});
    q.add("br2", Q_BR1, {0, -0.32f, -0.05f});
    q.add("br3", Q_BR2, {0, -0.33f, 0.04f});
    q.finalize();

    // Dragon = quadruped + second neck segment + wings
    Skeleton& d = gDragon;
    d = q;
    d.bones.resize(Q_NECK2);
    d.add("neck2", Q_NECK, {0, 0.14f, 0.14f});
    d.bones[Q_HEAD].parent = Q_NECK2;
    d.add("wingL1", Q_CHEST, {0.12f, 0.12f, -0.05f});
    d.add("wingL2", Q_WINGL1, {0.45f, 0.1f, -0.05f});
    d.add("wingR1", Q_CHEST, {-0.12f, 0.12f, -0.05f});
    d.add("wingR2", Q_WINGR1, {-0.45f, 0.1f, -0.05f});
    d.finalize();

    Skeleton& s = gSpider;
    s.add("root", -1, {0, 0.55f, 0});
    s.add("abdomen", S_ROOT, {0, 0.1f, -0.35f});
    s.add("head", S_ROOT, {0, 0.02f, 0.28f});
    for (int i = 0; i < 8; i++) {
        int side = i < 4 ? 1 : -1, k = i % 4;
        float z = 0.18f - k * 0.12f;
        int up = s.add("leg" + std::to_string(i) + "a", S_ROOT, {side * 0.14f, 0.02f, z});
        s.add("leg" + std::to_string(i) + "b", up, {side * 0.38f, 0.32f, (z) * 0.5f});
    }
    s.finalize();
}

const Skeleton& skeleton(Arch a) {
    std::call_once(gSkelOnce, buildSkeletons);
    switch (a) {
    case Arch::Humanoid: return gHum;
    case Arch::Quadruped: return gQuad;
    case Arch::Spider: return gSpider;
    default: return gDragon;
    }
}

// ------------------------------------------------------------------------------------------ humanoid mesh
static vec3 pal_dark() { return hexColor(0x0d0b0a); }
static vec3 J(int b) { return skeleton(Arch::Humanoid).bones[b].bindWorld; }

void buildHumanoid(MeshBuilder& mb, const HumanoidLook& L) {
    skeleton(Arch::Humanoid);
    Rng rng(L.seed);
    const float B = L.bulk;
    bool skel = L.species == Species::Skeleton, golem = L.species == Species::Golem, treant = L.species == Species::Treant;
    bool specter = L.species == Species::Specter, robe = L.body == Body::Robe || L.body == Body::Dress || specter;
    bool bony = skel || L.body == Body::Bones;
    Mat skin(L.skin, 0.55f), cloth1(L.c1, 0.85f), cloth2(L.c2, 0.85f), boot(L.c3, 0.7f);
    Mat leather(lerp(L.c2, hexColor(0x5a3a22), 0.5f), 0.65f), metal(L.metal, 0.35f, 0.9f), trim(L.trim, 0.3f, 1.f);
    Mat bone(hexColor(0xe2d8c0), 0.6f), rock(hexColor(0x6a6660), 0.9f), bark(hexColor(0x5a4636), 0.9f);
    if (L.species == Species::Undead) skin = Mat(hexColor(0x8a9a7a), 0.7f);
    if (golem) skin = rock;
    if (treant) skin = bark;
    bool plate = L.body == Body::Plate;

    auto limb = [&](int a, int b, float ra, float rb, const Mat& m, bool caps = true) {
        if (golem) {  // chunky rock segments
            mb.setBone(a);
            vec3 pa = J(a), pb = J(b);
            mb.rock(lerp(pa, pb, 0.5f), vec3(ra * 1.6f, length(pb - pa) * 0.55f, ra * 1.6f), m, rng.next(), 0);
            return;
        }
        mb.tube(J(a), J(b), ra, rb, m, 9, caps, a, b, 0.7f);
    };

    // ---- legs & feet
    for (int s = 0; s < 2; s++) {
        int th = s ? H_THIGHR : H_THIGHL, sh = s ? H_SHINR : H_SHINL, ft = s ? H_FOOTR : H_FOOTL;
        float lr = bony ? 0.028f : 0.088f * B, kr = bony ? 0.025f : 0.062f * B, ar = bony ? 0.022f : 0.045f * B;
        const Mat& legM = bony ? bone : (robe ? cloth1 : cloth2);
        if (!specter) {
            limb(th, sh, lr, kr, legM);
            limb(sh, ft, kr, ar, legM);
            if (bony) { mb.setBone(sh); mb.ellipsoid(J(sh), vec3(0.04f), bone, 6, 4); }
        }
        // boots / feet
        mb.setBone(ft);
        vec3 a = J(ft);
        if (bony) mb.box(a + vec3(0, -0.06f, 0.06f), {0.035f, 0.025f, 0.08f}, bone);
        else if (!specter) {
            const Mat& bm = (plate || L.greaves) ? metal : boot;
            mb.bevelBox(a + vec3(0, -0.055f, 0.055f), vec3(0.055f, 0.05f, 0.12f) * vec3(B, 1, 1), 0.02f, bm);
            mb.tube(a + vec3(0, -0.02f, 0), a + vec3(0, 0.16f, 0), 0.058f * B, 0.062f * B, bm, 8, false, ft, sh, 0.6f);
            if (L.greaves || plate) { mb.setBone(sh); mb.ellipsoid(lerp(J(sh), a, 0.4f) + vec3(0, 0, 0.03f), vec3(0.06f, 0.14f, 0.05f) * B, metal, 8, 5); }
        }
    }
    // ---- torso
    if (bony) {
        mb.setBone(H_ROOT);
        mb.ellipsoid(J(H_ROOT) + vec3(0, 0, 0), vec3(0.13f, 0.06f, 0.07f), bone, 8, 4);
        mb.setBone(H_SPINE);
        mb.tube(J(H_ROOT), J(H_NECK), 0.03f, 0.025f, bone, 6, false, H_SPINE, H_CHEST);
        mb.setBone(H_CHEST);
        for (int i = 0; i < 5; i++) {
            float y = J(H_CHEST).y - 0.1f + i * 0.055f;
            for (int sd = -1; sd <= 1; sd += 2) {
                vec3 a{0, y, -0.07f}, b{sd * 0.13f, y - 0.01f, 0.0f}, c{sd * 0.06f, y - 0.03f, 0.08f};
                mb.tube(a, b, 0.012f, 0.012f, bone, 4, false);
                mb.tube(b, c, 0.012f, 0.01f, bone, 4, false);
            }
        }
        if (L.body == Body::Rags || L.body == Body::Bones) {
            mb.setBone(H_ROOT);
            mb.tube(J(H_ROOT) + vec3(0, 0.02f, 0), J(H_ROOT) + vec3(0, -0.22f, 0), 0.16f, 0.2f, Mat(lerp(L.c1, vec3(0.3f), 0.5f), 0.95f), 9, false);
        }
    } else if (specter) {
        mb.setBone(H_CHEST);
        mb.ellipsoid(J(H_CHEST) + vec3(0, 0.03f, 0), vec3(0.2f, 0.2f, 0.13f) * B, cloth1, 10, 7);
        mb.setBone(H_ROOT);
        // flowing robe that fades into wisps
        mb.lathe({{0.17f * B, 0.25f}, {0.24f * B, -0.25f}, {0.28f * B, -0.65f}, {0.12f, -0.9f}}, cloth1.withEmis(0.2f), 12, J(H_ROOT));
    } else {
        Mat torsoM = L.body == Body::Plate ? metal : (L.body == Body::Chain ? Mat(hexColor(0x8a8c90), 0.5f, 0.75f) : (L.body == Body::Leather ? leather : (L.body == Body::Fur ? Mat(hexColor(0x7a6450), 0.95f) : cloth1)));
        if (golem) torsoM = rock;
        if (treant) torsoM = bark;
        float bw = L.female ? 0.92f : 1.f;
        if (golem) {
            mb.setBone(H_CHEST); mb.rock(J(H_CHEST) + vec3(0, 0.05f, 0), vec3(0.3f, 0.24f, 0.2f) * B, rock, 21, 1);
            mb.setBone(H_SPINE); mb.rock(J(H_SPINE), vec3(0.2f, 0.16f, 0.15f) * B, rock, 22, 0);
            mb.setBone(H_ROOT); mb.rock(J(H_ROOT), vec3(0.2f, 0.12f, 0.14f) * B, rock, 23, 0);
            // glowing core
            mb.setBone(H_CHEST); mb.ellipsoid(J(H_CHEST) + vec3(0, 0.05f, 0.17f * B), vec3(0.07f), Mat(L.glow, 0.3f, 0, 4.f), 8, 6);
        } else {
            mb.setBone(H_ROOT);
            mb.ellipsoid(J(H_ROOT) + vec3(0, 0.01f, 0), vec3(0.155f * B, 0.11f, 0.11f * B), robe ? cloth1 : cloth2, 12, 7);
            mb.setBone(H_SPINE);
            mb.ellipsoid(J(H_SPINE) + vec3(0, 0.03f, 0), vec3(0.15f * B * bw, 0.13f, 0.1f * B), torsoM, 12, 7);
            mb.setBone(H_CHEST);
            mb.ellipsoid(J(H_CHEST) + vec3(0, 0.04f, 0), vec3(0.19f * B * bw, 0.16f, 0.12f * B), torsoM, 14, 8);
            if (L.female && !plate) mb.ellipsoid(J(H_CHEST) + vec3(0, 0.03f, 0.07f * B), vec3(0.13f * B, 0.07f, 0.06f), torsoM, 10, 6);
            // tunic hem / skirt (short) or long robe
            mb.setBone(H_ROOT);
            if (robe) {
                mb.lathe({{0.17f * B, 0.06f}, {0.21f * B, -0.3f}, {0.27f * B, -0.7f}, {0.3f * B, -0.92f}, {0.01f, -0.92f}}, cloth1, 14, J(H_ROOT));
                mb.lathe({{0.29f * B, -0.86f}, {0.305f * B, -0.92f}}, trim, 14, J(H_ROOT));
            } else if (L.body != Body::Plate) {
                // tunic hem down to mid-thigh, with a darker border
                mb.lathe({{0.165f * B, 0.04f}, {0.19f * B, -0.12f}, {0.205f * B, -0.24f}, {0.21f * B, -0.27f}}, torsoM, 14, J(H_ROOT));
                if (L.body == Body::Tunic || L.body == Body::Apron) mb.lathe({{0.207f * B, -0.25f}, {0.212f * B, -0.275f}}, Mat(torsoM.color * 0.7f, 0.85f), 14, J(H_ROOT));
            } else {
                // plate faulds
                for (int i = 0; i < 3; i++) mb.lathe({{(0.165f + i * 0.01f) * B, 0.02f - i * 0.06f}, {(0.18f + i * 0.012f) * B, -0.05f - i * 0.06f}}, metal, 12, J(H_ROOT));
                mb.ellipsoid(J(H_CHEST) + vec3(0, 0.03f, 0.045f * B), vec3(0.18f * B, 0.15f, 0.1f * B), Mat(L.metal * 1.05f, 0.25f, 1.f), 12, 7);
                mb.box(J(H_CHEST) + vec3(0, 0.03f, 0.142f * B), {0.012f, 0.1f, 0.008f}, trim);  // ridge
            }
            if (L.body == Body::Apron) {
                // work apron over the tunic, from the waist to the knees (curved to follow the hem)
                Mat apron(lerp(hexColor(0x8a7458), L.c2, 0.2f), 0.85f);
                mb.setBone(H_ROOT);
                for (int k = -2; k <= 2; k++) {
                    float a = k * 0.28f;
                    mb.bevelBox(J(H_ROOT) + vec3(std::sin(a) * 0.205f * B, -0.13f, std::cos(a) * 0.205f * B), {0.032f * B, 0.17f, 0.008f}, 0.004f, apron, quat::axisAngle({0, 1, 0}, a));
                }
            }
            if (!plate && L.body != Body::Rags) {
                // collar at the base of the neck (contrasting colour) and rounded shoulders
                Mat collar(lerp(L.c2, hexColor(0xe8dcc0), L.body == Body::Fur ? 0.f : 0.35f), 0.8f);
                mb.setBone(H_CHEST);
                mb.lathe({{0.07f * B, -0.025f}, {0.085f * B, 0.0f}, {0.075f * B, 0.025f}}, collar, 12, J(H_NECK));
                for (int sd = 0; sd < 2; sd++) {
                    int ua = sd ? H_UARMR : H_UARML;
                    mb.setBone(ua);
                    mb.ellipsoid(J(ua) + vec3((sd ? -1 : 1) * 0.01f, 0.005f, 0), vec3(0.074f, 0.066f, 0.078f) * B, torsoM, 10, 6);
                }
            }
            if (L.body == Body::Fur) { mb.setBone(H_CHEST); mb.flat = true; mb.rock(J(H_CHEST) + vec3(0, 0.17f, 0), vec3(0.24f * B, 0.08f, 0.15f * B), Mat(hexColor(0xa08870), 0.95f), 41, 0); mb.flat = false; }
            if (L.belt) {
                mb.setBone(H_ROOT);
                mb.lathe({{0.162f * B, 0.08f}, {0.165f * B, 0.12f}}, Mat(hexColor(0x3a2818), 0.6f), 12, J(H_ROOT));
                mb.box(J(H_ROOT) + vec3(0, 0.1f, 0.165f * B), {0.03f, 0.025f, 0.01f}, trim);
            }
            if (L.satchel) { mb.setBone(H_ROOT); mb.bevelBox(J(H_ROOT) + vec3(0.17f * B, -0.02f, 0.02f), {0.03f, 0.07f, 0.08f}, 0.01f, leather); }
        }
        if (L.pauldrons || plate) {
            for (int s = 0; s < 2; s++) {
                int ua = s ? H_UARMR : H_UARML;
                mb.setBone(ua);
                vec3 p = J(ua) + vec3((s ? -1 : 1) * 0.015f, 0.03f, 0);
                mb.ellipsoid(p, vec3(0.082f, 0.06f, 0.085f) * B, metal, 10, 6);
                mb.ellipsoid(p + vec3(0, 0.028f, 0), vec3(0.07f, 0.03f, 0.072f) * B, trim, 8, 4);
            }
        }
    }
    // ---- neck & head
    if (!specter) { mb.setBone(H_NECK); limb(H_NECK, H_HEAD, bony ? 0.025f : 0.055f * std::sqrt(B), bony ? 0.025f : 0.05f, bony ? bone : skin, false); }
    mb.setBone(H_HEAD);
    float hs = L.headSize;
    vec3 hc = J(H_HEAD) + vec3(0, 0.1f * hs, 0.01f);
    vec3 hr = vec3(0.1f, 0.12f, 0.115f) * hs;
    if (bony) {
        mb.ellipsoid(hc, hr * vec3(0.95f, 1, 1), bone, 12, 8);
        mb.ellipsoid(hc + vec3(0, -0.07f * hs, 0.05f * hs), vec3(0.06f, 0.035f, 0.06f) * hs, bone, 8, 4);  // jaw
        for (int s = -1; s <= 1; s += 2) mb.ellipsoid(hc + vec3(s * 0.038f * hs, 0.005f, 0.09f * hs), vec3(0.028f, 0.03f, 0.02f) * hs, Mat(pal_dark(), 1.f), 6, 4);
        for (int s = -1; s <= 1; s += 2) mb.ellipsoid(hc + vec3(s * 0.038f * hs, 0.005f, 0.1f * hs), vec3(0.012f) * hs, Mat(L.glow, 0.3f, 0, 3.f + L.emissiveEyes), 5, 3);
    } else if (golem) {
        mb.rock(hc, hr * 1.2f, rock, 31, 0);
        for (int s = -1; s <= 1; s += 2) mb.ellipsoid(hc + vec3(s * 0.04f, 0.0f, 0.11f), vec3(0.02f, 0.015f, 0.01f), Mat(L.glow, 0.3f, 0, 4.f), 5, 3);
    } else if (specter) {
        // hood with void face and two burning eyes
        mb.ellipsoid(hc + vec3(0, 0.01f, -0.01f), hr * 1.35f, cloth1, 12, 8);
        mb.ellipsoid(hc + vec3(0, -0.01f, 0.06f * hs), vec3(0.08f, 0.09f, 0.06f) * hs, Mat(pal_dark(), 1.f), 8, 6);
        for (int s = -1; s <= 1; s += 2) mb.ellipsoid(hc + vec3(s * 0.03f, 0.0f, 0.115f * hs), vec3(0.014f), Mat(L.glow, 0.3f, 0, 5.f), 5, 3);
    } else {
        mb.ellipsoid(hc, hr, skin, 14, 10);
        // jaw/cheeks
        mb.ellipsoid(hc + vec3(0, -0.05f * hs, 0.025f * hs), vec3(0.085f, 0.07f, 0.09f) * hs, skin, 10, 6);
        // eyes (1.6.1): small almond eyes with a dark iris and an upper lid; large white eyes read as "bug eyes"
        bool humanish = L.species == Species::Human || L.species == Species::Elf || L.species == Species::Dwarf;
        for (int s = -1; s <= 1; s += 2) {
            vec3 e = hc + vec3(s * 0.036f * hs, 0.012f * hs, 0.097f * hs);
            if (humanish && L.emissiveEyes <= 0) {
                mb.ellipsoid(e, vec3(0.016f, 0.0095f, 0.009f) * hs, Mat(hexColor(0xe6ddd0), 0.35f), 8, 5);
                mb.ellipsoid(e + vec3(0, -0.0005f, 0.0065f * hs), vec3(0.0085f, 0.0088f, 0.005f) * hs, Mat(L.eyes * 0.55f, 0.15f), 6, 4);
                mb.ellipsoid(e + vec3(0, 0.0075f * hs, 0.003f * hs), vec3(0.018f, 0.0055f, 0.0095f) * hs, Mat(L.skin * 0.82f, 0.6f), 8, 4);  // upper lid
            } else {
                mb.ellipsoid(e, vec3(0.019f, 0.014f, 0.01f) * hs, Mat(hexColor(0xf4efe6), 0.3f), 8, 5);
                mb.ellipsoid(e + vec3(0, 0, 0.007f * hs), vec3(0.009f, 0.01f, 0.006f) * hs, Mat(L.eyes, 0.2f, 0, L.emissiveEyes * 4.f), 6, 4);
            }
            // brow
            mb.box(e + vec3(0, 0.024f * hs, 0.002f), vec3(0.022f, 0.0055f, 0.008f) * hs, Mat(L.species == Species::Goblin ? L.skin * 0.7f : L.hairCol * 0.9f, 0.8f), quat::axisAngle({0, 0, 1}, s * 0.15f));
            // cheeks: a touch of colour
            if (humanish) mb.ellipsoid(hc + vec3(s * 0.05f * hs, -0.024f * hs, 0.066f * hs), vec3(0.024f, 0.015f, 0.016f) * hs, Mat(lerp(L.skin, hexColor(0xd0786a), 0.18f), 0.6f), 7, 4);
        }
        // nose
        float nose = L.species == Species::Goblin ? 1.9f : (L.species == Species::Troll ? 1.5f : 1.f);
        if (nose > 1.f) mb.ellipsoid(hc + vec3(0, -0.012f * hs, 0.112f * hs), vec3(0.016f, 0.026f, 0.022f) * hs * nose, skin, 7, 5);
        else {   // human nose: bridge + small tip, close to the face
            mb.ellipsoid(hc + vec3(0, -0.004f * hs, 0.1f * hs), vec3(0.011f, 0.022f, 0.014f) * hs, skin, 7, 5);
            mb.ellipsoid(hc + vec3(0, -0.024f * hs, 0.108f * hs), vec3(0.014f, 0.011f, 0.012f) * hs, skin, 7, 4);
        }
        // mouth
        mb.ellipsoid(hc + vec3(0, -0.056f * hs, 0.098f * hs), vec3(0.024f, 0.006f, 0.008f) * hs, Mat(lerp(L.skin, hexColor(0x9a4a44), 0.55f), 0.5f), 8, 4);
        // chin
        mb.ellipsoid(hc + vec3(0, -0.085f * hs, 0.07f * hs), vec3(0.035f, 0.025f, 0.03f) * hs, skin, 8, 5);
        // ears
        bool pointy = L.species == Species::Elf || L.species == Species::Goblin || L.species == Species::Demon;
        for (int s = -1; s <= 1; s += 2) {
            vec3 ep = hc + vec3(s * 0.1f * hs, 0, -0.005f);
            if (pointy) mb.cone(ep, ep + vec3(s * (L.species == Species::Goblin ? 0.14f : 0.07f), 0.06f, -0.03f) * hs, 0.025f * hs, skin, 6);
            else mb.ellipsoid(ep, vec3(0.015f, 0.03f, 0.02f) * hs, skin, 6, 4);
        }
        if (L.species == Species::Troll) for (int s = -1; s <= 1; s += 2) mb.cone(hc + vec3(s * 0.035f, -0.06f, 0.09f) * hs, hc + vec3(s * 0.05f, 0.0f, 0.12f) * hs, 0.012f * hs, Mat(hexColor(0xe8e0c8), 0.5f), 5);
        if (L.species == Species::Demon) for (int s = -1; s <= 1; s += 2) mb.cone(hc + vec3(s * 0.06f, 0.08f, 0) * hs, hc + vec3(s * 0.14f, 0.22f, -0.06f) * hs, 0.025f * hs, Mat(hexColor(0x2a2020), 0.4f), 6);
        if (treant) {
            // crown of leaves & branches instead of hair
            for (int i = 0; i < 6; i++) {
                float a = i * TAU / 6;
                mb.tube(hc + vec3(0, 0.08f, 0), hc + vec3(std::cos(a) * 0.14f, 0.25f, std::sin(a) * 0.14f), 0.02f, 0.008f, bark, 4, false);
            }
            mb.flat = true;
            mb.rock(hc + vec3(0, 0.22f, 0), vec3(0.22f, 0.1f, 0.22f), Mat(hexColor(0x4f8a3a), 0.8f), 51, 0);
            mb.flat = false;
        }
        // hair
        Mat hairM(L.hairCol, 0.55f);
        if (!treant && L.helm != Helm::Hood && L.helm != Helm::Great && L.helm != Helm::Skull) {
            switch (L.hair) {
            case Hair::Short:
                mb.ellipsoid(hc + vec3(0, 0.035f * hs, -0.014f), hr * vec3(1.07f, 0.92f, 1.05f), hairM, 12, 7);
                // fringe swept to one side, fuller crown, sideburns
                mb.ellipsoid(hc + vec3((rng.chance(0.5f) ? 1 : -1) * 0.025f * hs, 0.088f * hs, 0.062f * hs), vec3(0.07f, 0.03f, 0.045f) * hs, hairM, 9, 5);
                mb.ellipsoid(hc + vec3(0, 0.07f * hs, -0.03f * hs), vec3(0.1f, 0.07f, 0.1f) * hs, hairM, 10, 6);
                for (int sd = -1; sd <= 1; sd += 2) mb.ellipsoid(hc + vec3(sd * 0.095f * hs, -0.01f * hs, 0.03f * hs), vec3(0.012f, 0.03f, 0.018f) * hs, hairM, 5, 4);
                break;
            case Hair::Long: case Hair::Braids:
                mb.ellipsoid(hc + vec3(0, 0.035f * hs, -0.014f), hr * vec3(1.09f, 0.95f, 1.07f), hairM, 12, 7);
                mb.ellipsoid(hc + vec3(0, 0.082f * hs, 0.055f * hs), vec3(0.085f, 0.032f, 0.05f) * hs, hairM, 9, 5);  // parted fringe
                mb.ellipsoid(hc + vec3(0, -0.08f * hs, -0.06f * hs), vec3(0.11f, 0.17f, 0.065f) * hs, hairM, 10, 6);
                if (L.hair == Hair::Long) {
                    // hair falling on the shoulders, framing the face
                    for (int sd = -1; sd <= 1; sd += 2) mb.tube(hc + vec3(sd * 0.095f, 0.02f, 0.0f) * hs, hc + vec3(sd * 0.11f, -0.2f, -0.015f) * hs, 0.035f * hs, 0.025f * hs, hairM, 7, true);
                }
                if (L.hair == Hair::Braids) for (int s = -1; s <= 1; s += 2) mb.tube(hc + vec3(s * 0.08f, -0.05f, 0.02f) * hs, hc + vec3(s * 0.09f, -0.26f, 0.05f) * hs, 0.022f * hs, 0.015f * hs, hairM, 6, true);
                break;
            case Hair::Ponytail: mb.ellipsoid(hc + vec3(0, 0.03f * hs, -0.012f), hr * vec3(1.05f, 0.92f, 1.04f), hairM, 12, 7); break;  // tail is a verlet chain
            case Hair::Bun: mb.ellipsoid(hc + vec3(0, 0.03f * hs, -0.012f), hr * vec3(1.05f, 0.92f, 1.04f), hairM, 12, 7); mb.ellipsoid(hc + vec3(0, 0.07f, -0.1f) * hs, vec3(0.05f) * hs, hairM, 8, 6); break;
            case Hair::Mohawk: for (int i = 0; i < 5; i++) mb.cone(hc + vec3(0, 0.09f, 0.06f - i * 0.04f) * hs, hc + vec3(0, 0.2f, 0.03f - i * 0.05f) * hs, 0.03f * hs, hairM, 5); break;
            default: break;
            }
        }
        if (L.beard) {
            mb.ellipsoid(hc + vec3(0, -0.075f * hs, 0.065f * hs), vec3(0.07f, 0.08f, 0.05f) * hs * (L.species == Species::Dwarf ? 1.5f : 1.f), hairM, 10, 6);
            mb.box(hc + vec3(0, -0.035f * hs, 0.108f * hs), vec3(0.032f, 0.006f, 0.008f) * hs, hairM);  // moustache
        }
    }
    // villager hats (1.6.1)
    if (!bony && !golem && !specter && L.helm == Helm::None && L.hat > 0) {
        mb.setBone(H_HEAD);
        if (L.hat == 1) {   // straw hat: wide brim + low crown, woven band
            Mat straw(hexColor(0xd8b874), 0.9f);
            mb.lathe({{0.24f * hs, 0.065f * hs}, {0.2f * hs, 0.075f * hs}, {0.115f * hs, 0.085f * hs}, {0.11f * hs, 0.15f * hs}, {0.07f * hs, 0.175f * hs}, {0.01f, 0.18f * hs}}, straw, 16, hc);
            mb.lathe({{0.113f * hs, 0.09f * hs}, {0.113f * hs, 0.105f * hs}}, Mat(hexColor(0x7a3a2a), 0.8f), 16, hc);
        } else if (L.hat == 2) {   // soft cloth cap leaning back
            Mat cap(lerp(L.c2, hexColor(0x3a3028), 0.4f), 0.9f);
            mb.ellipsoid(hc + vec3(0, 0.07f * hs, -0.02f * hs), vec3(0.118f, 0.07f, 0.125f) * hs, cap, 12, 6);
            mb.lathe({{0.112f * hs, 0.045f * hs}, {0.116f * hs, 0.065f * hs}}, cap, 14, hc);
        } else if (L.hat == 3) {   // headscarf knotted at the back
            Mat scarf(lerp(L.c1, hexColor(0xe8dcc0), 0.45f), 0.9f);
            mb.ellipsoid(hc + vec3(0, 0.04f * hs, -0.012f), hr * vec3(1.13f, 0.98f, 1.1f), scarf, 12, 7);
            mb.cone(hc + vec3(0, -0.03f, -0.11f) * hs, hc + vec3(0, -0.12f, -0.16f) * hs, 0.035f * hs, scarf, 6);
        }
    }
    // helmets
    switch (L.helm) {
    case Helm::Hood: mb.ellipsoid(hc + vec3(0, 0.02f, -0.015f), hr * vec3(1.2f, 1.12f, 1.16f), cloth1, 12, 8); mb.cone(hc + vec3(0, 0.02f, -0.1f), hc + vec3(0, -0.02f, -0.24f), 0.07f, cloth1, 8); break;
    case Helm::Leather: mb.ellipsoid(hc + vec3(0, 0.035f, 0), hr * vec3(1.1f, 0.9f, 1.1f), leather, 12, 6); break;
    case Helm::Iron: mb.ellipsoid(hc + vec3(0, 0.035f, 0), hr * vec3(1.12f, 0.95f, 1.12f), metal, 12, 7); mb.box(hc + vec3(0, 0.0f, 0.12f * hs), {0.012f, 0.05f, 0.01f}, metal); break;
    case Helm::Great:
        mb.tube(hc + vec3(0, -0.12f, 0), hc + vec3(0, 0.12f, 0), 0.125f * hs, 0.12f * hs, metal, 12, true);
        mb.box(hc + vec3(0, 0.0f, 0.122f * hs), {0.08f, 0.012f, 0.008f}, Mat(pal_dark(), 1.f, 0, 0));
        if (L.emissiveEyes > 0) mb.box(hc + vec3(0, 0.0f, 0.124f * hs), {0.06f, 0.006f, 0.006f}, Mat(L.glow, 0.3f, 0, 4.f));
        mb.cone(hc + vec3(0, 0.12f, 0), hc + vec3(0, 0.25f, -0.06f), 0.03f, Mat(hexColor(0x8e2a24), 0.9f), 6);
        break;
    case Helm::Crown: for (int i = 0; i < 6; i++) { float a = i * TAU / 6; mb.cone(hc + vec3(std::cos(a) * 0.1f, 0.09f, std::sin(a) * 0.1f) * hs, hc + vec3(std::cos(a) * 0.1f, 0.17f, std::sin(a) * 0.1f) * hs, 0.022f * hs, trim, 4); }
        mb.lathe({{0.105f * hs, 0.075f * hs}, {0.105f * hs, 0.1f * hs}}, trim, 12, hc); break;
    case Helm::Horned: mb.ellipsoid(hc + vec3(0, 0.035f, 0), hr * vec3(1.12f, 0.95f, 1.12f), metal, 12, 7);
        for (int s = -1; s <= 1; s += 2) mb.tube(hc + vec3(s * 0.1f, 0.06f, 0) * hs, hc + vec3(s * 0.22f, 0.2f, 0.03f) * hs, 0.03f * hs, 0.006f, Mat(hexColor(0xe0d4b8), 0.5f), 6, true);
        break;
    case Helm::Circlet: mb.lathe({{0.104f * hs, 0.04f * hs}, {0.104f * hs, 0.055f * hs}}, trim, 12, hc); mb.ellipsoid(hc + vec3(0, 0.05f, 0.105f) * hs, vec3(0.015f), Mat(L.glow, 0.2f, 0, 3.f), 6, 4); break;
    case Helm::Skull: mb.ellipsoid(hc + vec3(0, 0.04f, 0.01f), hr * 1.12f, bone, 12, 8); for (int s = -1; s <= 1; s += 2) mb.cone(hc + vec3(s * 0.08f, 0.1f, 0) * hs, hc + vec3(s * 0.2f, 0.25f, -0.1f) * hs, 0.03f, bone, 6); break;
    default: break;
    }
    // ---- arms & hands
    for (int s = 0; s < 2; s++) {
        int ua = s ? H_UARMR : H_UARML, la = s ? H_LARMR : H_LARML, hd = s ? H_HANDR : H_HANDL;
        float r0 = bony ? 0.022f : 0.058f * B, r1 = bony ? 0.02f : 0.046f * B, r2 = bony ? 0.018f : 0.036f * B;
        bool sleeves = !bony && (L.body == Body::Tunic || L.body == Body::Robe || L.body == Body::Dress || L.body == Body::Chain || L.body == Body::Fur || L.body == Body::Apron || specter);
        Mat sleeve = L.body == Body::Chain ? Mat(hexColor(0x8a8c90), 0.5f, 0.75f) : cloth1;
        limb(ua, la, r0, r1, bony ? bone : (sleeves ? sleeve : (L.body == Body::Plate ? metal : skin)));
        bool longSleeve = sleeves && L.body != Body::Apron;
        limb(la, hd, r1, r2, bony ? bone : (L.gauntlets || plate ? metal : (L.body == Body::Leather ? leather : (longSleeve ? sleeve : skin))));
        if (longSleeve && !specter && !L.gauntlets && !plate) {   // cuff at the wrist
            mb.setBone(la);
            mb.tube(lerp(J(la), J(hd), 0.78f), lerp(J(la), J(hd), 0.92f), r2 * 1.32f, r2 * 1.4f, Mat(lerp(sleeve.color, hexColor(0xe8dcc0), 0.3f), 0.85f), 8, true);
        }
        if (L.gauntlets || plate || L.body == Body::Leather) { mb.setBone(la); mb.tube(lerp(J(la), J(hd), 0.45f), lerp(J(la), J(hd), 0.95f), r1 * 1.18f, r2 * 1.25f, L.body == Body::Leather ? leather : metal, 8, true); }
        mb.setBone(hd);
        vec3 hp = J(hd) + vec3(0, -0.045f, 0.005f);
        float hsz = L.species == Species::Troll || golem ? 1.6f : 1.f;
        if (golem) mb.rock(hp, vec3(0.06f, 0.07f, 0.06f) * hsz * B, rock, 61 + s, 0);
        else {
            mb.ellipsoid(hp, vec3(0.032f, 0.052f, 0.045f) * hsz * std::sqrt(B), bony ? bone : (L.gauntlets || plate ? metal : skin), 8, 6);
            mb.ellipsoid(hp + vec3((s ? -1 : 1) * 0.0f, -0.01f, 0.04f) * hsz, vec3(0.014f, 0.025f, 0.014f) * hsz, bony ? bone : skin, 6, 4);  // thumb
        }
    }
    if (golem) for (int i = 0; i < 6; i++) {  // crystals growing on the back
        mb.setBone(H_CHEST);
        vec3 b = J(H_CHEST) + vec3(rng.range(-0.15f, 0.15f), rng.range(0.f, 0.15f), -0.18f * B);
        mb.cone(b, b + vec3(rng.range(-0.1f, 0.1f), 0.25f, -0.12f), 0.05f, Mat(L.glow, 0.15f, 0, 1.5f), 5);
    }
    if (specter) for (auto& v : mb.v) v.emis = std::max<uint8_t>(v.emis, 8);
}


// ------------------------------------------------------------------------------------------ quadrupeds
static vec3 QJ(const Skeleton& sk, int b) { return sk.bones[b].bindWorld; }

void buildQuadruped(MeshBuilder& mb, const CreatureLook& L) {
    const Skeleton& sk = skeleton(Arch::Quadruped);
    Rng rng(L.seed);
    Mat fur(L.c1, 0.9f), belly(L.c2, 0.9f), dark(L.c3, 0.7f), hoof(hexColor(0x2a2420), 0.5f);
    float g = L.girth;
    bool hoofed = L.kind == 3 || L.kind == 4 || L.kind == 7;
    // body: hips, belly, chest (overlapping ellipsoids)
    mb.setBone(Q_ROOT); mb.ellipsoid(QJ(sk, Q_ROOT) + vec3(0, 0, 0.02f), vec3(0.17f, 0.18f, 0.22f) * g, fur, 12, 8);
    mb.setBone(Q_SPINE); mb.ellipsoid(QJ(sk, Q_SPINE), vec3(0.16f, 0.17f, 0.25f) * g, fur, 12, 8);
    mb.ellipsoid(QJ(sk, Q_SPINE) + vec3(0, -0.07f * g, 0), vec3(0.13f, 0.12f, 0.22f) * g, belly, 10, 6);
    mb.setBone(Q_CHEST); mb.ellipsoid(QJ(sk, Q_CHEST) + vec3(0, 0.0f, 0.02f), vec3(0.18f, 0.22f, 0.2f) * g, fur, 12, 8);
    if (L.mane) { mb.flat = true; mb.rock(QJ(sk, Q_CHEST) + vec3(0, 0.12f, 0.04f), vec3(0.2f, 0.17f, 0.2f) * g, Mat(L.c2 * 0.9f, 0.95f), 7, 1); mb.flat = false; }
    if (L.kind == 1) {  // boar: bristly hump
        mb.flat = true; mb.rock(QJ(sk, Q_CHEST) + vec3(0, 0.17f, -0.05f), vec3(0.14f, 0.12f, 0.25f) * g, dark, 9, 0); mb.flat = false;
    }
    // neck & head
    mb.tube(QJ(sk, Q_CHEST) + vec3(0, 0.08f, 0.1f), QJ(sk, Q_HEAD), 0.11f * g, 0.08f * g, fur, 10, false, Q_NECK, Q_HEAD, 0.6f);
    mb.setBone(Q_HEAD);
    vec3 h = QJ(sk, Q_HEAD);
    float snout = (L.kind == 1 ? 0.18f : (L.kind == 2 ? 0.12f : (hoofed ? 0.2f : 0.16f)));
    mb.ellipsoid(h + vec3(0, 0.02f, 0.02f), vec3(0.1f, 0.095f, 0.11f) * g, fur, 10, 7);
    mb.tube(h + vec3(0, 0.0f, 0.05f), h + vec3(0, -0.02f, 0.05f + snout), 0.065f * g, 0.045f * g, fur, 8, true);
    mb.ellipsoid(h + vec3(0, -0.01f, 0.05f + snout), vec3(0.03f, 0.025f, 0.02f), dark, 6, 4);  // nose
    mb.setBone(Q_JAW);
    mb.tube(QJ(sk, Q_JAW) + vec3(0, 0, -0.02f), QJ(sk, Q_JAW) + vec3(0, -0.01f, snout * 0.9f), 0.04f * g, 0.03f * g, belly, 7, true);
    if (L.tusks || L.kind == 1) for (int s = -1; s <= 1; s += 2) mb.cone(QJ(sk, Q_JAW) + vec3(s * 0.035f, 0, snout * 0.75f), QJ(sk, Q_JAW) + vec3(s * 0.06f, 0.09f, snout * 0.85f), 0.014f, Mat(hexColor(0xeee6d0), 0.4f), 5);
    mb.setBone(Q_HEAD);
    for (int s = -1; s <= 1; s += 2) {
        mb.ellipsoid(h + vec3(s * 0.06f, 0.04f, 0.09f), vec3(0.016f), Mat(L.glow, 0.2f, 0, 1.f + L.emissiveEyes * 3.f), 6, 4);
        if (L.kind == 0 || L.kind == 6 || L.kind == 3 || L.kind == 4 || L.kind == 7) mb.cone(h + vec3(s * 0.05f, 0.08f, -0.02f), h + vec3(s * 0.07f, 0.2f, -0.04f), 0.035f, fur, 5);  // ears
        else mb.ellipsoid(h + vec3(s * 0.07f, 0.08f, -0.02f), vec3(0.03f, 0.03f, 0.015f), fur, 6, 4);
        if (L.horns || L.kind == 4 || L.kind == 7) {
            vec3 b = h + vec3(s * 0.04f, 0.1f, 0.0f);
            mb.tube(b, b + vec3(s * 0.08f, 0.18f, -0.06f), 0.018f, 0.012f, Mat(hexColor(0xd8c8a8), 0.5f), 5, false);
            mb.tube(b + vec3(s * 0.08f, 0.18f, -0.06f), b + vec3(s * 0.05f, 0.3f, -0.12f), 0.012f, 0.004f, Mat(hexColor(0xd8c8a8), 0.5f), 5, true);
            mb.tube(b + vec3(s * 0.06f, 0.12f, -0.04f), b + vec3(s * 0.12f, 0.2f, 0.02f), 0.01f, 0.004f, Mat(hexColor(0xd8c8a8), 0.5f), 4, true);
        }
    }
    // tail
    float tl = L.kind == 2 || L.kind == 5 ? 0.3f : 1.f;
    mb.tube(QJ(sk, Q_TAIL1), lerp(QJ(sk, Q_TAIL1), QJ(sk, Q_TAIL2), 1.f), 0.05f * g, 0.045f * g * tl, fur, 7, false, Q_TAIL1, Q_TAIL2);
    if (tl > 0.5f) {
        mb.tube(QJ(sk, Q_TAIL2), QJ(sk, Q_TAIL3), 0.045f * g, 0.035f * g, fur, 7, false, Q_TAIL2, Q_TAIL3);
        mb.setBone(Q_TAIL3);
        mb.tube(QJ(sk, Q_TAIL3), QJ(sk, Q_TAIL3) + vec3(0, -0.05f, -0.18f), 0.035f * g, 0.01f, hoofed ? dark : belly, 6, true);
    }
    // legs
    int legs[4][3] = {{Q_FL1, Q_FL2, Q_FL3}, {Q_FR1, Q_FR2, Q_FR3}, {Q_BL1, Q_BL2, Q_BL3}, {Q_BR1, Q_BR2, Q_BR3}};
    for (int l = 0; l < 4; l++) {
        bool back = l >= 2;
        float r0 = (back ? 0.085f : 0.07f) * g, r1 = 0.04f * g, r2 = 0.03f * g;
        mb.tube(QJ(sk, legs[l][0]) + vec3(0, 0.06f, 0), QJ(sk, legs[l][1]), r0, r1, fur, 8, false, legs[l][0], legs[l][1]);
        mb.tube(QJ(sk, legs[l][1]), QJ(sk, legs[l][2]), r1, r2, hoofed ? fur : fur, 7, false, legs[l][1], legs[l][2]);
        mb.setBone(legs[l][2]);
        vec3 f = QJ(sk, legs[l][2]);
        if (hoofed) mb.tube(f + vec3(0, 0.0f, 0), f + vec3(0, -0.06f, 0.01f), r2 * 1.1f, r2 * 1.3f, hoof, 7, true);
        else mb.ellipsoid(f + vec3(0, -0.035f, 0.03f), vec3(0.04f, 0.03f, 0.055f) * g, L.kind == 2 ? dark : fur, 7, 4);
    }
    if (L.saddle) {
        mb.setBone(Q_SPINE);
        mb.ellipsoid(QJ(sk, Q_SPINE) + vec3(0, 0.15f * g, -0.02f), vec3(0.15f, 0.06f, 0.17f) * g, Mat(hexColor(0x6a3a22), 0.6f), 10, 5);
        mb.box(QJ(sk, Q_SPINE) + vec3(0, 0.05f * g, 0), vec3(0.17f * g, 0.12f * g, 0.02f), Mat(hexColor(0x8e2a24), 0.8f));
    }
    if (L.armored) {
        mb.setBone(Q_CHEST);
        mb.ellipsoid(QJ(sk, Q_CHEST) + vec3(0, 0.05f, 0.08f), vec3(0.19f, 0.2f, 0.16f) * g, Mat(hexColor(0x5a5c62), 0.35f, 0.9f), 10, 6);
    }
    if (L.spikes || L.crystal) for (int i = 0; i < 7; i++) {
        float t = i / 6.f;
        int b = t < 0.4f ? Q_ROOT : (t < 0.7f ? Q_SPINE : Q_CHEST);
        mb.setBone(b);
        vec3 base = lerp(QJ(sk, Q_ROOT), QJ(sk, Q_CHEST), t) + vec3(0, 0.15f * g, 0);
        mb.cone(base, base + vec3(0, 0.14f, -0.05f), 0.04f, L.crystal ? Mat(L.glow, 0.15f, 0, 2.f) : dark, 5);
    }
    mb.applyColorJitter(0, 0.06f, L.seed);
}

void buildSpider(MeshBuilder& mb, const CreatureLook& L) {
    const Skeleton& sk = skeleton(Arch::Spider);
    Mat body(L.c1, 0.5f, 0.1f), dark(L.c3, 0.4f, 0.1f), mark(L.c2, 0.5f, 0, 0.6f);
    mb.setBone(S_ROOT); mb.ellipsoid(QJ(sk, S_ROOT), vec3(0.16f, 0.12f, 0.2f), body, 12, 8);
    mb.setBone(S_ABDOMEN); mb.ellipsoid(QJ(sk, S_ABDOMEN) + vec3(0, 0.05f, -0.1f), vec3(0.26f, 0.22f, 0.32f), body, 14, 10);
    mb.ellipsoid(QJ(sk, S_ABDOMEN) + vec3(0, 0.24f, -0.1f), vec3(0.08f, 0.04f, 0.18f), mark, 8, 4);  // hourglass marking
    mb.setBone(S_HEAD); mb.ellipsoid(QJ(sk, S_HEAD), vec3(0.1f, 0.08f, 0.1f), dark, 10, 7);
    for (int i = 0; i < 8; i++) {
        float x = (i % 4 - 1.5f) * 0.025f, y = (i / 4) * 0.025f;
        mb.ellipsoid(QJ(sk, S_HEAD) + vec3(x, 0.05f + y, 0.08f), vec3(0.014f), Mat(L.glow, 0.2f, 0, 3.f), 5, 3);
    }
    for (int s = -1; s <= 1; s += 2) mb.cone(QJ(sk, S_HEAD) + vec3(s * 0.035f, -0.03f, 0.08f), QJ(sk, S_HEAD) + vec3(s * 0.02f, -0.12f, 0.13f), 0.022f, Mat(hexColor(0x1a1414), 0.3f), 5);  // fangs
    for (int i = 0; i < 8; i++) {
        int a = S_LEG0 + i * 2, b = a + 1;
        vec3 pa = QJ(sk, a), pb = QJ(sk, b);
        int side = i < 4 ? 1 : -1;
        vec3 tip = pb + vec3(side * 0.25f, -0.88f, 0.f);
        mb.tube(pa, pb, 0.035f, 0.028f, dark, 6, true, a, b, 0.8f);
        mb.setBone(b);
        mb.tube(pb, tip, 0.028f, 0.008f, body, 6, true);
    }
    if (L.crystal) for (int i = 0; i < 5; i++) { mb.setBone(S_ABDOMEN); vec3 p = QJ(sk, S_ABDOMEN) + vec3((i - 2) * 0.07f, 0.22f, -0.15f); mb.cone(p, p + vec3(0, 0.18f, -0.05f), 0.04f, Mat(L.glow, 0.15f, 0, 2.f), 5); }
}

void buildDragon(MeshBuilder& mb, const CreatureLook& L) {
    const Skeleton& sk = skeleton(Arch::Dragon);
    Mat scales(L.c1, 0.45f, 0.15f), belly(L.c2, 0.6f), horn(hexColor(0x2a2420), 0.4f), membrane(L.c3, 0.7f);
    float g = L.girth;
    mb.setBone(Q_ROOT); mb.ellipsoid(QJ(sk, Q_ROOT), vec3(0.18f, 0.18f, 0.24f) * g, scales, 12, 8);
    mb.setBone(Q_SPINE); mb.ellipsoid(QJ(sk, Q_SPINE), vec3(0.2f, 0.2f, 0.28f) * g, scales, 12, 8);
    mb.ellipsoid(QJ(sk, Q_SPINE) + vec3(0, -0.09f, 0), vec3(0.16f, 0.13f, 0.26f) * g, belly, 10, 6);
    mb.setBone(Q_CHEST); mb.ellipsoid(QJ(sk, Q_CHEST), vec3(0.21f, 0.22f, 0.22f) * g, scales, 12, 8);
    mb.tube(QJ(sk, Q_NECK) - vec3(0, 0, 0.05f), QJ(sk, Q_NECK2), 0.11f * g, 0.09f * g, scales, 9, false, Q_NECK, Q_NECK2);
    mb.tube(QJ(sk, Q_NECK2), QJ(sk, Q_HEAD), 0.09f * g, 0.075f * g, scales, 9, false, Q_NECK2, Q_HEAD);
    vec3 h = QJ(sk, Q_HEAD);
    mb.setBone(Q_HEAD);
    mb.ellipsoid(h + vec3(0, 0.02f, 0.03f), vec3(0.09f, 0.08f, 0.12f) * g, scales, 10, 7);
    mb.tube(h + vec3(0, 0.0f, 0.08f), h + vec3(0, -0.01f, 0.28f), 0.07f * g, 0.045f * g, scales, 8, true);
    for (int s = -1; s <= 1; s += 2) {
        mb.ellipsoid(h + vec3(s * 0.065f, 0.05f, 0.12f), vec3(0.02f, 0.012f, 0.02f), Mat(L.glow, 0.2f, 0, 4.f), 6, 4);
        mb.tube(h + vec3(s * 0.05f, 0.07f, 0.0f), h + vec3(s * 0.12f, 0.16f, -0.22f), 0.03f, 0.005f, horn, 6, true);
        mb.tube(h + vec3(s * 0.07f, 0.03f, -0.02f), h + vec3(s * 0.16f, 0.06f, -0.14f), 0.02f, 0.004f, horn, 5, true);
    }
    mb.setBone(Q_JAW);
    mb.tube(QJ(sk, Q_JAW) + vec3(0, 0, 0.02f), QJ(sk, Q_JAW) + vec3(0, -0.02f, 0.22f), 0.05f * g, 0.03f * g, belly, 7, true);
    for (int i = 0; i < 5; i++) for (int s = -1; s <= 1; s += 2) mb.cone(QJ(sk, Q_JAW) + vec3(s * 0.035f, 0.01f, 0.06f + i * 0.035f), QJ(sk, Q_JAW) + vec3(s * 0.035f, 0.05f, 0.065f + i * 0.035f), 0.008f, Mat(hexColor(0xf0e8d8), 0.4f), 4);
    mb.tube(QJ(sk, Q_TAIL1), QJ(sk, Q_TAIL2), 0.11f * g, 0.08f * g, scales, 8, false, Q_TAIL1, Q_TAIL2);
    mb.tube(QJ(sk, Q_TAIL2), QJ(sk, Q_TAIL3), 0.08f * g, 0.05f * g, scales, 8, false, Q_TAIL2, Q_TAIL3);
    mb.setBone(Q_TAIL3);
    mb.tube(QJ(sk, Q_TAIL3), QJ(sk, Q_TAIL3) + vec3(0, -0.05f, -0.35f), 0.05f * g, 0.01f, scales, 8, true);
    mb.flat = true;
    mb.cone(QJ(sk, Q_TAIL3) + vec3(0, -0.05f, -0.32f), QJ(sk, Q_TAIL3) + vec3(0, 0.08f, -0.42f), 0.07f, horn, 4);
    mb.flat = false;
    // dorsal spikes along neck, back and tail
    int spineBones[] = {Q_NECK2, Q_NECK, Q_CHEST, Q_SPINE, Q_ROOT, Q_TAIL1, Q_TAIL2};
    for (int b : spineBones) {
        mb.setBone(b);
        vec3 p = QJ(sk, b) + vec3(0, 0.17f * g, 0);
        mb.cone(p, p + vec3(0, 0.13f, -0.08f), 0.04f, horn, 4);
    }
    int legs[4][3] = {{Q_FL1, Q_FL2, Q_FL3}, {Q_FR1, Q_FR2, Q_FR3}, {Q_BL1, Q_BL2, Q_BL3}, {Q_BR1, Q_BR2, Q_BR3}};
    for (int l = 0; l < 4; l++) {
        mb.tube(QJ(sk, legs[l][0]) + vec3(0, 0.06f, 0), QJ(sk, legs[l][1]), 0.09f * g, 0.06f * g, scales, 8, false, legs[l][0], legs[l][1]);
        mb.tube(QJ(sk, legs[l][1]), QJ(sk, legs[l][2]), 0.06f * g, 0.045f * g, scales, 8, false, legs[l][1], legs[l][2]);
        mb.setBone(legs[l][2]);
        vec3 f = QJ(sk, legs[l][2]);
        mb.ellipsoid(f + vec3(0, -0.03f, 0.04f), vec3(0.06f, 0.035f, 0.08f) * g, scales, 7, 4);
        for (int c = -1; c <= 1; c++) mb.cone(f + vec3(c * 0.03f, -0.03f, 0.1f), f + vec3(c * 0.04f, -0.06f, 0.15f), 0.012f, horn, 4);
    }
    // wings: bone arms + membrane skinned between wing bones and the body
    for (int s = 0; s < 2; s++) {
        int w1 = s ? Q_WINGR1 : Q_WINGL1, w2 = s ? Q_WINGR2 : Q_WINGL2;
        float sx = s ? -1.f : 1.f;
        vec3 a = QJ(sk, w1), b = QJ(sk, w2);
        vec3 tip = b + vec3(sx * 0.55f, 0.15f, -0.1f);
        mb.tube(a, b, 0.035f, 0.025f, scales, 6, false, w1, w2, 0.8f);
        mb.setBone(w2);
        mb.tube(b, tip, 0.025f, 0.008f, scales, 6, true);
        // finger struts
        vec3 f1 = b + vec3(sx * 0.4f, -0.05f, -0.45f), f2 = b + vec3(sx * 0.15f, -0.08f, -0.55f);
        mb.tube(b, f1, 0.015f, 0.005f, scales, 4, false);
        mb.tube(b, f2, 0.015f, 0.005f, scales, 4, false);
        // membrane (double sided)
        auto memb = [&](vec3 p0, int b0, vec3 p1, int b1, vec3 p2, int b2) {
            vec3 n = normalize(cross(p1 - p0, p2 - p0));
            uint32_t i0 = mb.vertSkin(p0, n, membrane, b0, b0, 1), i1 = mb.vertSkin(p1, n, membrane, b1, b1, 1), i2 = mb.vertSkin(p2, n, membrane, b2, b2, 1);
            mb.tri(i0, i1, i2);
            uint32_t j0 = mb.vertSkin(p0, -n, membrane, b0, b0, 1), j1 = mb.vertSkin(p1, -n, membrane, b1, b1, 1), j2 = mb.vertSkin(p2, -n, membrane, b2, b2, 1);
            mb.tri(j0, j2, j1);
        };
        vec3 root = QJ(sk, Q_ROOT) + vec3(sx * 0.12f, 0.1f, 0.05f);
        memb(a, w1, b, w2, root, Q_ROOT);
        memb(b, w2, f2, w2, root, Q_ROOT);
        memb(b, w2, f1, w2, f2, w2);
        memb(b, w2, tip, w2, f1, w2);
    }
    mb.applyColorJitter(0, 0.07f, L.seed);
}

// ------------------------------------------------------------------------------------------ weapons
Stance stanceFor(WeaponType t) {
    switch (t) {
    case WeaponType::Sword: case WeaponType::Axe: return ST_ONEHAND;
    case WeaponType::Longsword: case WeaponType::Hammer: return ST_TWOHAND;
    case WeaponType::Dagger: return ST_DAGGER;
    case WeaponType::Spear: return ST_SPEAR;
    case WeaponType::Bow: case WeaponType::Crossbow: return ST_BOW;
    case WeaponType::Staff: return ST_STAFF;
    case WeaponType::Pickaxe: case WeaponType::WoodAxe: case WeaponType::FishingRod: case WeaponType::Torch: return ST_TOOL;
    default: return ST_UNARMED;
    }
}

void buildWeapon(MeshBuilder& mb, WeaponType t, int tier, vec3 accent, float* tipOut) {
    vec3 metalC = tier >= 4 ? hexColor(0xd8dce8) : (tier >= 2 ? hexColor(0xb8bcc4) : hexColor(0x9a9ca2));
    Mat blade(metalC, 0.22f, 1.f), grip(hexColor(0x4a3020), 0.7f), guard(tier >= 3 ? hexColor(0xd8b060) : hexColor(0x6a6a70), 0.3f, 1.f);
    Mat glow(accent, 0.2f, 0, tier >= 4 ? 3.f : (tier >= 3 ? 1.5f : 0.f));
    float tip = 0.8f;
    mb.flat = false;
    switch (t) {
    case WeaponType::Sword: case WeaponType::Longsword: {
        float L = t == WeaponType::Longsword ? 1.15f : 0.8f, W = t == WeaponType::Longsword ? 0.05f : 0.042f;
        float gl = t == WeaponType::Longsword ? 0.26f : 0.12f;
        mb.tube({0, 0, -gl}, {0, 0, 0.04f}, 0.018f, 0.018f, grip, 6, true);
        mb.ellipsoid({0, 0, -gl - 0.02f}, vec3(0.03f), guard, 6, 4);
        mb.box({0, 0, 0.05f}, {0.12f + tier * 0.01f, 0.018f, 0.018f}, guard);
        // blade: diamond cross-section with fuller, tapering tip
        mb.flat = true;
        vec3 b0{0, 0, 0.06f}, b1{0, 0, L * 0.85f}, bt{0, 0, L};
        vec3 sx{W, 0, 0}, sy{0, 0.012f, 0};
        mb.quadP(b0 - sx, b1 - sx * 0.85f, b1 + sy, b0 + sy, blade);
        mb.quadP(b0 + sy, b1 + sy, b1 + sx * 0.85f, b0 + sx, blade);
        mb.quadP(b0 + sx, b1 + sx * 0.85f, b1 - sy, b0 - sy, blade);
        mb.quadP(b0 - sy, b1 - sy, b1 - sx * 0.85f, b0 - sx, blade);
        mb.triP(b1 - sx * 0.85f, bt, b1 + sy, blade); mb.triP(b1 + sy, bt, b1 + sx * 0.85f, blade);
        mb.triP(b1 + sx * 0.85f, bt, b1 - sy, blade); mb.triP(b1 - sy, bt, b1 - sx * 0.85f, blade);
        mb.flat = false;
        if (tier >= 3) mb.box({0, 0, L * 0.45f}, {0.008f, 0.014f, L * 0.35f}, glow);  // runic fuller
        tip = L;
        break;
    }
    case WeaponType::Dagger:
        mb.tube({0, 0, -0.1f}, {0, 0, 0.02f}, 0.015f, 0.015f, grip, 6, true);
        mb.box({0, 0, 0.03f}, {0.06f, 0.015f, 0.012f}, guard);
        mb.flat = true;
        mb.cone({0, 0, 0.04f}, {0, 0, 0.36f}, 0.032f, blade, 4);
        mb.flat = false;
        tip = 0.36f;
        break;
    case WeaponType::Axe: case WeaponType::WoodAxe: {
        float L = t == WeaponType::Axe ? 0.75f : 0.65f;
        mb.tube({0, 0, -0.12f}, {0, 0, L}, 0.02f, 0.018f, Mat(hexColor(0x6a4a2a), 0.7f), 6, true);
        mb.flat = true;
        vec3 h{0, 0, L - 0.1f};
        mb.quadP(h + vec3(0.012f, 0.0f, -0.05f), h + vec3(0.012f, -0.2f, -0.12f), h + vec3(0.012f, -0.24f, 0.1f), h + vec3(0.012f, 0.0f, 0.06f), blade);
        mb.quadP(h + vec3(-0.012f, 0.0f, 0.06f), h + vec3(-0.012f, -0.24f, 0.1f), h + vec3(-0.012f, -0.2f, -0.12f), h + vec3(-0.012f, 0.0f, -0.05f), blade);
        if (t == WeaponType::Axe && tier >= 2) {
            mb.quadP(h + vec3(-0.012f, 0.0f, 0.06f), h + vec3(-0.012f, 0.14f, 0.08f), h + vec3(-0.012f, 0.12f, -0.07f), h + vec3(-0.012f, 0.0f, -0.05f), blade);
            mb.quadP(h + vec3(0.012f, 0.0f, -0.05f), h + vec3(0.012f, 0.12f, -0.07f), h + vec3(0.012f, 0.14f, 0.08f), h + vec3(0.012f, 0.0f, 0.06f), blade);
        }
        mb.flat = false;
        tip = L;
        break;
    }
    case WeaponType::Hammer:
        mb.tube({0, 0, -0.25f}, {0, 0, 0.95f}, 0.024f, 0.022f, Mat(hexColor(0x5a3a22), 0.7f), 6, true);
        mb.bevelBox({0, 0, 0.95f}, {0.11f, 0.14f, 0.11f}, 0.03f, Mat(metalC * 0.8f, 0.35f, 1.f), quat::axisAngle({0, 0, 1}, PI / 2));
        if (tier >= 3) mb.box({0, 0.12f, 0.95f}, {0.08f, 0.02f, 0.08f}, glow);
        tip = 1.0f;
        break;
    case WeaponType::Spear:
        mb.tube({0, 0, -0.7f}, {0, 0, 1.0f}, 0.02f, 0.018f, Mat(hexColor(0x6a4a2a), 0.7f), 6, true);
        mb.flat = true;
        mb.cone({0, 0, 1.0f}, {0, 0, 1.32f}, 0.05f, blade, 4);
        mb.flat = false;
        mb.lathe({{0.026f, 0.95f}, {0.03f, 1.0f}}, guard, 6, {}, quat::axisAngle({1, 0, 0}, PI / 2));
        tip = 1.32f;
        break;
    case WeaponType::Bow: {
        Mat wood(hexColor(0x7a5230), 0.6f);
        for (int s = -1; s <= 1; s += 2) {
            vec3 prev{0, 0, 0};
            for (int i = 1; i <= 5; i++) {
                float t2 = i / 5.f;
                vec3 p{0, s * t2 * 0.62f, -std::sin(t2 * 1.6f) * 0.12f + t2 * t2 * 0.05f};
                mb.tube(prev, p, 0.02f - t2 * 0.008f, 0.02f - t2 * 0.01f, wood, 5, false);
                prev = p;
            }
        }
        mb.tube({0, 0.62f, -0.08f}, {0, -0.62f, -0.08f}, 0.003f, 0.003f, Mat(hexColor(0xe8e0d0), 0.6f), 3, false);
        mb.tube({0, -0.06f, 0}, {0, 0.06f, 0}, 0.025f, 0.025f, grip, 6, true);
        tip = 0.6f;
        break;
    }
    case WeaponType::Crossbow:
        mb.box({0, 0, 0.2f}, {0.025f, 0.03f, 0.35f}, Mat(hexColor(0x6a4a2a), 0.7f));
        mb.box({0, 0.02f, 0.5f}, {0.32f, 0.015f, 0.02f}, Mat(metalC, 0.4f, 1.f));
        tip = 0.55f;
        break;
    case WeaponType::Staff: {
        mb.tube({0, 0, -0.6f}, {0, 0, 1.0f}, 0.022f, 0.018f, Mat(hexColor(0x5a3e2a), 0.7f), 6, true);
        for (int i = 0; i < 3; i++) {
            float a = i * TAU / 3;
            mb.tube({0, 0, 0.95f}, {std::cos(a) * 0.08f, std::sin(a) * 0.08f, 1.15f}, 0.012f, 0.006f, Mat(hexColor(0x5a3e2a), 0.7f), 4, true);
        }
        mb.ellipsoid({0, 0, 1.12f}, vec3(0.06f), Mat(accent, 0.1f, 0, 3.f + tier * 0.8f), 8, 6);
        tip = 1.12f;
        break;
    }
    case WeaponType::Shield: {
        Mat wood(hexColor(0x6a4a2a), 0.7f), face(accent, 0.6f);
        // kite shield facing +X (outwards from the left arm)
        mb.lathe({{0.28f, 0}, {0.3f, 0.01f}, {0.01f, 0.04f}}, face, 14, {0.06f, 0, 0.0f}, quat::axisAngle({0, 0, 1}, -PI / 2), 1.f);
        mb.lathe({{0.3f, 0.0f}, {0.31f, 0.015f}}, guard, 14, {0.06f, 0, 0.0f}, quat::axisAngle({0, 0, 1}, -PI / 2), 1.f);
        mb.ellipsoid({0.11f, 0, 0}, vec3(0.03f, 0.06f, 0.06f), guard, 6, 4);
        if (tier >= 2) { mb.box({0.105f, 0, 0}, {0.006f, 0.2f, 0.025f}, guard); mb.box({0.105f, 0, 0}, {0.006f, 0.025f, 0.2f}, guard); }
        tip = 0.3f;
        break;
    }
    case WeaponType::Pickaxe:
        mb.tube({0, 0, -0.1f}, {0, 0, 0.7f}, 0.018f, 0.018f, Mat(hexColor(0x6a4a2a), 0.7f), 6, true);
        mb.flat = true;
        mb.cone({0, 0, 0.65f}, {0, 0.32f, 0.6f}, 0.03f, Mat(hexColor(0x8a8c90), 0.4f, 1.f), 4);
        mb.cone({0, 0, 0.65f}, {0, -0.28f, 0.58f}, 0.03f, Mat(hexColor(0x8a8c90), 0.4f, 1.f), 4);
        mb.flat = false;
        tip = 0.7f;
        break;
    case WeaponType::FishingRod:
        mb.tube({0, 0, -0.2f}, {0, 0, 1.6f}, 0.016f, 0.006f, Mat(hexColor(0x9a7a4a), 0.6f), 5, true);
        mb.lathe({{0.035f, 0}, {0.035f, 0.03f}}, Mat(hexColor(0x8a8c90), 0.4f, 1.f), 8, {0, 0.03f, 0.05f});
        tip = 1.6f;
        break;
    case WeaponType::Torch:
        mb.tube({0, 0, -0.1f}, {0, 0, 0.45f}, 0.022f, 0.026f, Mat(hexColor(0x5a3a22), 0.8f), 6, true);
        mb.ellipsoid({0, 0, 0.47f}, vec3(0.04f, 0.04f, 0.06f), Mat(hexColor(0xff8a3a), 0.5f, 0, 4.f), 6, 4);
        tip = 0.5f;
        break;
    default: break;
    }
    if (tipOut) *tipOut = tip;
}

}  // namespace lk
