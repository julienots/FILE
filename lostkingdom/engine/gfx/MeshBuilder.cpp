#include "MeshBuilder.h"

namespace agr {

static inline int8_t snorm8(float f) { return (int8_t)clampf(std::round(f * 127.f), -127, 127); }
static inline uint8_t unorm8(float f) { return (uint8_t)clampf(std::round(f * 255.f), 0, 255); }

vec3 unpackNormal(const Vertex& v) { return normalize(vec3(v.nx / 127.f, v.ny / 127.f, v.nz / 127.f)); }

uint32_t MeshBuilder::vertSkin(vec3 p, vec3 n, const Mat& m, int b0, int b1, float w) {
    vec3 wp = xf.transformPoint(p);
    vec3 wn = normalize(xf.transformDir(n));
    Vertex o;
    o.px = wp.x; o.py = wp.y; o.pz = wp.z;
    o.nx = snorm8(wn.x); o.ny = snorm8(wn.y); o.nz = snorm8(wn.z); o.nw = 0;
    vec3 c = m.color;
    if (jitter > 0) {
        uint32_t h = hash3i((int)std::floor(wp.x * 7.f), (int)std::floor(wp.y * 7.f), (int)std::floor(wp.z * 7.f), jitterSeed);
        float j = (hashf(h) - 0.5f) * 2.f * jitter;
        c = c * (1.f + j);
    }
    o.r = unorm8(c.x); o.g = unorm8(c.y); o.b = unorm8(c.z); o.a = unorm8(ao);
    o.rough = unorm8(m.rough); o.metal = unorm8(m.metal); o.emis = unorm8(m.emis / 4.f); o.wind = unorm8(m.wind);
    o.b0 = (uint8_t)b0; o.b1 = (uint8_t)b1; o.w0 = unorm8(w); o.ex = m.night ? 255 : 0;
    v.push_back(o);
    return (uint32_t)v.size() - 1;
}

uint32_t MeshBuilder::vert(vec3 p, vec3 n, const Mat& m) { return vertSkin(p, n, m, bone0, bone1, w0); }

void MeshBuilder::triP(vec3 a, vec3 b, vec3 c, const Mat& m) {
    vec3 n = normalize(cross(b - a, c - a));
    uint32_t i0 = vert(a, n, m), i1 = vert(b, n, m), i2 = vert(c, n, m);
    tri(i0, i1, i2);
}

void MeshBuilder::quadP(vec3 a, vec3 b, vec3 c, vec3 d, const Mat& m) {
    vec3 n = normalize(cross(b - a, c - a) + cross(c - a, d - a));
    uint32_t i0 = vert(a, n, m), i1 = vert(b, n, m), i2 = vert(c, n, m), i3 = vert(d, n, m);
    quad(i0, i1, i2, i3);
}

void MeshBuilder::box(vec3 c, vec3 h, const Mat& m, quat r) {
    vec3 X = r.rotate({h.x, 0, 0}), Y = r.rotate({0, h.y, 0}), Z = r.rotate({0, 0, h.z});
    auto P = [&](float sx, float sy, float sz) { return c + X * sx + Y * sy + Z * sz; };
    quadP(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), m);     // +Z
    quadP(P(1, -1, -1), P(-1, -1, -1), P(-1, 1, -1), P(1, 1, -1), m); // -Z
    quadP(P(1, -1, 1), P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), m);     // +X
    quadP(P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1), P(-1, 1, -1), m); // -X
    quadP(P(-1, 1, 1), P(1, 1, 1), P(1, 1, -1), P(-1, 1, -1), m);     // +Y
    quadP(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), m); // -Y
}

void MeshBuilder::bevelBox(vec3 c, vec3 h, float bv, const Mat& m, quat r) {
    bv = std::min(bv, std::min(h.x, std::min(h.y, h.z)) * 0.9f);
    // Octagonal prism footprint + chamfered top ring.
    std::vector<vec2> fp = {{-h.x + bv, -h.z}, {h.x - bv, -h.z}, {h.x, -h.z + bv}, {h.x, h.z - bv},
                            {h.x - bv, h.z}, {-h.x + bv, h.z}, {-h.x, h.z - bv}, {-h.x, -h.z + bv}};
    auto W = [&](vec2 p, float y) { return c + r.rotate({p.x, y, p.y}); };
    int n = (int)fp.size();
    float yb = -h.y, yt = h.y - bv;
    for (int i = 0; i < n; i++) {
        vec2 a = fp[i], b = fp[(i + 1) % n];
        quadP(W(a, yb), W(a, yt), W(b, yt), W(b, yb), m);
        vec2 ai = a * ((std::max(h.x, h.z) - bv) / std::max(h.x, h.z)), bi = b * ((std::max(h.x, h.z) - bv) / std::max(h.x, h.z));
        quadP(W(a, yt), W(ai, h.y), W(bi, h.y), W(b, yt), m);
    }
    // top & bottom caps
    vec3 ctop = W({0, 0}, h.y), cbot = W({0, 0}, yb);
    for (int i = 0; i < n; i++) {
        vec2 a = fp[i], b = fp[(i + 1) % n];
        vec2 ai = a * ((std::max(h.x, h.z) - bv) / std::max(h.x, h.z)), bi = b * ((std::max(h.x, h.z) - bv) / std::max(h.x, h.z));
        triP(ctop, W(bi, h.y), W(ai, h.y), m);
        triP(cbot, W(a, yb), W(b, yb), m);
    }
}

void MeshBuilder::ellipsoid(vec3 c, vec3 rad, const Mat& m, int seg, int rings, quat rot) {
    uint32_t base = (uint32_t)v.size();
    if (flat) {
        auto P = [&](int i, int j) {
            float th = PI * j / rings, ph = TAU * i / seg;
            vec3 u{std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph)};
            return c + rot.rotate(u * rad);
        };
        for (int j = 0; j < rings; j++)
            for (int i = 0; i < seg; i++) {
                vec3 a = P(i, j), b = P(i + 1, j), cc = P(i + 1, j + 1), d = P(i, j + 1);
                if (j == 0) triP(a, cc, d, m);
                else if (j == rings - 1) triP(a, b, d, m);
                else quadP(a, b, cc, d, m);
            }
        return;
    }
    for (int j = 0; j <= rings; j++) {
        float th = PI * j / rings;
        for (int i = 0; i <= seg; i++) {
            float ph = TAU * i / seg;
            vec3 u{std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph)};
            vec3 p = c + rot.rotate(u * rad);
            vec3 n = rot.rotate(normalize(vec3(u.x / rad.x, u.y / rad.y, u.z / rad.z)));
            vert(p, n, m);
        }
    }
    for (int j = 0; j < rings; j++)
        for (int i = 0; i < seg; i++) {
            uint32_t a = base + j * (seg + 1) + i, b = a + 1, d = a + (seg + 1), cc = d + 1;
            tri(a, b, cc); tri(a, cc, d);
        }
}

void MeshBuilder::tube(vec3 a, vec3 b, float ra, float rb, const Mat& m, int seg, bool caps, int boneA, int boneB, float blendStart) {
    vec3 axis = b - a;
    float len = length(axis);
    if (len < 1e-5f) return;
    vec3 dir = axis / len;
    vec3 t = std::fabs(dir.y) < 0.95f ? normalize(cross(dir, {0, 1, 0})) : normalize(cross(dir, {1, 0, 0}));
    vec3 bt = cross(dir, t);
    bool skin = boneA >= 0;
    int rings = skin ? 4 : 1;
    uint32_t base = (uint32_t)v.size();
    float slope = (ra - rb) / len;
    for (int j = 0; j <= rings; j++) {
        float f = (float)j / rings;
        vec3 c = a + axis * f;
        float r = lerpf(ra, rb, f);
        float w = 1.f;
        if (skin) w = 1.f - smoothstepf(blendStart, 1.f, f) * 0.5f;  // near the child joint share weight 50/50
        for (int i = 0; i <= seg; i++) {
            float ph = TAU * i / seg;
            vec3 rd = t * std::cos(ph) + bt * std::sin(ph);
            vec3 n = normalize(rd + dir * slope);
            if (skin) vertSkin(c + rd * r, n, m, boneA, boneB, w);
            else vert(c + rd * r, n, m);
        }
    }
    for (int j = 0; j < rings; j++)
        for (int i = 0; i < seg; i++) {
            uint32_t i0 = base + j * (seg + 1) + i, i1 = i0 + 1, i3 = i0 + (seg + 1), i2 = i3 + 1;
            tri(i0, i1, i2); tri(i0, i2, i3);
        }
    if (caps) {
        // Rounded-ish caps: a small cone outwards for organic limbs.
        auto cap = [&](vec3 c, float r, vec3 out, float w) {
            uint32_t ci = skin ? vertSkin(c + out * r * 0.5f, out, m, boneA, boneB, w) : vert(c + out * r * 0.5f, out, m);
            uint32_t ring = (uint32_t)v.size();
            for (int i = 0; i <= seg; i++) {
                float ph = TAU * i / seg;
                vec3 rd = t * std::cos(ph) + bt * std::sin(ph);
                vec3 n = normalize(rd + out * 0.8f);
                if (skin) vertSkin(c + rd * r, n, m, boneA, boneB, w); else vert(c + rd * r, n, m);
            }
            for (int i = 0; i < seg; i++) {
                if (dot(out, dir) > 0) tri(ci, ring + i, ring + i + 1);
                else tri(ci, ring + i + 1, ring + i);
            }
        };
        cap(a, ra, -dir, 1.f);
        cap(b, rb, dir, skin ? 0.5f : 1.f);
    }
}

void MeshBuilder::cone(vec3 base, vec3 tip, float r, const Mat& m, int seg) {
    vec3 dir = normalize(tip - base);
    vec3 t = std::fabs(dir.y) < 0.95f ? normalize(cross(dir, {0, 1, 0})) : normalize(cross(dir, {1, 0, 0}));
    vec3 bt = cross(dir, t);
    for (int i = 0; i < seg; i++) {
        float a0 = TAU * i / seg, a1 = TAU * (i + 1) / seg;
        vec3 p0 = base + (t * std::cos(a0) + bt * std::sin(a0)) * r;
        vec3 p1 = base + (t * std::cos(a1) + bt * std::sin(a1)) * r;
        if (flat) { triP(p0, p1, tip, m); triP(base, p1, p0, m); continue; }
        vec3 n0 = normalize(p0 - base + dir * r * 0.5f), n1 = normalize(p1 - base + dir * r * 0.5f);
        uint32_t i0 = vert(p0, n0, m), i1 = vert(p1, n1, m), it = vert(tip, normalize(n0 + n1), m);
        tri(i0, i1, it);
        uint32_t b0 = vert(base, -dir, m), b1 = vert(p0, -dir, m), b2 = vert(p1, -dir, m);
        tri(b0, b2, b1);
    }
}

void MeshBuilder::lathe(const std::vector<vec2>& prof, const Mat& m, int seg, vec3 o, quat rot, float squashZ) {
    int n = (int)prof.size();
    for (int j = 0; j + 1 < n; j++)
        for (int i = 0; i < seg; i++) {
            float a0 = TAU * i / seg, a1 = TAU * (i + 1) / seg;
            auto P = [&](int k, float a) { return o + rot.rotate({std::cos(a) * prof[k].x, prof[k].y, std::sin(a) * prof[k].x * squashZ}); };
            vec3 p00 = P(j, a0), p01 = P(j, a1), p10 = P(j + 1, a0), p11 = P(j + 1, a1);
            if (flat) { quadP(p00, p10, p11, p01, m); continue; }
            vec2 d = prof[j + 1] - prof[j];
            vec2 pn = normalize(vec2(d.y, -d.x));  // outward in (r,y) plane
            auto N = [&](float a) { return rot.rotate(normalize(vec3(std::cos(a) * pn.x, pn.y, std::sin(a) * pn.x))); };
            uint32_t i0 = vert(p00, N(a0), m), i1 = vert(p10, N(a0), m), i2 = vert(p11, N(a1), m), i3 = vert(p01, N(a1), m);
            quad(i0, i1, i2, i3);
        }
}

void MeshBuilder::prism(const std::vector<vec2>& poly, float y0, float y1, const Mat& m) {
    int n = (int)poly.size();
    vec2 c{0, 0};
    for (auto& p : poly) c += p;
    c = c / (float)n;
    for (int i = 0; i < n; i++) {
        vec2 a = poly[i], b = poly[(i + 1) % n];
        quadP({a.x, y0, a.y}, {a.x, y1, a.y}, {b.x, y1, b.y}, {b.x, y0, b.y}, m);
        triP({c.x, y1, c.y}, {b.x, y1, b.y}, {a.x, y1, a.y}, m);
    }
}

void MeshBuilder::plane(vec3 c, vec2 h, const Mat& m, quat rot) {
    vec3 X = rot.rotate({h.x, 0, 0}), Z = rot.rotate({0, 0, h.y});
    quadP(c - X + Z, c + X + Z, c + X - Z, c - X - Z, m);
}

void MeshBuilder::blade(vec3 root, vec3 tip, float width, vec3 side, const Mat& mr, const Mat& mt) {
    vec3 s = normalize(side) * (width * 0.5f);
    vec3 mid = lerp(root, tip, 0.5f);
    vec3 n = normalize(cross(s, tip - root));
    uint32_t a = vert(root - s, n, mr), b = vert(root + s, n, mr);
    uint32_t c = vert(mid + s * 0.8f, n, lerpf(0, 1, 0.5f) > 0 ? mt : mr), d = vert(mid - s * 0.8f, n, mt);
    uint32_t t = vert(tip, n, mt);
    quad(a, b, c, d); tri(d, c, t);
    // back side
    uint32_t a2 = vert(root - s, -n, mr), b2 = vert(root + s, -n, mr), c2 = vert(mid + s * 0.8f, -n, mt), d2 = vert(mid - s * 0.8f, -n, mt), t2 = vert(tip, -n, mt);
    quad(a2, d2, c2, b2); tri(d2, t2, c2);
}

void MeshBuilder::rock(vec3 c, vec3 rad, const Mat& m, uint32_t seed, int detail) {
    bool oldFlat = flat; flat = true;
    int seg = 7 + detail * 2, rings = 5 + detail;
    auto P = [&](int i, int j) {
        float th = PI * j / rings, ph = TAU * (i % seg) / seg;
        vec3 u{std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph)};
        float d = 0.78f + 0.36f * hashf(hash3i(i % seg, j, 0, seed));
        if (j == 0 || j == rings) d = 0.85f;
        vec3 p = u * rad * d;
        if (p.y < -rad.y * 0.35f) p.y = -rad.y * 0.35f;  // flat-ish bottom so rocks sit in the ground
        return c + p;
    };
    for (int j = 0; j < rings; j++)
        for (int i = 0; i < seg; i++) {
            vec3 a = P(i, j), b = P(i + 1, j), cc = P(i + 1, j + 1), d = P(i, j + 1);
            if (j == 0) triP(a, cc, d, m);
            else if (j == rings - 1) triP(a, b, d, m);
            else { triP(a, b, cc, m); triP(a, cc, d, m); }
        }
    flat = oldFlat;
}

void MeshBuilder::append(const MeshBuilder& o) {
    uint32_t base = (uint32_t)v.size();
    v.insert(v.end(), o.v.begin(), o.v.end());
    idx.reserve(idx.size() + o.idx.size());
    for (uint32_t i : o.idx) idx.push_back(i + base);
}

void MeshBuilder::bakeHeightAO(float y0, float y1, float minAO, size_t from) {
    for (size_t i = from; i < v.size(); i++) {
        float t = smoothstepf(y0, y1, v[i].py);
        float a = lerpf(minAO, 1.f, t);
        v[i].a = (uint8_t)clampf(v[i].a * a, 0, 255);
    }
}

void MeshBuilder::applyColorJitter(size_t from, float amount, uint32_t seed) {
    for (size_t i = from; i < v.size(); i++) {
        uint32_t h = hash3i((int)std::floor(v[i].px * 5), (int)std::floor(v[i].py * 5), (int)std::floor(v[i].pz * 5), seed);
        float j = 1.f + (hashf(h) - 0.5f) * 2.f * amount;
        v[i].r = (uint8_t)clampf(v[i].r * j, 0, 255);
        v[i].g = (uint8_t)clampf(v[i].g * j, 0, 255);
        v[i].b = (uint8_t)clampf(v[i].b * j, 0, 255);
    }
}

AABB MeshBuilder::bounds() const {
    AABB b;
    for (auto& p : v) b.add({p.px, p.py, p.pz});
    return b;
}

}  // namespace agr
