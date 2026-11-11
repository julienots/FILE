// Renderer smoke test: terrain patch, rocks, spheres, water, particles -> screenshot.
#include "../engine/core/App.h"
#include "../engine/gfx/Renderer.h"
#include "../engine/gfx/MeshBuilder.h"
#include "../engine/gfx/Particles.h"
using namespace agr;
struct Smoke : App {
    Renderer r; ParticleSystem ps; Mesh terrain, objs, water; int W, H, f = 0; float t = 0;
    bool init(int w, int h) override {
        W = w; H = h; r.width = w; r.height = h;
        if (!r.init(GraphicsSettings::forProfile(QualityProfile::Ultra))) return false;
        ps.init(3000);
        MeshBuilder mb; int N = 64; float S = 1.f;
        auto hgt = [](float x, float z) { return fbm2(x * 0.03f, z * 0.03f, 4, 7) * 6.f; };
        for (int j = 0; j <= N; j++) for (int i = 0; i <= N; i++) {
            float x = (i - N / 2) * S, z = (j - N / 2) * S, y = hgt(x, z);
            vec3 n = normalize(vec3(hgt(x - 0.5f, z) - hgt(x + .5f, z), 1.f, hgt(x, z - .5f) - hgt(x, z + .5f)));
            Mat m(vec3(0.5f)); m.rough = clampf((y - 2.f) * 0.3f, 0, 1); m.metal = clampf(0.5f - std::fabs(x) * 0.05f, 0, 1) * (z > 0); m.emis = 0; m.wind = 0;
            Vertex& v = mb.v[mb.vert({x, y, z}, n, m)]; (void)v;
        }
        for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) { uint32_t a = j * (N + 1) + i; mb.tri(a, a + N + 1, a + 1); mb.tri(a + 1, a + N + 1, a + N + 2); }
        mb.upload(terrain);
        MeshBuilder o;
        o.rock({4, hgt(4, 2), 2}, {1.5f, 1.2f, 1.3f}, Mat(hexColor(0x8a8580), 0.8f), 7);
        o.ellipsoid({-3, hgt(-3, 0) + 1, 0}, vec3(1), Mat(hexColor(0xc0a060), 0.25f, 1.f), 16, 12);
        o.ellipsoid({0, hgt(0, 0) + 1, 0}, vec3(1), Mat(hexColor(0xa03030), 0.5f, 0.f), 16, 12);
        o.tube({2, hgt(2, -3), -3}, {2, hgt(2, -3) + 3, -3}, 0.4f, 0.25f, Mat(hexColor(0x6b4a2b)), 10);
        o.bevelBox({-5, hgt(-5, -4) + 1, -4}, {1.5f, 1.f, 1.f}, 0.2f, Mat(hexColor(0xb8a890)));
        o.cone({6, hgt(6, -2), -2}, {6, hgt(6, -2) + 4, -2}, 1.5f, Mat(hexColor(0x2d5a2d)), 9);
        o.upload(objs);
        MeshBuilder wb; for (int j = 0; j <= 16; j++) for (int i = 0; i <= 16; i++) { float x = (i - 8) * 4.f, z = (j - 8) * 4.f; uint32_t id = wb.vert({x, -1.f, z}, {0, 1, 0}, Mat()); wb.v[id].a = (uint8_t)clampf((-1.f - hgt(x, z)) / 8.f * 255, 0, 255); wb.v[id].rough = 128; wb.v[id].metal = 128; wb.v[id].emis = 0; }
        for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) { uint32_t a = j * 17 + i; wb.tri(a, a + 17, a + 1); wb.tri(a + 1, a + 17, a + 18); }
        wb.upload(water);
        return true;
    }
    void frame(float dt) override {
        t += dt; f++;
        Camera c; c.target = {0, 0, 0}; c.pos = {14, 22, 18}; c.update((float)W / H);
        Environment e; e.time = t; e.tiltShift = 0.6f; e.playerPos = {0, 0, 0};
        r.begin(c, e);
        DrawItem d; d.mesh = &terrain; d.flags = LIT_TERRAIN; r.submit(d);
        DrawItem o; o.mesh = &objs; o.outline = true; r.submit(o);
        r.submitWater({&water, mat4(), hexColor(0x0d3b4f), hexColor(0x3f8f8f), 0.f, false});
        r.addLight({{0, 3, 3}, 8, {4, 2, 0.8f}, 1});
        ps.fire({0, 2.2f, 0}, 1, dt); ps.update(dt);
        if (f % 20 == 1) ps.magicBurst({-3, 3, 0}, {0.4f, 0.6f, 1.f}, 1);
        r.render(&ps);
        if (f == 40) platform::requestScreenshot("build/smoke.png");
        glCheck("frame");
    }
    void resize(int, int) override {} void touch(const TouchEvent&) override {} bool back() override { return false; }
    void pause() override {} void resume() override {} void contextLost() override {} void shutdown() override { r.shutdown(); }
    void renderAudio(float*, int, int) override {}
};
App* agr::createApp() { return new Smoke(); }
