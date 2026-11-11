// AGR Engine — procedural geometry construction.
// All models in Lost Kingdom are authored in code with this builder: characters,
// creatures, buildings, foliage and props. Vertices carry PBR material channels,
// baked ambient occlusion (albedo alpha) and up to two skinning bones.
#pragma once
#include "GL.h"
#include "../core/Noise.h"
#include <vector>
#include <functional>

namespace agr {

struct Mat {
    vec3 color{0.8f, 0.8f, 0.8f};
    float rough = 0.8f, metal = 0.f, emis = 0.f, wind = 0.f;
    bool night = false;  // emissive only at night (windows, lanterns)
    Mat() = default;
    Mat(vec3 c, float r = 0.8f, float m = 0.f, float e = 0.f, float w = 0.f) : color(c), rough(r), metal(m), emis(e), wind(w) {}
    Mat withColor(vec3 c) const { Mat m = *this; m.color = c; return m; }
    Mat withWind(float w) const { Mat m = *this; m.wind = w; return m; }
    Mat withEmis(float e) const { Mat m = *this; m.emis = e; return m; }
    Mat nightGlow(float e) const { Mat m = *this; m.emis = e; m.night = true; return m; }
};

class MeshBuilder {
public:
    std::vector<Vertex> v;
    std::vector<uint32_t> idx;

    // Current transform applied to all emitted geometry.
    mat4 xf;
    // Skinning: vertices are bound to bone0 with weight w0, and bone1 with 1-w0.
    int bone0 = 0, bone1 = 0; float w0 = 1.f;
    // Colour jitter amplitude applied per-vertex for a hand-painted look.
    float jitter = 0.f;
    uint32_t jitterSeed = 1;
    bool flat = false;  // faceted shading (duplicate vertices per face)
    float ao = 1.f;     // AO value written to albedo alpha

    void clear() { v.clear(); idx.clear(); xf = mat4(); bone0 = bone1 = 0; w0 = 1; }
    void setBone(int b) { bone0 = bone1 = b; w0 = 1; }
    uint32_t vert(vec3 p, vec3 n, const Mat& m);
    uint32_t vertSkin(vec3 p, vec3 n, const Mat& m, int b0, int b1, float w);
    void tri(uint32_t a, uint32_t b, uint32_t c) { idx.push_back(a); idx.push_back(b); idx.push_back(c); }
    void quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { tri(a, b, c); tri(a, c, d); }
    // Emits a flat-shaded triangle/quad from positions (normal computed).
    void triP(vec3 a, vec3 b, vec3 c, const Mat& m);
    void quadP(vec3 a, vec3 b, vec3 c, vec3 d, const Mat& m);

    void box(vec3 center, vec3 half, const Mat& m, quat rot = quat());
    // Box with bevelled vertical edges & top (cheap chamfer), looks less "programmer art".
    void bevelBox(vec3 center, vec3 half, float bevel, const Mat& m, quat rot = quat());
    void ellipsoid(vec3 c, vec3 radii, const Mat& m, int seg = 10, int rings = 7, quat rot = quat());
    // Tapered tube from a to b. If boneA != boneB the weights blend along the tube length (smooth joints).
    void tube(vec3 a, vec3 b, float ra, float rb, const Mat& m, int seg = 8, bool caps = true, int boneA = -1, int boneB = -1, float blendStart = 0.7f);
    void cone(vec3 base, vec3 tip, float r, const Mat& m, int seg = 8);
    // Revolve a (radius, height) profile around the local Y axis.
    void lathe(const std::vector<vec2>& profile, const Mat& m, int seg, vec3 origin = {}, quat rot = quat(), float squashZ = 1.f);
    // Extrude a 2D polygon (XZ plane) by height.
    void prism(const std::vector<vec2>& poly, float y0, float y1, const Mat& m);
    void plane(vec3 c, vec2 half, const Mat& m, quat rot = quat());
    // Thin blade/leaf card (double sided) from root to tip with given width.
    void blade(vec3 root, vec3 tip, float width, vec3 side, const Mat& mRoot, const Mat& mTip);
    // Irregular rock: displaced ellipsoid, faceted.
    void rock(vec3 c, vec3 radii, const Mat& m, uint32_t seed, int detail = 1);

    void append(const MeshBuilder& o);
    // Darken vertices near y0, brighten towards y1 (ground occlusion), multiplied into AO.
    void bakeHeightAO(float y0, float y1, float minAO, size_t fromVertex = 0);
    void applyColorJitter(size_t from, float amount, uint32_t seed);
    AABB bounds() const;
    size_t vertexCount() const { return v.size(); }
    void upload(Mesh& m, bool dynamic = false) const { m.upload(v, idx, dynamic); }
};

vec3 unpackNormal(const Vertex& v);

}  // namespace agr
