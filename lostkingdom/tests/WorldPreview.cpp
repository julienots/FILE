// World preview harness: streams the world around given points and captures screenshots.
#include "../engine/core/App.h"
#include "../engine/gfx/Renderer.h"
#include "../engine/gfx/Particles.h"
#include "../game/World.h"
#include "../game/Props.h"
#include <cstdlib>
using namespace agr; using namespace lk;
struct Preview : App {
    Renderer r; World w; int W, H, f = 0, shot = 0; std::vector<vec3> spots;
    bool init(int w_, int h_) override {
        W = w_; H = h_; r.width = W; r.height = H;
        if (!r.init(GraphicsSettings::forProfile(QualityProfile::High))) return false;
        double t0 = platform::timeSeconds(); initPropTemplates(); AGR_LOG("props %.0f ms", (platform::timeSeconds()-t0)*1000);
        t0 = platform::timeSeconds(); w.buildPlan(); AGR_LOG("plan %.0f ms: %zu pois %zu structures", (platform::timeSeconds()-t0)*1000, w.pois.size(), w.structures.size());
        w.startWorkers(3);
        for (auto& a : platform::launchArgs()) { float x, z, yaw = 0.8f; if (sscanf(a.c_str(), "%f,%f,%f", &x, &z, &yaw) >= 2) spots.push_back({x, yaw, z}); }
        if (spots.empty()) spots.push_back({1480, 0.8f, 1660});
        return true;
    }
    void frame(float dt) override {
        vec3 s = spots[shot]; vec3 tgt{s.x, w.height(s.x, s.z), s.z};
        float yaw = s.y, dist = 60, pitch = 0.85f;
        Camera c; c.target = tgt; c.pos = tgt + vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)) * dist; c.update((float)W / H);
        w.update(tgt, 110, 64, f);
        Environment e; e.time = f * dt; e.sunDir = normalize(vec3(0.5f, 0.75f, 0.35f)); e.sunIntensity = 2.6f; e.sunColor = srgb(0xfff1d8);
        e.skyAmbient = srgb(0x8fa8c8) * 0.9f; e.groundAmbient = srgb(0x6a5a40) * 0.6f; e.fogColor = srgb(0xa9bfd0); e.fogDensity = 0.006f; e.tiltShift = 0.5f;
        e.playerPos = tgt;
        r.begin(c, e);
        int region = w.regionId(tgt.x, tgt.z);
        w.forEachVisibleChunk([&](Chunk& ch) {
            DrawItem d; d.tint = vec4(1, 1, 1, 0);
            if (ch.terrain.valid()) { d.mesh = &ch.terrain; d.flags = LIT_TERRAIN; r.submit(d); }
            if (ch.props.valid()) { d.mesh = &ch.props; d.flags = 0; d.tint.w = ch.snow * 0.6f; r.submit(d); }
            if (ch.foliage.valid()) { d.mesh = &ch.foliage; d.flags = LIT_WIND; r.submit(d); }
            if (ch.grass.valid()) { d.mesh = &ch.grass; d.flags = LIT_INSTANCED | LIT_WIND | LIT_GRASS; d.castShadow = false; r.submit(d); }
            if (ch.flowers.valid()) { d.mesh = &ch.flowers; d.flags = LIT_INSTANCED | LIT_WIND | LIT_GRASS; d.castShadow = false; r.submit(d); }
            if (ch.water.valid()) r.submitWater({&ch.water, mat4(), srgb(0x0f3a4a), srgb(0x3a8a86), 0.f, false});
            if (ch.lava.valid()) r.submitWater({&ch.lava, mat4(), vec3(0), vec3(0), 0.f, true});
        });
        r.render(nullptr);
        f++;
        if (f % 60 == 0) { char b[128]; snprintf(b, sizeof b, "build/world_%d.png", shot); platform::requestScreenshot(b); AGR_LOG("shot %d region %s chunks %zu gen %d (%.1f ms avg) draws %d tris %d", shot, regionName(region), w.loadedChunks(), w.chunksGenerated.load(), w.genMsTotal / std::max(1, w.chunksGenerated.load()), r.stats.drawCalls, r.stats.triangles); shot = std::min(shot + 1, (int)spots.size() - 1); }
    }
    void resize(int, int) override {} void touch(const TouchEvent&) override {} bool back() override { return false; }
    void pause() override {} void resume() override {} void contextLost() override {} void shutdown() override { w.stopWorkers(); r.shutdown(); }
    void renderAudio(float*, int, int) override {}
};
App* agr::createApp() { return new Preview(); }
