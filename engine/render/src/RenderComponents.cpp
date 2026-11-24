#include "agr/render/RenderComponents.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string_view>

#include "agr/scene/Reflection.hpp"

namespace agr {

math::Vec2 orthographicHalfExtents(ScaleMode mode, float size, float refAspect, float aspect) {
    size = std::max(size, 1e-4f);
    refAspect = std::max(refAspect, 1e-3f);
    aspect = std::max(aspect, 1e-3f);
    float halfH = size;
    switch (mode) {
        case ScaleMode::FixedHeight: halfH = size; break;
        case ScaleMode::FixedWidth: halfH = size * refAspect / aspect; break;
        case ScaleMode::FitInside: halfH = std::max(size, size * refAspect / aspect); break;
        case ScaleMode::Crop: halfH = std::min(size, size * refAspect / aspect); break;
    }
    return {halfH * aspect, halfH};
}

const std::array<u8, 47>& blobTileMasks() {
    static const std::array<u8, 47> masks = [] {
        std::array<u8, 47> out{};
        usize n = 0;
        for (u32 m = 0; m < 256; ++m) {
            const bool N = m & 1, E = m & 4, S = m & 16, W = m & 64;
            if ((m & 2) && !(N && E)) continue;  // a corner without both of its sides is not a distinct tile
            if ((m & 8) && !(S && E)) continue;
            if ((m & 32) && !(S && W)) continue;
            if ((m & 128) && !(N && W)) continue;
            out[n++] = static_cast<u8>(m);
        }
        return out;
    }();
    return masks;
}

TileRules TileRules::parse(const TilemapComponent& tm, usize frameCount) {
    TileRules r;
    auto toInt = [](std::string_view s, i32& out) {
        while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
        if (s.empty()) return false;
        char* end = nullptr;
        const std::string tmp(s);
        const long v = std::strtol(tmp.c_str(), &end, 10);
        if (end == tmp.c_str()) return false;
        out = static_cast<i32>(v);
        return true;
    };
    auto split = [](std::string_view s, char sep, auto&& fn) {
        while (!s.empty()) {
            const usize p = s.find(sep);
            fn(s.substr(0, p));
            if (p == std::string_view::npos) break;
            s.remove_prefix(p + 1);
        }
    };
    if (!tm.autotile.empty()) {
        r.group.assign(frameCount, -1);
        r.blob.assign(frameCount, 0);
        split(tm.autotile, ';', [&](std::string_view e) {
            i32 base = 0;
            const usize colon = e.find(':');
            const bool blob = colon != std::string_view::npos && e.substr(colon + 1).find("47") != std::string_view::npos;
            const bool corners = colon != std::string_view::npos && e.substr(colon + 1).find('c') != std::string_view::npos;
            if (!toInt(colon == std::string_view::npos ? e : e.substr(0, colon), base) || base < 0) return;
            if (corners) r.cornerSets.push_back(base);
            const i32 count = blob ? 47 : 16;
            for (i32 k = 0; k < count; ++k)
                if (static_cast<usize>(base + k) < frameCount) {
                    r.group[static_cast<usize>(base + k)] = base;
                    r.blob[static_cast<usize>(base + k)] = blob ? 1 : 0;
                }
        });
        if (std::all_of(r.group.begin(), r.group.end(), [](i32 g) { return g < 0; })) r.group.clear();
    }
    if (!tm.animatedTiles.empty()) {
        r.anim.assign(frameCount, -1);
        split(tm.animatedTiles, ';', [&](std::string_view e) {
            const usize at = e.find('@');
            float fps = 8.0f;
            if (at != std::string_view::npos) {
                const std::string f(e.substr(at + 1));
                fps = std::strtof(f.c_str(), nullptr);
                e = e.substr(0, at);
            }
            std::vector<i32> seq;
            split(e, ',', [&](std::string_view f) {
                i32 v = 0;
                if (toInt(f, v) && v >= 0 && static_cast<usize>(v) < frameCount) seq.push_back(v);
            });
            if (seq.size() < 2 || fps <= 0.0f || r.anim[static_cast<usize>(seq[0])] >= 0) return;
            r.anim[static_cast<usize>(seq[0])] = static_cast<i32>(r.sequences.size());
            r.sequences.push_back(std::move(seq));
            r.fps.push_back(fps);
        });
        if (r.sequences.empty()) r.anim.clear();
    }
    return r;
}

i32 TileRules::resolve(const TilemapComponent& tm, u32 x, u32 y, double time) const {
    i32 id = tm.tileAt(x, y);
    if (id < 0) return id;
    bool cornered = false;
    for (const i32 set : cornerSets) {  // terrain transitions by corners
        auto in = [&](i64 nx, i64 ny) {
            if (nx < 0 || ny < 0) return false;
            const i32 n = tm.tileAt(static_cast<u32>(nx), static_cast<u32>(ny));
            return n >= 0 && static_cast<usize>(n) < group.size() && group[static_cast<usize>(n)] == set;
        };
        const i64 ix = x, iy = y;
        const bool self = in(ix, iy);
        const bool l = in(ix - 1, iy), r = in(ix + 1, iy), u = in(ix, iy + 1), d = in(ix, iy - 1);
        const int mask = ((self || l || u || in(ix - 1, iy + 1)) ? 1 : 0) | ((self || r || u || in(ix + 1, iy + 1)) ? 2 : 0) |
                         ((self || l || d || in(ix - 1, iy - 1)) ? 4 : 0) | ((self || r || d || in(ix + 1, iy - 1)) ? 8 : 0);
        if (mask != 0) {
            id = set + mask;
            cornered = true;
            break;
        }
    }
    if (!cornered && !group.empty() && static_cast<usize>(id) < group.size() && group[static_cast<usize>(id)] >= 0) {
        const i32 g = group[static_cast<usize>(id)];
        auto same = [&](i64 nx, i64 ny) {
            if (nx < 0 || ny < 0) return false;
            const i32 n = tm.tileAt(static_cast<u32>(nx), static_cast<u32>(ny));
            return n >= 0 && static_cast<usize>(n) < group.size() && group[static_cast<usize>(n)] == g;
        };
        const i64 ix = x, iy = y;
        if (blob[static_cast<usize>(id)]) {
            const bool n = same(ix, iy + 1), e = same(ix + 1, iy), so = same(ix, iy - 1), w = same(ix - 1, iy);
            u8 m = static_cast<u8>((n ? 1 : 0) | (e ? 4 : 0) | (so ? 16 : 0) | (w ? 64 : 0));
            if (n && e && same(ix + 1, iy + 1)) m |= 2;
            if (so && e && same(ix + 1, iy - 1)) m |= 8;
            if (so && w && same(ix - 1, iy - 1)) m |= 32;
            if (n && w && same(ix - 1, iy + 1)) m |= 128;
            const auto& masks = blobTileMasks();
            const auto it = std::lower_bound(masks.begin(), masks.end(), m);
            id = g + static_cast<i32>(it - masks.begin());
        } else {
            const i32 mask = (same(ix, iy + 1) ? 1 : 0) | (same(ix + 1, iy) ? 2 : 0) | (same(ix, iy - 1) ? 4 : 0) | (same(ix - 1, iy) ? 8 : 0);
            id = g + mask;
        }
    }
    if (!anim.empty() && static_cast<usize>(id) < anim.size() && anim[static_cast<usize>(id)] >= 0) {
        const usize a = static_cast<usize>(anim[static_cast<usize>(id)]);
        const auto& seq = sequences[a];
        const i64 step = static_cast<i64>(std::floor(std::max(time, 0.0) * static_cast<double>(fps[a])));
        id = seq[static_cast<usize>(step % static_cast<i64>(seq.size()))];
    }
    return id;
}

void registerRenderComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<CameraComponent>()) return;

    reg.add<CameraComponent>("Camera")
        .category("Rendering")
        .description("Renders the scene from this entity, looking down its -Z axis.")
        .field("projection", &CameraComponent::projection,
               {.enumEntries = {{"Perspective", 0}, {"Orthographic", 1}}})
        .field("fieldOfView", &CameraComponent::fieldOfView,
               {.flags = FieldFlag_Angle, .rangeMin = math::radians(1.0f), .rangeMax = math::radians(170.0f), .speed = 0.01f})
        .field("orthographicSize", &CameraComponent::orthographicSize, {.rangeMin = 0.01f, .rangeMax = 10000.0f})
        .field("nearPlane", &CameraComponent::nearPlane, {.rangeMin = 0.001f, .rangeMax = 1000.0f, .speed = 0.01f})
        .field("farPlane", &CameraComponent::farPlane, {.rangeMin = 0.01f, .rangeMax = 100000.0f, .speed = 1.0f})
        .field("clearColor", &CameraComponent::clearColor, {.flags = FieldFlag_Color})
        .field("primary", &CameraComponent::primary)
        .field("scaleMode", &CameraComponent::scaleMode,
               {.tooltip = "Orthographic: how the view adapts to screen shapes (phones, tablets)",
                .enumEntries = {{"FixedHeight", 0}, {"FixedWidth", 1}, {"FitInside", 2}, {"Crop", 3}}})
        .field("referenceAspect", &CameraComponent::referenceAspect,
               {.tooltip = "Width / height of the design area (16:9 = 1.778)", .rangeMin = 0.2f, .rangeMax = 5.0f, .speed = 0.01f});

    reg.add<DirectionalLightComponent>("DirectionalLight")
        .category("Rendering")
        .description("Light shining along the entity's forward (-Z) axis, like the sun. Casts shadows.")
        .field("color", &DirectionalLightComponent::color, {.flags = FieldFlag_Color})
        .field("intensity", &DirectionalLightComponent::intensity, {.rangeMin = 0.0f, .rangeMax = 20.0f, .speed = 0.01f})
        .field("castShadows", &DirectionalLightComponent::castShadows)
        .field("shadowDistance", &DirectionalLightComponent::shadowDistance,
               {.tooltip = "Shadows are drawn up to this distance from the camera (smaller = sharper)",
                .rangeMin = 1.0f, .rangeMax = 1000.0f, .speed = 0.1f})
        .field("shadowStrength", &DirectionalLightComponent::shadowStrength, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f});

    reg.add<PointLightComponent>("PointLight")
        .category("Rendering")
        .description("Light radiating in every direction, up to its range (bulbs, torches).")
        .field("color", &PointLightComponent::color, {.flags = FieldFlag_Color})
        .field("intensity", &PointLightComponent::intensity, {.rangeMin = 0.0f, .rangeMax = 1000.0f, .speed = 0.05f})
        .field("range", &PointLightComponent::range, {.rangeMin = 0.01f, .rangeMax = 1000.0f, .speed = 0.05f})
        .field("castShadows", &PointLightComponent::castShadows,
               {.tooltip = "Shadows in every direction (the nearest shadowed point light; 6 shadow views)"});

    reg.add<SpotLightComponent>("SpotLight")
        .category("Rendering")
        .description("Cone of light along the entity's forward (-Z) axis (flashlights, street lamps).")
        .field("color", &SpotLightComponent::color, {.flags = FieldFlag_Color})
        .field("intensity", &SpotLightComponent::intensity, {.rangeMin = 0.0f, .rangeMax = 1000.0f, .speed = 0.05f})
        .field("range", &SpotLightComponent::range, {.rangeMin = 0.01f, .rangeMax = 1000.0f, .speed = 0.05f})
        .field("innerAngle", &SpotLightComponent::innerAngle,
               {.flags = FieldFlag_Angle, .rangeMin = 0.0f, .rangeMax = math::radians(89.0f), .speed = 0.01f})
        .field("outerAngle", &SpotLightComponent::outerAngle,
               {.flags = FieldFlag_Angle, .rangeMin = math::radians(1.0f), .rangeMax = math::radians(89.0f), .speed = 0.01f})
        .field("castShadows", &SpotLightComponent::castShadows,
               {.tooltip = "Shadows in the cone (the 2 nearest shadowed spot lights; needs a profile with shadows)"});

    reg.add<EnvironmentComponent>("Environment")
        .category("Rendering")
        .description("Ambient light, sky, fog and exposure of the scene.")
        .field("ambientColor", &EnvironmentComponent::ambientColor,
               {.tooltip = "Flat ambient light (sky = CameraColor)", .flags = FieldFlag_Color})
        .field("ambientIntensity", &EnvironmentComponent::ambientIntensity, {.rangeMin = 0.0f, .rangeMax = 4.0f, .speed = 0.01f})
        .field("sky", &EnvironmentComponent::sky,
               {.tooltip = "Gradient: procedural sky and hemisphere ambient light from its colors; Image: an equirectangular "
                           "image lights the scene (diffuse + reflections)",
                .enumEntries = {{"CameraColor", 0}, {"Gradient", 1}, {"Image", 2}}})
        .field("skyTexture", &EnvironmentComponent::skyTexture,
               {.tooltip = "Image sky: equirectangular 2:1 image (.hdr for real light levels, or .png / .jpg)", .flags = FieldFlag_AssetPath})
        .field("skyIntensity", &EnvironmentComponent::skyIntensity, {.rangeMin = 0.0f, .rangeMax = 16.0f, .speed = 0.01f})
        .field("skyRotation", &EnvironmentComponent::skyRotation, {.tooltip = "Image sky: turn around the vertical axis", .flags = FieldFlag_Angle})
        .field("skyColor", &EnvironmentComponent::skyColor, {.flags = FieldFlag_Color})
        .field("horizonColor", &EnvironmentComponent::horizonColor, {.flags = FieldFlag_Color})
        .field("groundColor", &EnvironmentComponent::groundColor, {.flags = FieldFlag_Color})
        .field("sunDisc", &EnvironmentComponent::sunDisc, {.tooltip = "Draws the sun of the directional light in the gradient sky"})
        .field("fog", &EnvironmentComponent::fog)
        .field("fogColor", &EnvironmentComponent::fogColor, {.flags = FieldFlag_Color})
        .field("fogStart", &EnvironmentComponent::fogStart, {.rangeMin = 0.0f, .rangeMax = 100000.0f, .speed = 0.1f})
        .field("fogEnd", &EnvironmentComponent::fogEnd, {.rangeMin = 0.0f, .rangeMax = 100000.0f, .speed = 0.1f})
        .field("exposure", &EnvironmentComponent::exposure, {.rangeMin = 0.01f, .rangeMax = 16.0f, .speed = 0.01f});

    reg.add<ParticleEmitter3DComponent>("ParticleEmitter3D")
        .category("Rendering")
        .description("3D particles (camera-facing): fire, smoke, sparks, magic, rain... (presets in the inspector).")
        .field("emitting", &ParticleEmitter3DComponent::emitting)
        .field("loop", &ParticleEmitter3DComponent::loop)
        .field("duration", &ParticleEmitter3DComponent::duration, {.rangeMin = 0.01f, .rangeMax = 600.0f, .speed = 0.05f})
        .field("rate", &ParticleEmitter3DComponent::rate, {.tooltip = "Particles per second", .rangeMin = 0.0f, .rangeMax = 10000.0f, .speed = 0.5f})
        .field("burst", &ParticleEmitter3DComponent::burst, {.tooltip = "Particles at the start of each cycle", .rangeMin = 0, .rangeMax = 10000, .speed = 1})
        .field("maxParticles", &ParticleEmitter3DComponent::maxParticles, {.rangeMin = 1, .rangeMax = 100000, .speed = 1})
        .field("lifetimeMin", &ParticleEmitter3DComponent::lifetimeMin, {.rangeMin = 0.01f, .rangeMax = 60.0f, .speed = 0.01f})
        .field("lifetimeMax", &ParticleEmitter3DComponent::lifetimeMax, {.rangeMin = 0.01f, .rangeMax = 60.0f, .speed = 0.01f})
        .field("speedMin", &ParticleEmitter3DComponent::speedMin, {.rangeMin = 0.0f, .rangeMax = 200.0f, .speed = 0.05f})
        .field("speedMax", &ParticleEmitter3DComponent::speedMax, {.rangeMin = 0.0f, .rangeMax = 200.0f, .speed = 0.05f})
        .field("emitFromSurface", &ParticleEmitter3DComponent::emitFromSurface, {.tooltip = "Sphere / box: start on the surface"})
        .field("alignToVelocity", &ParticleEmitter3DComponent::alignToVelocity, {.tooltip = "Mesh particles point along their motion"})
        .field("shape", &ParticleEmitter3DComponent::shape, {.enumEntries = {{"Point", 0}, {"Sphere", 1}, {"Box", 2}, {"Cone", 3}, {"Ring", 4}}})
        .field("shapeRadius", &ParticleEmitter3DComponent::shapeRadius, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("boxSize", &ParticleEmitter3DComponent::boxSize, {.speed = 0.05f})
        .field("coneAngle", &ParticleEmitter3DComponent::coneAngle, {.flags = FieldFlag_Angle, .rangeMin = 0.0f, .rangeMax = math::kPi, .speed = 0.01f})
        .field("gravity", &ParticleEmitter3DComponent::gravity, {.speed = 0.05f})
        .field("drag", &ParticleEmitter3DComponent::drag, {.rangeMin = 0.0f, .rangeMax = 20.0f, .speed = 0.01f})
        .field("startColor", &ParticleEmitter3DComponent::startColor, {.flags = FieldFlag_Color})
        .field("endColor", &ParticleEmitter3DComponent::endColor, {.flags = FieldFlag_Color})
        .field("startSize", &ParticleEmitter3DComponent::startSize, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("endSize", &ParticleEmitter3DComponent::endSize, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("angularVelocity", &ParticleEmitter3DComponent::angularVelocity, {.flags = FieldFlag_Angle, .speed = 0.01f})
        .field("texture", &ParticleEmitter3DComponent::texture, {.tooltip = "Empty = soft round particle", .flags = FieldFlag_AssetPath})
        .field("curves", &ParticleEmitter3DComponent::curves, {.tooltip = "Size curve and color gradient over the lifetime (.agrparticle)", .flags = FieldFlag_AssetPath})
        .field("flipbookColumns", &ParticleEmitter3DComponent::flipbookColumns, {.tooltip = "Frames of the texture across", .rangeMin = 1.0f, .rangeMax = 16.0f})
        .field("flipbookRows", &ParticleEmitter3DComponent::flipbookRows, {.rangeMin = 1.0f, .rangeMax = 16.0f})
        .field("flipbookCycles", &ParticleEmitter3DComponent::flipbookCycles, {.tooltip = "Times the frames play during a life", .rangeMin = 0.0f, .rangeMax = 32.0f, .speed = 0.05f})
        .field("velocityStretch", &ParticleEmitter3DComponent::velocityStretch, {.tooltip = "Stretch along the motion (seconds of speed): sparks, rain", .rangeMin = 0.0f, .rangeMax = 2.0f, .speed = 0.005f})
        .field("collisions", &ParticleEmitter3DComponent::collisions, {.tooltip = "Bounce on the ground plane and ParticleCollider shapes (world space only)"})
        .field("groundPlane", &ParticleEmitter3DComponent::groundPlane)
        .field("groundHeight", &ParticleEmitter3DComponent::groundHeight, {.speed = 0.01f})
        .field("bounce", &ParticleEmitter3DComponent::bounce, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("collisionFriction", &ParticleEmitter3DComponent::collisionFriction, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("dieOnCollision", &ParticleEmitter3DComponent::dieOnCollision)
        .field("affectedByAttractors", &ParticleEmitter3DComponent::affectedByAttractors)
        .field("turbulence", &ParticleEmitter3DComponent::turbulence, {.tooltip = "Swirling noise force (units / s^2), 0 = off", .rangeMin = -100.0f, .rangeMax = 100.0f, .speed = 0.05f})
        .field("turbulenceScale", &ParticleEmitter3DComponent::turbulenceScale, {.tooltip = "Size of the swirls (world units)", .rangeMin = 0.01f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("turbulenceSpeed", &ParticleEmitter3DComponent::turbulenceSpeed, {.tooltip = "How fast the swirls change", .rangeMin = 0.0f, .rangeMax = 20.0f, .speed = 0.01f})
        .field("inheritVelocity", &ParticleEmitter3DComponent::inheritVelocity, {.tooltip = "Fraction of the emitter's velocity given to new particles (world space)", .rangeMin = -2.0f, .rangeMax = 2.0f, .speed = 0.01f})
        .field("lit", &ParticleEmitter3DComponent::lit, {.tooltip = "Lit by ambient, the directional light and point / spot lights in range"})
        .field("meshParticles", &ParticleEmitter3DComponent::meshParticles, {.tooltip = "Each particle is a small lit 3D mesh (debris, rocks, coins)"})
        .field("particleMesh", &ParticleEmitter3DComponent::particleMesh, {.enumEntries = {{"Cube", 0}, {"Sphere", 1}, {"Plane", 2}, {"Quad", 3}}})
        .field("particleModel", &ParticleEmitter3DComponent::particleModel, {.tooltip = "Imported model whose first mesh is used (overrides particleMesh)", .flags = FieldFlag_AssetPath})
        .field("meshMetallic", &ParticleEmitter3DComponent::meshMetallic, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("meshRoughness", &ParticleEmitter3DComponent::meshRoughness, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("meshCastShadows", &ParticleEmitter3DComponent::meshCastShadows)
        .field("subEmitter", &ParticleEmitter3DComponent::subEmitter, {.tooltip = "Emitter that bursts where a particle dies or hits something"})
        .field("gpu", &ParticleEmitter3DComponent::gpu,
               {.tooltip = "Simulated on the GPU: hundreds of thousands of particles (rain, snow, dust); no curves, collisions, attractors, lit, meshes"})
        .field("particleMaterial", &ParticleEmitter3DComponent::particleMaterial, {.tooltip = "Mesh particles: .agrmat material (textures, shader, transparency); the particle color tints it", .flags = FieldFlag_AssetPath})
        .field("subEmitOn", &ParticleEmitter3DComponent::subEmitOn, {.enumEntries = {{"Death", 0}, {"Collision", 1}}})
        .field("subEmitCount", &ParticleEmitter3DComponent::subEmitCount, {.rangeMin = 0.0f, .rangeMax = 1000.0f})
        .field("additive", &ParticleEmitter3DComponent::additive, {.tooltip = "Adds light (fire, sparks, magic)"})
        .field("worldSpace", &ParticleEmitter3DComponent::worldSpace, {.tooltip = "Particles stay where emitted when the emitter moves"})
        .field("seed", &ParticleEmitter3DComponent::seed, {.tooltip = "0 = random; otherwise the same particles every time"});

    reg.add<WaterComponent>("Water")
        .category("Rendering")
        .description("Animated water surface: waves, sky reflection, sun highlight, transparency.")
        .field("size", &WaterComponent::size, {.speed = 0.5f})
        .field("resolution", &WaterComponent::resolution, {.tooltip = "Grid cells per side", .rangeMin = 2, .rangeMax = 512, .speed = 1})
        .field("shallowColor", &WaterComponent::shallowColor, {.flags = FieldFlag_Color})
        .field("deepColor", &WaterComponent::deepColor, {.flags = FieldFlag_Color})
        .field("opacity", &WaterComponent::opacity, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("waveHeight", &WaterComponent::waveHeight, {.rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("waveLength", &WaterComponent::waveLength, {.rangeMin = 0.1f, .rangeMax = 200.0f, .speed = 0.05f})
        .field("waveSpeed", &WaterComponent::waveSpeed, {.rangeMin = 0.0f, .rangeMax = 20.0f, .speed = 0.01f})
        .field("waveDirection", &WaterComponent::waveDirection, {.flags = FieldFlag_Angle})
        .field("reflectivity", &WaterComponent::reflectivity, {.rangeMin = 0.0f, .rangeMax = 2.0f, .speed = 0.01f});

    reg.add<PostProcessComponent>("PostProcess")
        .category("Rendering")
        .description("Image effects: bloom, anti-aliasing (FXAA), tone mapping, color grading, vignette.")
        .field("bloom", &PostProcessComponent::bloom, {.tooltip = "Glow around bright areas (needs a graphics profile that allows it)"})
        .field("bloomIntensity", &PostProcessComponent::bloomIntensity, {.rangeMin = 0.0f, .rangeMax = 5.0f, .speed = 0.01f})
        .field("bloomThreshold", &PostProcessComponent::bloomThreshold, {.tooltip = "Brightness above which pixels glow", .rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("bloomRadius", &PostProcessComponent::bloomRadius, {.rangeMin = 0.25f, .rangeMax = 4.0f, .speed = 0.01f})
        .field("fxaa", &PostProcessComponent::fxaa, {.tooltip = "Smooths jagged edges (cheap anti-aliasing)"})
        .field("filmicToneMapping", &PostProcessComponent::filmicToneMapping, {.tooltip = "ACES filmic curve; off for flat 2D colors"})
        .field("contrast", &PostProcessComponent::contrast, {.rangeMin = 0.0f, .rangeMax = 3.0f, .speed = 0.01f})
        .field("saturation", &PostProcessComponent::saturation, {.rangeMin = 0.0f, .rangeMax = 3.0f, .speed = 0.01f})
        .field("colorFilter", &PostProcessComponent::colorFilter, {.flags = FieldFlag_Color})
        .field("vignette", &PostProcessComponent::vignette, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("ambientOcclusion", &PostProcessComponent::ambientOcclusion,
               {.tooltip = "Contact shadows in corners and under objects (SSAO, half resolution; MSAA off, FXAA instead)"})
        .field("aoRadius", &PostProcessComponent::aoRadius, {.rangeMin = 0.05f, .rangeMax = 5.0f, .speed = 0.01f})
        .field("aoIntensity", &PostProcessComponent::aoIntensity, {.rangeMin = 0.0f, .rangeMax = 2.0f, .speed = 0.01f})
        .field("heightFog", &PostProcessComponent::heightFog, {.tooltip = "Fog denser low down, thinning with height, lit around the sun"})
        .field("fogDensity", &PostProcessComponent::fogDensity, {.rangeMin = 0.0f, .rangeMax = 2.0f, .speed = 0.002f})
        .field("fogHeight", &PostProcessComponent::fogHeight, {.tooltip = "World height of the given density", .speed = 0.05f})
        .field("fogFalloff", &PostProcessComponent::fogFalloff, {.tooltip = "Thinning per world unit of height", .rangeMin = 0.0f, .rangeMax = 5.0f, .speed = 0.005f})
        .field("fogColor", &PostProcessComponent::fogColor, {.flags = FieldFlag_Color})
        .field("fogLightScatter", &PostProcessComponent::fogLightScatter, {.tooltip = "Halos of point / spot lights in the height fog", .rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("fogSunScatter", &PostProcessComponent::fogSunScatter, {.rangeMin = 0.0f, .rangeMax = 5.0f, .speed = 0.01f})
        .field("lightShafts", &PostProcessComponent::lightShafts, {.tooltip = "Rays of sunlight through gaps (god rays)"})
        .field("shaftIntensity", &PostProcessComponent::shaftIntensity, {.rangeMin = 0.0f, .rangeMax = 5.0f, .speed = 0.01f})
        .field("shaftDecay", &PostProcessComponent::shaftDecay, {.tooltip = "0.9 = short rays, 0.99 = long rays", .rangeMin = 0.5f, .rangeMax = 0.999f, .speed = 0.001f})
        .field("depthOfField", &PostProcessComponent::depthOfField, {.tooltip = "Lens blur in front of and behind the focus zone (bokeh)"})
        .field("focusDistance", &PostProcessComponent::focusDistance, {.tooltip = "World units from the camera", .rangeMin = 0.01f, .rangeMax = 10000.0f, .speed = 0.05f})
        .field("focusRange", &PostProcessComponent::focusRange, {.tooltip = "Depth of the sharp zone", .rangeMin = 0.01f, .rangeMax = 10000.0f, .speed = 0.05f})
        .field("dofBlur", &PostProcessComponent::dofBlur, {.tooltip = "Largest blur radius (fraction of the image height)", .rangeMin = 0.0f, .rangeMax = 0.05f, .speed = 0.0005f})
        .field("dofNear", &PostProcessComponent::dofNear)
        .field("dofFar", &PostProcessComponent::dofFar)
        .field("motionBlur", &PostProcessComponent::motionBlur, {.tooltip = "Blur along the camera's motion"})
        .field("motionBlurIntensity", &PostProcessComponent::motionBlurIntensity, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("shader", &PostProcessComponent::shader, {.tooltip = "Custom screen effect (.agrshader with post())", .flags = FieldFlag_AssetPath})
        .field("temporalAA", &PostProcessComponent::temporalAA, {.tooltip = "Temporal anti-aliasing: smoother edges, accumulated over frames"})
        .field("taaBlend", &PostProcessComponent::taaBlend, {.tooltip = "Weight of the new frame", .rangeMin = 0.02f, .rangeMax = 1.0f, .speed = 0.005f})
        .field("screenSpaceReflections", &PostProcessComponent::screenSpaceReflections,
               {.tooltip = "Smooth / metallic surfaces reflect what is on screen (wet floors, metal)"})
        .field("ssrIntensity", &PostProcessComponent::ssrIntensity, {.rangeMin = 0.0f, .rangeMax = 2.0f, .speed = 0.01f})
        .field("ssrMaxDistance", &PostProcessComponent::ssrMaxDistance, {.tooltip = "World units a reflected ray travels at most", .rangeMin = 0.1f, .rangeMax = 1000.0f, .speed = 0.05f})
        .field("ssrThickness", &PostProcessComponent::ssrThickness, {.tooltip = "Depth of the surfaces a ray can hit", .rangeMin = 0.01f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("autoExposure", &PostProcessComponent::autoExposure, {.tooltip = "Brightness adapts to the light seen (HDR)"})
        .field("exposureTarget", &PostProcessComponent::exposureTarget, {.rangeMin = 0.01f, .rangeMax = 4.0f, .speed = 0.01f})
        .field("exposureMin", &PostProcessComponent::exposureMin, {.rangeMin = 0.01f, .rangeMax = 64.0f, .speed = 0.01f})
        .field("exposureMax", &PostProcessComponent::exposureMax, {.rangeMin = 0.01f, .rangeMax = 64.0f, .speed = 0.01f})
        .field("exposureSpeed", &PostProcessComponent::exposureSpeed, {.tooltip = "Adaptation speed (per second)", .rangeMin = 0.01f, .rangeMax = 100.0f, .speed = 0.05f});
    reg.add<MeshRendererComponent>("MeshRenderer")
        .category("Rendering")
        .description("Draws a built-in mesh or a mesh of an imported model (glTF, OBJ, FBX) with PBR materials.")
        .field("mesh", &MeshRendererComponent::mesh,
               {.tooltip = "Built-in mesh (used when no model is set)", .enumEntries = {{"Cube", 0}, {"Sphere", 1}, {"Plane", 2}, {"Quad", 3}}})
        .field("model", &MeshRendererComponent::model,
               {.tooltip = "Imported model: assets://models/robot.glb", .flags = FieldFlag_AssetPath})
        .field("meshIndex", &MeshRendererComponent::meshIndex, {.tooltip = "Mesh of the model", .rangeMin = 0.0f, .rangeMax = 100000.0f})
        .field("skin", &MeshRendererComponent::skin, {.tooltip = "Skin of the model (-1 = none): the mesh follows the joints of its skeleton"})
        .field("material", &MeshRendererComponent::material,
               {.tooltip = "Optional .agrmat material replacing the mesh materials", .flags = FieldFlag_AssetPath})
        .field("texture", &MeshRendererComponent::texture,
               {.tooltip = "Built-in material: base color texture", .flags = FieldFlag_AssetPath})
        .field("color", &MeshRendererComponent::color, {.tooltip = "Tint applied to any material", .flags = FieldFlag_Color})
        .field("uvScale", &MeshRendererComponent::uvScale, {.speed = 0.01f})
        .field("metallic", &MeshRendererComponent::metallic,
               {.tooltip = "Built-in material", .rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("roughness", &MeshRendererComponent::roughness,
               {.tooltip = "Built-in material", .rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("morph", &MeshRendererComponent::morph,
               {.tooltip = "Morph target (blend shape) weights 0..3 of an imported mesh", .rangeMin = -1.0f, .rangeMax = 2.0f, .speed = 0.01f})
        .field("morph2", &MeshRendererComponent::morph2, {.tooltip = "Morph target weights 4..7", .rangeMin = -1.0f, .rangeMax = 2.0f, .speed = 0.01f})
        .field("castShadows", &MeshRendererComponent::castShadows)
        .field("visible", &MeshRendererComponent::visible)
        .field("lodBias", &MeshRendererComponent::lodBias,
               {.tooltip = "Automatic LOD of imported models: > 1 keeps detail longer, 0 = always full detail", .rangeMin = 0.0f, .rangeMax = 8.0f,
                .speed = 0.01f})
        .field("visibilityRange", &MeshRendererComponent::visibilityRange,
               {.tooltip = "Hidden beyond this distance from the camera, shadows included (0 = always drawn)", .rangeMin = 0.0f,
                .rangeMax = 100000.0f, .speed = 0.5f})
        .field("toon", &MeshRendererComponent::toon,
               {.tooltip = "Cartoon lighting for every material of the mesh: light bands, tinted shadows, rim light"})
        .field("outlineWidth", &MeshRendererComponent::outlineWidth,
               {.tooltip = "Outline in pixels (0: the material's). Best on smooth meshes", .rangeMin = 0.0f, .rangeMax = 16.0f, .speed = 0.05f})
        .field("outlineColor", &MeshRendererComponent::outlineColor, {.flags = FieldFlag_Color});

    reg.add<BlobShadowComponent>("BlobShadow")
        .category("Rendering")
        .description("Soft round shadow on the ground under the entity (cheap, readable, mobile-friendly).")
        .field("radius", &BlobShadowComponent::radius, {.rangeMin = 0.01f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("opacity", &BlobShadowComponent::opacity, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("softness", &BlobShadowComponent::softness, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("groundHeight", &BlobShadowComponent::groundHeight, {.tooltip = "World height of the ground", .speed = 0.01f})
        .field("fadeHeight", &BlobShadowComponent::fadeHeight,
               {.tooltip = "Height above the ground where the shadow has faded out", .rangeMin = 0.01f, .rangeMax = 1000.0f, .speed = 0.05f})
        .field("color", &BlobShadowComponent::color, {.flags = FieldFlag_Color});

    reg.add<Skeleton2DComponent>("Skeleton2D")
        .category("2D")
        .description("Root of a 2D skeleton: its Bone2D children deform the Skin2D parts below it.")
        .field("showBones", &Skeleton2DComponent::showBones);
    reg.add<Bone2DComponent>("Bone2D")
        .category("2D")
        .description("Bone of a 2D skeleton, pointing along +X. Its pose in edit mode is the rest pose of the skins.")
        .field("length", &Bone2DComponent::length, {.rangeMin = 0.001f, .rangeMax = 1000.0f, .speed = 0.01f})
        .field("restSet", &Bone2DComponent::restSet, {.flags = FieldFlag_Hidden})
        .field("restPosition", &Bone2DComponent::restPosition, {.flags = FieldFlag_Hidden})
        .field("restRotation", &Bone2DComponent::restRotation, {.flags = FieldFlag_Hidden})
        .field("restScale", &Bone2DComponent::restScale, {.flags = FieldFlag_Hidden});
    reg.add<Skin2DComponent>("Skin2D")
        .category("2D")
        .description("Picture deformed by the bones of the enclosing Skeleton2D (smooth bending).")
        .field("texture", &Skin2DComponent::texture, {.flags = FieldFlag_AssetPath})
        .field("normalMap", &Skin2DComponent::normalMap, {.tooltip = "Normal map (lit skins)", .flags = FieldFlag_AssetPath})
        .field("color", &Skin2DComponent::color, {.flags = FieldFlag_Color})
        .field("size", &Skin2DComponent::size, {.rangeMin = 0.0f, .speed = 0.01f})
        .field("pivot", &Skin2DComponent::pivot, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("uvRect", &Skin2DComponent::uvRect, {.speed = 0.001f})
        .field("columns", &Skin2DComponent::columns, {.tooltip = "Grid cells across: more = smoother bends", .rangeMin = 1.0f, .rangeMax = 64.0f})
        .field("rows", &Skin2DComponent::rows, {.rangeMin = 1.0f, .rangeMax = 64.0f})
        .field("stiffness", &Skin2DComponent::stiffness,
               {.tooltip = "Higher = rigid parts and sharper joints, lower = soft bends", .rangeMin = 0.5f, .rangeMax = 16.0f, .speed = 0.05f})
        .field("sortingLayer", &Skin2DComponent::sortingLayer)
        .field("orderInLayer", &Skin2DComponent::orderInLayer)
        .field("pixelArt", &Skin2DComponent::pixelArt)
        .field("lit", &Skin2DComponent::lit)
        .field("visible", &Skin2DComponent::visible);

    reg.add<ParticleColliderComponent>("ParticleCollider")
        .category("Effects")
        .description("Obstacle the particles of emitters with collisions bounce on (sphere or box).")
        .field("shape", &ParticleColliderComponent::shape, {.enumEntries = {{"Sphere", 0}, {"Box", 1}}})
        .field("size", &ParticleColliderComponent::size, {.tooltip = "Box size, or sphere diameter (x)", .rangeMin = 0.0f, .speed = 0.01f});
    reg.add<ParticleAttractorComponent>("ParticleAttractor")
        .category("Effects")
        .description("Pulls (strength > 0) or pushes (< 0) nearby particles.")
        .field("strength", &ParticleAttractorComponent::strength, {.speed = 0.05f})
        .field("radius", &ParticleAttractorComponent::radius, {.rangeMin = 0.0f, .rangeMax = 1000.0f, .speed = 0.05f})
        .field("enabled", &ParticleAttractorComponent::enabled);
    reg.add<TrailComponent>("Trail")
        .category("Effects")
        .description("Ribbon left behind the entity as it moves (projectiles, swords, dashes).")
        .field("emitting", &TrailComponent::emitting)
        .field("lifetime", &TrailComponent::lifetime, {.rangeMin = 0.01f, .rangeMax = 30.0f, .speed = 0.01f})
        .field("minDistance", &TrailComponent::minDistance, {.rangeMin = 0.001f, .rangeMax = 10.0f, .speed = 0.005f})
        .field("width", &TrailComponent::width, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("color", &TrailComponent::color, {.flags = FieldFlag_Color})
        .field("curves", &TrailComponent::curves, {.tooltip = "Width curve and color gradient over the age (.agrparticle)", .flags = FieldFlag_AssetPath})
        .field("texture", &TrailComponent::texture, {.flags = FieldFlag_AssetPath})
        .field("additive", &TrailComponent::additive)
        .field("sortingLayer", &TrailComponent::sortingLayer)
        .field("orderInLayer", &TrailComponent::orderInLayer)
        .field("maxPoints", &TrailComponent::maxPoints, {.rangeMin = 2.0f, .rangeMax = 4096.0f});

    reg.add<ReflectionProbeComponent>("ReflectionProbe")
        .category("Rendering")
        .description("Captures the scene around it: shiny surfaces inside its box reflect the surroundings (rooms, metal, water).")
        .field("size", &ReflectionProbeComponent::size, {.tooltip = "Box of influence (full size)", .speed = 0.05f})
        .field("intensity", &ReflectionProbeComponent::intensity, {.rangeMin = 0.0f, .rangeMax = 4.0f, .speed = 0.01f})
        .field("blendDistance", &ReflectionProbeComponent::blendDistance,
               {.tooltip = "Fade-in distance from the box faces", .rangeMin = 0.0f, .rangeMax = 50.0f, .speed = 0.05f})
        .field("boxProjection", &ReflectionProbeComponent::boxProjection, {.tooltip = "Line reflections up with the walls of a room the size of the box"})
        .field("update", &ReflectionProbeComponent::update, {.tooltip = "Once: when added or moved. Always: refreshed continuously (6 frames each)"})
        .field("maxDistance", &ReflectionProbeComponent::maxDistance, {.rangeMin = 1.0f, .rangeMax = 5000.0f, .speed = 1.0f})
        .field("enabled", &ReflectionProbeComponent::enabled)
        .field("diffuseGI", &ReflectionProbeComponent::diffuseGI, {.tooltip = "Indirect diffuse light from the capture (global illumination), 0 = off", .rangeMin = 0.0f, .rangeMax = 4.0f, .speed = 0.01f});

    reg.add<DecalComponent>("Decal")
        .category("Rendering")
        .description("Projects an image onto the surfaces inside its box (impacts, stains, road marks). Projects along local -Y.")
        .field("texture", &DecalComponent::texture, {.tooltip = "Color + alpha image (empty: a plain square)", .flags = FieldFlag_AssetPath})
        .field("color", &DecalComponent::color, {.tooltip = "Tint; alpha = opacity", .flags = FieldFlag_Color})
        .field("size", &DecalComponent::size, {.tooltip = "Box size: x, z = image, y = projection depth", .speed = 0.01f})
        .field("normalFade", &DecalComponent::normalFade,
               {.tooltip = "0: every slope; 1: only surfaces facing the decal", .rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("upperFade", &DecalComponent::upperFade, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("lowerFade", &DecalComponent::lowerFade, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("emission", &DecalComponent::emission, {.tooltip = "Glow strength (adds light)", .rangeMin = 0.0f, .rangeMax = 16.0f, .speed = 0.01f})
        .field("enabled", &DecalComponent::enabled);

    reg.add<OccluderComponent>("Occluder")
        .category("Rendering")
        .description("Hides the objects behind it from the renderer (occlusion culling). Keep it inside the visible geometry.")
        .field("shape", &OccluderComponent::shape, {.enumEntries = {{"Box", 0}, {"Quad", 1}}})
        .field("size", &OccluderComponent::size, {.tooltip = "Full size in local units (Quad: x and y)", .rangeMin = 0.0f, .speed = 0.01f})
        .field("enabled", &OccluderComponent::enabled);

    reg.add<SpriteRendererComponent>("SpriteRenderer")
        .category("Rendering")
        .description("Textured quad for 2D games, batched automatically.")
        .field("texture", &SpriteRendererComponent::texture, {.tooltip = "Asset URI, e.g. assets://sprites/hero.png", .flags = FieldFlag_AssetPath})
        .field("color", &SpriteRendererComponent::color, {.flags = FieldFlag_Color})
        .field("size", &SpriteRendererComponent::size, {.speed = 0.01f})
        .field("pivot", &SpriteRendererComponent::pivot, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("uvRect", &SpriteRendererComponent::uvRect, {.tooltip = "u0, v0, u1, v1 — region of a sprite sheet", .speed = 0.001f})
        .field("flipX", &SpriteRendererComponent::flipX)
        .field("flipY", &SpriteRendererComponent::flipY)
        .field("sortingLayer", &SpriteRendererComponent::sortingLayer, {.speed = 1.0f})
        .field("orderInLayer", &SpriteRendererComponent::orderInLayer, {.speed = 1.0f})
        .field("pixelArt", &SpriteRendererComponent::pixelArt)
        .field("visible", &SpriteRendererComponent::visible)
        .field("sheet", &SpriteRendererComponent::sheet, {.tooltip = "Sprite sheet (.agrsheet): texture and frames", .flags = FieldFlag_AssetPath})
        .field("frame", &SpriteRendererComponent::frame, {.tooltip = "Frame index in the sheet (-1 = none)", .rangeMin = -1, .rangeMax = 100000, .speed = 0.2f})
        .field("lit", &SpriteRendererComponent::lit, {.tooltip = "Lit by Light2D components"})
        .field("shader", &SpriteRendererComponent::shader, {.tooltip = "Sprite shader (.agrshader with void sprite(inout Sprite s))", .flags = FieldFlag_AssetPath})
        .field("normalMap", &SpriteRendererComponent::normalMap,
               {.tooltip = "Normal map of the texture (lit sprites): the lights shade its relief", .flags = FieldFlag_AssetPath})
        .field("specularMap", &SpriteRendererComponent::specularMap,
               {.tooltip = "With a normal map: highlights (rgb = strength, alpha = glossiness)", .flags = FieldFlag_AssetPath});

    reg.add<TilemapComponent>("Tilemap")
        .category("2D")
        .description("Grid of tiles from a tileset sprite sheet. Paint it with the Tilemap tool of the editor.")
        .field("tileset", &TilemapComponent::tileset, {.tooltip = "Sprite sheet whose frames are the tiles", .flags = FieldFlag_AssetPath})
        .field("cellSize", &TilemapComponent::cellSize, {.speed = 0.01f})
        .field("width", &TilemapComponent::width, {.rangeMin = 1, .rangeMax = 4096, .speed = 0.2f})
        .field("height", &TilemapComponent::height, {.rangeMin = 1, .rangeMax = 4096, .speed = 0.2f})
        .field("color", &TilemapComponent::color, {.flags = FieldFlag_Color})
        .field("sortingLayer", &TilemapComponent::sortingLayer, {.speed = 1.0f})
        .field("orderInLayer", &TilemapComponent::orderInLayer, {.speed = 1.0f})
        .field("lit", &TilemapComponent::lit)
        .field("normalMap", &TilemapComponent::normalMap, {.tooltip = "Normal map laid out like the tileset texture (lit tilemaps)", .flags = FieldFlag_AssetPath})
        .field("animatedTiles", &TilemapComponent::animatedTiles, {.tooltip = "Animated tiles: \"f0,f1,f2@fps;...\" (cells holding f0 cycle)"})
        .field("autotile", &TilemapComponent::autotile,
               {.tooltip = "Autotile groups: \"F;G\" — 16 frames from F chosen by the neighbours (+y=1, +x=2, -y=4, -x=8)"})
        .field("dataWidth", &TilemapComponent::dataWidth, {.flags = FieldFlag_Hidden})
        .field("tiles", &TilemapComponent::tiles, {.flags = FieldFlag_Hidden});

    reg.add<Light2DComponent>("Light2D")
        .category("2D")
        .description("Point light for sprites and tilemaps marked 'lit'.")
        .field("color", &Light2DComponent::color, {.flags = FieldFlag_Color})
        .field("intensity", &Light2DComponent::intensity, {.rangeMin = 0.0f, .rangeMax = 20.0f, .speed = 0.01f})
        .field("radius", &Light2DComponent::radius, {.rangeMin = 0.01f, .rangeMax = 1000.0f, .speed = 0.05f})
        .field("falloff", &Light2DComponent::falloff, {.rangeMin = 0.1f, .rangeMax = 8.0f, .speed = 0.01f})
        .field("castShadows", &Light2DComponent::castShadows, {.tooltip = "Shadows of LightOccluder2D shapes (8 shadowed lights at most)"})
        .field("shadowSoftness", &Light2DComponent::shadowSoftness, {.rangeMin = 0.0f, .rangeMax = 4.0f, .speed = 0.01f})
        .field("shadowStrength", &Light2DComponent::shadowStrength, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("shadowPenetration", &Light2DComponent::shadowPenetration,
               {.tooltip = "How far the light enters occluders before the shadow starts (lit wall faces)", .rangeMin = 0.0f, .rangeMax = 100.0f,
                .speed = 0.01f})
        .field("height", &Light2DComponent::height,
               {.tooltip = "Height above the sprites for normal maps: low = grazing light, strong relief", .rangeMin = 0.0f, .rangeMax = 100.0f,
                .speed = 0.01f})
        .field("directional", &Light2DComponent::directional, {.tooltip = "Sun-like light along the entity's +X axis: no radius, no falloff"});

    reg.add<LightOccluder2DComponent>("LightOccluder2D")
        .category("2D")
        .description("Blocks 2D light: a box, or every non-empty cell of the entity's Tilemap.")
        .field("size", &LightOccluder2DComponent::size, {.speed = 0.01f})
        .field("offset", &LightOccluder2DComponent::offset, {.speed = 0.01f});

    reg.add<ParallaxComponent>("Parallax")
        .category("2D")
        .description("Scrolls this entity and its children slower or faster than the world (backgrounds).")
        .field("factor", &ParallaxComponent::factor, {.tooltip = "1 = moves with the world, 0 = stays with the camera", .speed = 0.01f});

    reg.add<ParticleEmitter2DComponent>("ParticleEmitter2D")
        .category("2D")
        .description("Emits and simulates 2D particles (fire, smoke, sparks, magic...).")
        .field("emitting", &ParticleEmitter2DComponent::emitting)
        .field("loop", &ParticleEmitter2DComponent::loop)
        .field("duration", &ParticleEmitter2DComponent::duration, {.rangeMin = 0.05f, .rangeMax = 600.0f, .speed = 0.01f})
        .field("rate", &ParticleEmitter2DComponent::rate, {.rangeMin = 0.0f, .rangeMax = 10000.0f, .speed = 0.5f})
        .field("burst", &ParticleEmitter2DComponent::burst, {.rangeMin = 0, .rangeMax = 100000, .speed = 0.5f})
        .field("maxParticles", &ParticleEmitter2DComponent::maxParticles, {.rangeMin = 1, .rangeMax = 100000, .speed = 1.0f})
        .field("lifetimeMin", &ParticleEmitter2DComponent::lifetimeMin, {.rangeMin = 0.01f, .rangeMax = 60.0f, .speed = 0.01f})
        .field("lifetimeMax", &ParticleEmitter2DComponent::lifetimeMax, {.rangeMin = 0.01f, .rangeMax = 60.0f, .speed = 0.01f})
        .field("speedMin", &ParticleEmitter2DComponent::speedMin, {.speed = 0.01f})
        .field("speedMax", &ParticleEmitter2DComponent::speedMax, {.speed = 0.01f})
        .field("direction", &ParticleEmitter2DComponent::direction, {.flags = FieldFlag_Angle, .speed = 0.01f})
        .field("spread", &ParticleEmitter2DComponent::spread, {.flags = FieldFlag_Angle, .rangeMin = 0.0f, .rangeMax = math::kPi, .speed = 0.01f})
        .field("spawnShape", &ParticleEmitter2DComponent::spawnShape, {.enumEntries = {{"Disc", 0}, {"Circle", 1}, {"Rectangle", 2}, {"Line", 3}}})
        .field("spawnSize", &ParticleEmitter2DComponent::spawnSize, {.tooltip = "Rectangle size / line length (x)", .speed = 0.01f})
        .field("spawnRadius", &ParticleEmitter2DComponent::spawnRadius, {.rangeMin = 0.0f, .rangeMax = 1000.0f, .speed = 0.01f})
        .field("gravity", &ParticleEmitter2DComponent::gravity, {.speed = 0.01f})
        .field("drag", &ParticleEmitter2DComponent::drag, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("startColor", &ParticleEmitter2DComponent::startColor, {.flags = FieldFlag_Color})
        .field("endColor", &ParticleEmitter2DComponent::endColor, {.flags = FieldFlag_Color})
        .field("startSize", &ParticleEmitter2DComponent::startSize, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.005f})
        .field("endSize", &ParticleEmitter2DComponent::endSize, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.005f})
        .field("angularVelocity", &ParticleEmitter2DComponent::angularVelocity, {.flags = FieldFlag_Angle, .speed = 0.01f})
        .field("texture", &ParticleEmitter2DComponent::texture, {.tooltip = "Empty = soft round particle", .flags = FieldFlag_AssetPath})
        .field("curves", &ParticleEmitter2DComponent::curves, {.tooltip = "Size curve and color gradient over the lifetime (.agrparticle)", .flags = FieldFlag_AssetPath})
        .field("flipbookColumns", &ParticleEmitter2DComponent::flipbookColumns, {.tooltip = "Frames of the texture across", .rangeMin = 1.0f, .rangeMax = 16.0f})
        .field("flipbookRows", &ParticleEmitter2DComponent::flipbookRows, {.rangeMin = 1.0f, .rangeMax = 16.0f})
        .field("flipbookCycles", &ParticleEmitter2DComponent::flipbookCycles, {.tooltip = "Times the frames play during a life", .rangeMin = 0.0f, .rangeMax = 32.0f, .speed = 0.05f})
        .field("velocityStretch", &ParticleEmitter2DComponent::velocityStretch, {.tooltip = "Stretch along the motion (seconds of speed): sparks, rain", .rangeMin = 0.0f, .rangeMax = 2.0f, .speed = 0.005f})
        .field("collisions", &ParticleEmitter2DComponent::collisions, {.tooltip = "Bounce on the ground plane and ParticleCollider shapes (world space only)"})
        .field("groundPlane", &ParticleEmitter2DComponent::groundPlane)
        .field("groundHeight", &ParticleEmitter2DComponent::groundHeight, {.speed = 0.01f})
        .field("bounce", &ParticleEmitter2DComponent::bounce, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("collisionFriction", &ParticleEmitter2DComponent::collisionFriction, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("dieOnCollision", &ParticleEmitter2DComponent::dieOnCollision)
        .field("affectedByAttractors", &ParticleEmitter2DComponent::affectedByAttractors)
        .field("turbulence", &ParticleEmitter2DComponent::turbulence, {.tooltip = "Swirling noise force (units / s^2), 0 = off", .rangeMin = -100.0f, .rangeMax = 100.0f, .speed = 0.05f})
        .field("turbulenceScale", &ParticleEmitter2DComponent::turbulenceScale, {.tooltip = "Size of the swirls (world units)", .rangeMin = 0.01f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("turbulenceSpeed", &ParticleEmitter2DComponent::turbulenceSpeed, {.tooltip = "How fast the swirls change", .rangeMin = 0.0f, .rangeMax = 20.0f, .speed = 0.01f})
        .field("inheritVelocity", &ParticleEmitter2DComponent::inheritVelocity, {.tooltip = "Fraction of the emitter's velocity given to new particles (world space)", .rangeMin = -2.0f, .rangeMax = 2.0f, .speed = 0.01f})
        .field("subEmitter", &ParticleEmitter2DComponent::subEmitter, {.tooltip = "Emitter that bursts where a particle dies or hits something"})
        .field("subEmitOn", &ParticleEmitter2DComponent::subEmitOn, {.enumEntries = {{"Death", 0}, {"Collision", 1}}})
        .field("subEmitCount", &ParticleEmitter2DComponent::subEmitCount, {.rangeMin = 0.0f, .rangeMax = 1000.0f})
        .field("lit", &ParticleEmitter2DComponent::lit, {.tooltip = "Lit by Light2D like lit sprites (not with additive)"})
        .field("additive", &ParticleEmitter2DComponent::additive, {.tooltip = "Additive blending (fire, light, magic)"})
        .field("worldSpace", &ParticleEmitter2DComponent::worldSpace)
        .field("sortingLayer", &ParticleEmitter2DComponent::sortingLayer, {.speed = 1.0f})
        .field("orderInLayer", &ParticleEmitter2DComponent::orderInLayer, {.speed = 1.0f})
        .field("seed", &ParticleEmitter2DComponent::seed, {.tooltip = "0 = random, otherwise reproducible", .speed = 1.0f})
        .field("gpu", &ParticleEmitter2DComponent::gpu, {.tooltip = "Simulated on the GPU: hundreds of thousands of particles; no curves, collisions, attractors, lit, sorting layer"});
}

} // namespace agr
