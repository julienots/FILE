#include "Renderer.h"
#include "Shaders.h"
#include "Particles.h"
#include "../core/Platform.h"
#include <algorithm>
#include <cstring>

namespace agr {

const char* profileName(QualityProfile p) {
    switch (p) {
    case QualityProfile::Ultra: return "Ultra";
    case QualityProfile::High: return "Élevé";
    case QualityProfile::Medium: return "Moyen";
    case QualityProfile::Low: return "Faible";
    default: return "Économie d'énergie";
    }
}

GraphicsSettings GraphicsSettings::forProfile(QualityProfile p) {
    GraphicsSettings s; s.profile = p;
    switch (p) {
    case QualityProfile::Ultra:
        s.renderScale = 1.0f; s.shadowRes = 2048; s.softShadows = true; s.bloom = true; s.fxaa = true; s.dof = true;
        s.terrainNormals = true; s.grassDensity = 1.0f; s.particleScale = 1.0f; s.drawDistance = 130; s.maxLights = 8; s.fpsCap = 60; break;
    case QualityProfile::High:
        s.renderScale = 0.85f; s.shadowRes = 2048; s.softShadows = false; s.bloom = true; s.fxaa = true; s.dof = true;
        s.terrainNormals = true; s.grassDensity = 0.8f; s.particleScale = 0.85f; s.drawDistance = 110; s.maxLights = 8; s.fpsCap = 60; break;
    case QualityProfile::Medium:
        s.renderScale = 0.75f; s.shadowRes = 1024; s.softShadows = false; s.bloom = true; s.fxaa = true; s.dof = false;
        s.terrainNormals = true; s.grassDensity = 0.55f; s.particleScale = 0.65f; s.drawDistance = 95; s.maxLights = 6; s.fpsCap = 60; break;
    case QualityProfile::Low:
        s.renderScale = 0.65f; s.shadowRes = 1024; s.softShadows = false; s.bloom = false; s.fxaa = false; s.dof = false;
        s.terrainNormals = false; s.grassDensity = 0.3f; s.particleScale = 0.45f; s.drawDistance = 80; s.maxLights = 4; s.fpsCap = 30; break;
    default:  // battery saver
        s.renderScale = 0.55f; s.shadowRes = 0; s.softShadows = false; s.bloom = false; s.fxaa = false; s.dof = false;
        s.terrainNormals = false; s.grassDensity = 0.15f; s.particleScale = 0.3f; s.drawDistance = 70; s.maxLights = 3; s.fpsCap = 30; break;
    }
    return s;
}

void Camera::update(float aspect) {
    view = mat4::lookAt(pos, target, {0, 1, 0});
    proj = mat4::perspective(fovY, aspect, zNear, zFar);
    viewProj = proj * view;
    frustum.fromMatrix(viewProj);
}

// std140 mirror of the GLSL "Frame" block
struct FrameUBO {
    float viewProj[16], view[16], shadow[16];
    float camPos[4], sunDir[4], sunColor[4], skyColor[4], groundColor[4], fogColor[4], fogParams[4], wind[4], player[4], misc[4];
    float lightPos[8][4], lightCol[8][4];
};

static std::string litDefines(int f) {
    std::string d;
    if (f & LIT_SKINNED) d += "#define SKINNED\n";
    if (f & LIT_INSTANCED) d += "#define INSTANCED\n";
    if (f & LIT_WIND) d += "#define WIND\n";
    if (f & LIT_GRASS) d += "#define GRASS\n";
    if (f & LIT_TERRAIN) d += "#define TERRAIN\n";
    if (f & LIT_DISSOLVE) d += "#define DISSOLVE\n";
    if (f & LIT_OUTLINE) d += "#define OUTLINE\n";
    if (f & LIT_TEXTURED) d += "#define TEXTURED\n";
    return d;
}

Shader* Renderer::lit(int flags) {
    auto it = litVariants.find(flags);
    if (it != litVariants.end()) return it->second.prog ? &it->second : nullptr;
    Shader& s = litVariants[flags];
    std::string defs = litDefines(flags);
    if (settings.softShadows) defs += "#define SHADOW_SOFT\n";
    if (!settings.terrainNormals) defs += "#define TERRAIN_SIMPLE\n";
    std::string fs = (flags & LIT_DEPTH) ? std::string(shaders::FRAME_UBO) + shaders::DEPTH_FS
                                         : std::string(shaders::FRAME_UBO) + shaders::LIGHTING + shaders::LIT_FS;
    if (!s.build("lit" + std::to_string(flags), std::string(shaders::FRAME_UBO) + shaders::LIT_VS, fs, defs)) return nullptr;
    s.use();
    s.set("uShadowMap", 0); s.set("uNoiseTex", 1); s.set("uTerrainTex", 2); s.set("uTerrainNrm", 3); s.set("uAlbedoTex", 5);
    return &s;
}

static bool buildFS(Shader& s, const char* name, const char* fs, const std::string& defs = "") {
    return s.build(name, std::string(shaders::FRAME_UBO) + shaders::FULLSCREEN_VS, std::string(shaders::FRAME_UBO) + fs, defs);
}

bool Renderer::init(const GraphicsSettings& s) {
    settings = s;
    gGpu.detect();
    if (gGpu.halfFloatColorBuffer || gGpu.floatColorBuffer) { hdrFormat = GL_RGBA16F; hdrScale = 1.f; }
    else { hdrFormat = GL_RGB10_A2; hdrScale = 4.f; AGR_WARN("No float render targets: using RGB10_A2 HDR encoding"); }

    glGenBuffers(1, &frameUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, frameUbo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(FrameUBO), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, frameUbo);

    const std::string U = shaders::FRAME_UBO, L = shaders::LIGHTING;
    bool ok = true;
    ok &= water.build("water", U + shaders::WATER_VS, U + L + shaders::WATER_FS, "");
    ok &= lava.build("lava", U + shaders::WATER_VS, U + L + shaders::WATER_FS, "#define LAVA\n");
    ok &= sky.build("sky", U + shaders::SKY_VS, U + shaders::SKY_FS, "");
    ok &= particleAlpha.build("particles", U + shaders::PARTICLE_VS, U + L + shaders::PARTICLE_FS, "");
    ok &= particleAdd.build("particlesAdd", U + shaders::PARTICLE_VS, U + L + shaders::PARTICLE_FS, "#define ADDITIVE\n");
    ok &= ribbon.build("ribbon", U + shaders::RIBBON_VS, U + shaders::RIBBON_FS, "");
    ok &= buildFS(bloomDown, "bloomDown", shaders::BLOOM_DOWN_FS);
    ok &= buildFS(bloomUp, "bloomUp", shaders::BLOOM_UP_FS);
    ok &= buildFS(composite, "composite", shaders::COMPOSITE_FS);
    ok &= buildFS(fxaa, "fxaa", shaders::FXAA_FS);
    ok &= buildFS(blit, "blit", shaders::BLIT_FS);
    ok &= ui.build("ui", shaders::UI_VS, shaders::UI_FS, "");
    if (!ok) { AGR_ERR("Renderer: shader compilation failed"); return false; }
    for (Shader* sh : {&water, &lava, &particleAlpha, &particleAdd}) { sh->use(); sh->set("uShadowMap", 0); sh->set("uNoiseTex", 1); sh->set("uAtlas", 4); }
    sky.use(); sky.set("uNoiseTex", 1);
    ribbon.use(); ribbon.set("uNoiseTex", 1);
    ui.use(); ui.set("uAtlas", 0); ui.set("uFont", 1); ui.set("uImage", 2);

    // Shader warm-up: compile every variant used in-game now to avoid hitches later.
    const int common[] = {0, LIT_WIND, LIT_TERRAIN, LIT_SKINNED, LIT_SKINNED | LIT_DISSOLVE, LIT_INSTANCED | LIT_WIND, LIT_INSTANCED | LIT_WIND | LIT_GRASS,
                          LIT_INSTANCED, LIT_SKINNED | LIT_OUTLINE, LIT_OUTLINE, LIT_DISSOLVE,
                          LIT_DEPTH, LIT_DEPTH | LIT_WIND, LIT_DEPTH | LIT_SKINNED, LIT_DEPTH | LIT_SKINNED | LIT_DISSOLVE, LIT_DEPTH | LIT_INSTANCED | LIT_WIND, LIT_DEPTH | LIT_INSTANCED};
    double t0 = platform::timeSeconds();
    for (int f : common) if (!lit(f)) { AGR_ERR("lit variant %d failed", f); return false; }
    AGR_LOG("Shader warm-up: %zu lit variants in %.0f ms", litVariants.size(), (platform::timeSeconds() - t0) * 1000);

    // Procedural textures
    t0 = platform::timeSeconds();
    ImageRGBA n = texgen::noiseTexture(256);
    noiseTex.create2D(n.w, n.h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, n.px.data(), true, true);
    std::vector<std::vector<uint8_t>> alb, nrm;
    texgen::terrainLayers(256, alb, nrm);
    terrainTex.createArray(256, 256, 8, alb, true);
    terrainNrm.createArray(256, 256, 8, nrm, true);
    ImageRGBA pa = texgen::particleAtlas(64);
    particleTex.create2D(pa.w, pa.h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, pa.px.data(), true, false);
    AGR_LOG("Procedural textures generated in %.0f ms", (platform::timeSeconds() - t0) * 1000);

    // Unit quad & fullscreen quad
    std::vector<Vertex> qv(4); std::vector<uint32_t> qi = {0, 1, 2, 0, 2, 3};
    float qx[4] = {-0.5f, 0.5f, 0.5f, -0.5f}, qy[4] = {-0.5f, -0.5f, 0.5f, 0.5f};
    for (int i = 0; i < 4; i++) { qv[i] = Vertex{}; qv[i].px = qx[i]; qv[i].py = qy[i]; }
    quadMesh.upload(qv, qi);
    for (int i = 0; i < 4; i++) { qv[i].px = qx[i] * 2; qv[i].py = qy[i] * 2; }
    fsMesh.upload(qv, qi);
    createTargets();
    return true;
}

void Renderer::applySettings(const GraphicsSettings& s) {
    bool rebuildShaders = s.softShadows != settings.softShadows || s.terrainNormals != settings.terrainNormals;
    settings = s;
    if (rebuildShaders) {
        for (auto& kv : litVariants) kv.second.destroy();
        litVariants.clear();
    }
    createTargets();
}

void Renderer::createTargets() {
    destroyTargets();
    int w = std::max(16, (int)(width * settings.renderScale)), h = std::max(16, (int)(height * settings.renderScale));
    if (!sceneRT.create(w, h, hdrFormat, true, false)) {
        hdrFormat = GL_RGB10_A2; hdrScale = 4.f;
        sceneRT.create(w, h, hdrFormat, true, false);
    }
    ldrRT.create(w, h, GL_RGBA8, false);
    blurRT.create(std::max(8, w / 4), std::max(8, h / 4), GL_RGBA8, false);
    if (settings.shadowRes > 0) shadowRT.create(settings.shadowRes, settings.shadowRes, 0, true, true);
    bloomChain.clear();
    int bw = w / 2, bh = h / 2;
    for (int i = 0; i < 5 && bw >= 8 && bh >= 8; i++) {
        bloomChain.emplace_back();
        bloomChain.back().create(bw, bh, hdrFormat == GL_RGBA16F ? GL_RGBA16F : GL_RGBA8, false);
        bw /= 2; bh /= 2;
    }
}

void Renderer::destroyTargets() {
    shadowRT.destroy(); sceneRT.destroy(); ldrRT.destroy(); blurRT.destroy();
    for (auto& b : bloomChain) b.destroy();
    bloomChain.clear();
}

void Renderer::resize(int w, int h) {
    if (w == width && h == height) return;
    width = w; height = h;
    if (frameUbo) createTargets();
}

void Renderer::shutdown() {
    destroyTargets();
    for (auto& kv : litVariants) kv.second.destroy();
    litVariants.clear();
    for (Shader* s : {&water, &lava, &sky, &particleAlpha, &particleAdd, &ribbon, &bloomDown, &bloomUp, &composite, &fxaa, &blit, &ui}) s->destroy();
    noiseTex.destroy(); terrainTex.destroy(); terrainNrm.destroy(); particleTex.destroy();
    quadMesh.destroy(); fsMesh.destroy();
    if (frameUbo) glDeleteBuffers(1, &frameUbo);
    frameUbo = 0;
}

void Renderer::begin(const Camera& cam, const Environment& e) {
    camera = cam; env = e;
    items.clear(); waters.clear(); ribbons.clear(); lights.clear();
    stats = RenderStats();
}

void Renderer::updateFrameUBO(const Camera& cam, const Environment& e, const mat4& sm, bool withShadow) {
    FrameUBO u;
    memset(&u, 0, sizeof(u));
    memcpy(u.viewProj, cam.viewProj.m, 64);
    memcpy(u.view, cam.view.m, 64);
    memcpy(u.shadow, sm.m, 64);
    float t = std::fmod(e.time, 3600.f);
    auto s4 = [](float* d, float a, float b, float c, float w) { d[0] = a; d[1] = b; d[2] = c; d[3] = w; };
    vec3 sd = normalize(e.sunDir);
    s4(u.camPos, cam.pos.x, cam.pos.y, cam.pos.z, t);
    s4(u.sunDir, sd.x, sd.y, sd.z, e.sunIntensity);
    s4(u.sunColor, e.sunColor.x, e.sunColor.y, e.sunColor.z, withShadow ? e.shadowStrength : 0.f);
    s4(u.skyColor, e.skyAmbient.x, e.skyAmbient.y, e.skyAmbient.z, 1.f / hdrScale);
    s4(u.groundColor, e.groundAmbient.x, e.groundAmbient.y, e.groundAmbient.z, e.wetness);
    s4(u.fogColor, e.fogColor.x, e.fogColor.y, e.fogColor.z, e.fogDensity);
    s4(u.fogParams, e.fogFalloff, e.fogBase, e.snowCover, e.night);
    s4(u.wind, e.windDir.x, e.windDir.y, e.windStrength, e.gust);
    s4(u.player, e.playerPos.x, e.playerPos.y, e.playerPos.z, e.grassPushRadius);
    int n = std::min((int)lights.size(), std::min(8, settings.maxLights));
    s4(u.misc, (float)n, settings.shadowRes > 0 ? 1.f / settings.shadowRes : 0.f, e.cloudShadows, e.lightning);
    for (int i = 0; i < n; i++) {
        s4(u.lightPos[i], lights[i].pos.x, lights[i].pos.y, lights[i].pos.z, lights[i].radius);
        s4(u.lightCol[i], lights[i].color.x, lights[i].color.y, lights[i].color.z, lights[i].flicker);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, frameUbo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FrameUBO), &u);
}

void Renderer::drawItem(const DrawItem& d, bool depthPass, bool outlinePass) {
    if (!d.mesh || !d.mesh->valid()) return;
    int f = d.flags;
    if (depthPass) f = (f & (LIT_SKINNED | LIT_INSTANCED | LIT_WIND | LIT_DISSOLVE)) | LIT_DEPTH;
    if (outlinePass) f = (f & (LIT_SKINNED)) | LIT_OUTLINE;
    Shader* s = lit(f);
    if (!s) return;
    s->use();
    s->set("uModel", d.model);
    if ((f & LIT_SKINNED) && d.bones) s->setMat4Array("uBones", d.bones, std::min(d.boneCount, 32));
    s->set("uFx", d.fx);
    if ((f & LIT_TEXTURED) && d.albedo) { glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, d.albedo); }
    if (!depthPass) {
        s->set("uTint", d.tint);
        s->set("uDissolveColor", vec4(d.dissolveColor, 1));
        if (outlinePass) { s->set("uOutline", vec4(d.outlineWidth, 0, 0, 0)); s->set("uOutlineColor", d.outlineColor); }
    }
    d.mesh->draw();
    stats.drawCalls++;
    stats.triangles += d.mesh->indexCount / 3 * std::max(1, d.mesh->instVbo ? d.mesh->instanceCount : 1);
}

void Renderer::shadowPass() {
    shadowsThisFrame = settings.shadowRes > 0 && shadowRT.fbo && env.sunIntensity > 0.05f && env.shadowStrength > 0.01f;
    if (!shadowsThisFrame) { shadowMatrix = mat4(); return; }
    // Fit an orthographic box around the camera focus; snap to texels to prevent shimmering.
    vec3 sd = normalize(env.sunDir);
    if (sd.y < 0.12f) sd = normalize(vec3(sd.x, 0.12f, sd.z));
    float radius = 34.f + std::min(40.f, length(camera.pos - camera.target) * 0.6f);
    vec3 focus = camera.target;
    vec3 up = std::fabs(sd.y) > 0.98f ? vec3(0, 0, 1) : vec3(0, 1, 0);
    mat4 lv = mat4::lookAt(focus + sd * 120.f, focus, up);
    float texel = radius * 2.f / settings.shadowRes;
    vec3 fl = lv.transformPoint(focus);
    float sx = std::fmod(fl.x, texel), sy = std::fmod(fl.y, texel);
    mat4 lp = mat4::ortho(-radius - sx, radius - sx, -radius - sy, radius - sy, 1.f, 260.f);
    mat4 lvp = lp * lv;
    mat4 bias = mat4::translate({0.5f, 0.5f, 0.5f}) * mat4::scale({0.5f, 0.5f, 0.5f});
    shadowMatrix = bias * lvp;
    Camera lc; lc.viewProj = lvp; lc.view = lv; lc.pos = focus + sd * 120.f;
    lc.frustum.fromMatrix(lvp);
    updateFrameUBO(lc, env, shadowMatrix, false);
    shadowRT.bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.6f, 3.f);
    glDisable(GL_CULL_FACE);  // thin geometry (leaves, cloth) must cast from both sides
    noiseTex.bind(1);
    for (auto& d : items) {
        if (!d.castShadow) continue;
        if (!d.mesh->instVbo && !lc.frustum.visible([&] { AABB b; vec3 c = d.model.transformPoint(d.mesh->bounds.center()); vec3 e = d.mesh->bounds.extent() * 1.8f; b.add(c - e); b.add(c + e); return b; }())) continue;
        drawItem(d, true, false);
        stats.shadowCasters++;
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void Renderer::render(ParticleSystem* particles) {
    double t0 = platform::timeSeconds();
    glBindVertexArray(0);
    shadowPass();
    updateFrameUBO(camera, env, shadowMatrix, shadowsThisFrame);

    sceneRT.bind();
    glDepthMask(GL_TRUE);
    glClearColor(env.fogColor.x / hdrScale, env.fogColor.y / hdrScale, env.fogColor.z / hdrScale, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_BLEND);

    if (shadowsThisFrame) shadowRT.depth.bind(0);
    else { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0); }
    noiseTex.bind(1); terrainTex.bind(2); terrainNrm.bind(3); particleTex.bind(4);

    // Opaque: sort by shader variant then front-to-back for early-z.
    std::vector<int> order;
    order.reserve(items.size());
    for (int i = 0; i < (int)items.size(); i++) {
        auto& d = items[i];
        if (!d.mesh) continue;
        if (!d.mesh->instVbo) {
            AABB b; vec3 c = d.model.transformPoint(d.mesh->bounds.center()); vec3 e = d.mesh->bounds.extent() * 1.8f + vec3(0.5f);
            b.add(c - e); b.add(c + e);
            if (!camera.frustum.visible(b)) continue;
            items[i].sortKey = length2(c - camera.pos);
        }
        order.push_back(i);
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        if (items[a].flags != items[b].flags) return items[a].flags < items[b].flags;
        return items[a].sortKey < items[b].sortKey;
    });
    for (int i : order) {
        bool twoSided = (items[i].flags & (LIT_WIND | LIT_TEXTURED)) != 0;  // imported models are often double-sided (capes)
        if (twoSided) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
        drawItem(items[i], false, false);
    }
    // Stylised outlines (inverted hull) for characters and interactables
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    for (int i : order) if (items[i].outline) drawItem(items[i], false, true);
    glCullFace(GL_BACK);

    // Sky (only where nothing was drawn)
    {
        sky.use();
        sky.set("uInvViewProj", camera.viewProj.inverse());
        sky.set("uSkyTop", vec4(env.skyTop, 1));
        sky.set("uSkyHorizon", vec4(env.skyHorizon, env.cloudCover));
        glDepthMask(GL_FALSE);
        fsMesh.draw();
        glDepthMask(GL_TRUE);
        stats.drawCalls++;
    }

    // Water & lava (alpha blended, depth tested, no depth write)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    for (auto& w : waters) {
        Shader& s = w.lava ? lava : water;
        s.use();
        s.set("uModel", w.model);
        s.set("uWaterDeep", vec4(w.deep, 1));
        s.set("uWaterShallow", vec4(w.shallow, w.frozen));
        if (w.lava) glDisable(GL_BLEND); else glEnable(GL_BLEND);
        if (w.lava) glDepthMask(GL_TRUE);
        w.mesh->draw();
        if (w.lava) glDepthMask(GL_FALSE);
        stats.drawCalls++;
    }
    glEnable(GL_BLEND);
    // Particles
    if (particles) {
        vec3 fwd = normalize(camera.target - camera.pos);
        vec3 right = normalize(cross(fwd, {0, 1, 0}));
        vec3 up = cross(right, fwd);
        particleTex.bind(4);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        particleAlpha.use(); particleAlpha.set("uCamRight", right); particleAlpha.set("uCamUp", up);
        stats.drawCalls += particles->draw(false, quadMesh);
        glBlendFunc(GL_ONE, GL_ONE);
        particleAdd.use(); particleAdd.set("uCamRight", right); particleAdd.set("uCamUp", up);
        stats.drawCalls += particles->draw(true, quadMesh);
    }
    // Ribbons (weapon trails)
    glBlendFunc(GL_ONE, GL_ONE);
    ribbon.use();
    for (auto& r : ribbons) { ribbon.set("uIntensity", r.intensity / hdrScale); r.m->draw(); stats.drawCalls++; }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);

    postProcess();
    stats.cpuMs = (platform::timeSeconds() - t0) * 1000;
}

void Renderer::postProcess() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    // Bloom: progressive downsample then tent upsample with additive accumulation.
    bool doBloom = settings.bloom && bloomChain.size() >= 3;
    if (doBloom) {
        bloomDown.use();
        const Texture* src = &sceneRT.color;
        for (size_t i = 0; i < bloomChain.size(); i++) {
            bloomChain[i].bind();
            src->bind(0);
            bloomDown.set("uSrc", 0);
            bloomDown.set("uTexel", vec2(1.f / src->w, 1.f / src->h));
            bloomDown.set("uThreshold", i == 0 ? 1.0f : -1.f);
            bloomDown.set("uHdrScale", i == 0 ? hdrScale : 1.f);
            fsMesh.draw();
            src = &bloomChain[i].color;
        }
        bloomUp.use();
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        for (int i = (int)bloomChain.size() - 1; i > 0; i--) {
            bloomChain[i - 1].bind();
            bloomChain[i].color.bind(0);
            bloomUp.set("uSrc", 0);
            bloomUp.set("uTexel", vec2(1.f / bloomChain[i].w, 1.f / bloomChain[i].h));
            bloomUp.set("uRadius", 1.f);
            fsMesh.draw();
        }
        glDisable(GL_BLEND);
    }
    // Blurred copy for depth of field (quarter resolution, bilinear)
    bool doDof = settings.dof && (env.dofAmount > 0.01f || env.tiltShift > 0.01f);
    if (doDof) {
        // 13-tap filtered downsample of the scene (keeps HDR scale), then bilinear upscale in composite
        blurRT.bind();
        bloomDown.use();
        sceneRT.color.bind(0);
        bloomDown.set("uSrc", 0);
        bloomDown.set("uTexel", vec2(2.f / sceneRT.w, 2.f / sceneRT.h));
        bloomDown.set("uThreshold", -1.f);
        bloomDown.set("uHdrScale", 1.f);
        fsMesh.draw();
    }
    // Composite into LDR (or directly to the screen when FXAA is off)
    bool doFxaa = settings.fxaa && ldrRT.fbo;
    if (doFxaa) ldrRT.bind();
    else { glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0, 0, width, height); }
    composite.use();
    sceneRT.color.bind(0); composite.set("uScene", 0);
    if (doBloom) bloomChain[0].color.bind(1); else { glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0); }
    composite.set("uBloom", 1);
    blurRT.color.bind(2); composite.set("uBlur", 2);
    sceneRT.depth.bind(3); composite.set("uDepth", 3);
    composite.set("uPost0", vec4(env.exposure, doBloom ? env.bloomIntensity : 0.f, env.vignette, settings.chromatic ? env.chromatic + 0.25f : env.chromatic));
    composite.set("uPost1", vec4(env.saturation, env.contrast, env.temperature, env.grain));
    composite.set("uLift", vec4(env.lift, doDof ? env.dofAmount : 0.f));
    composite.set("uGamma", vec4(env.gamma, env.dofFocus));
    composite.set("uGain", vec4(env.gain, doDof ? env.tiltShift : 0.f));
    composite.set("uFade", vec4(env.fadeColor, env.fade));
    composite.set("uProj", vec4(camera.zNear, camera.zFar, hdrScale, env.damagePulse));
    composite.set("uTime", std::fmod(env.time, 100.f));
    fsMesh.draw();
    if (doFxaa) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        fxaa.use();
        ldrRT.color.bind(0); fxaa.set("uSrc", 0);
        fxaa.set("uTexel", vec2(1.f / ldrRT.w, 1.f / ldrRT.h));
        fsMesh.draw();
    }
    stats.drawCalls += 4 + (int)bloomChain.size() * 2;
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderToTarget(RenderTarget& rt, const Camera& cam, const Environment& e, const std::vector<DrawItem>& list, vec4 clear) {
    // Render in HDR then tone-map into the (LDR) destination so icons/portraits match the game look.
    Environment saveEnv = env; Camera saveCam = camera;
    env = e; camera = cam;
    std::vector<PointLight> saveLights; saveLights.swap(lights);
    Environment eNoOcc = e; eNoOcc.grassPushRadius = 0;  // no see-through fading in previews
    RenderTarget hdr;
    if (!hdr.create(rt.w, rt.h, hdrFormat, true, false)) { lights.swap(saveLights); env = saveEnv; camera = saveCam; return; }
    updateFrameUBO(cam, eNoOcc, mat4(), false);
    hdr.bind();
    glClearColor(clear.x / hdrScale, clear.y / hdrScale, clear.z / hdrScale, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glEnable(GL_CULL_FACE); glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
    noiseTex.bind(1);
    for (auto& d : list) drawItem(d, false, false);
    glCullFace(GL_FRONT);
    for (auto& d : list) if (d.outline) drawItem(d, false, true);
    glCullFace(GL_BACK);
    // tone map
    rt.bind();
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    composite.use();
    hdr.color.bind(0); composite.set("uScene", 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0); composite.set("uBloom", 1);
    hdr.color.bind(2); composite.set("uBlur", 2);
    hdr.depth.bind(3); composite.set("uDepth", 3);
    composite.set("uPost0", vec4(e.exposure, 0.f, 0.f, 0.f));
    composite.set("uPost1", vec4(e.saturation, e.contrast, 0.f, 0.f));
    composite.set("uLift", vec4(0, 0, 0, 0)); composite.set("uGamma", vec4(1, 1, 1, 10)); composite.set("uGain", vec4(1, 1, 1, 0));
    composite.set("uFade", vec4(0, 0, 0, 0));
    composite.set("uProj", vec4(cam.zNear, cam.zFar, hdrScale, 0));
    composite.set("uTime", 0.f);
    composite.set("uAlphaFromScene", 1.f);
    fsMesh.draw();
    composite.set("uAlphaFromScene", 0.f);
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    hdr.destroy();
    lights.swap(saveLights);
    env = saveEnv; camera = saveCam;
}

}  // namespace agr
