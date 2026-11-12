// AGR Engine — .lkm loader (see Model.h).
#include "Model.h"
#include "../core/Platform.h"
#include "../third_party/stb_image.h"
#include <cstring>

namespace agr {

bool SkinnedModelData::loadLKM(const std::vector<uint8_t>& b) {
    verts.clear(); idx.clear(); joints.clear();
    size_t off = 0;
    auto need = [&](size_t n) { return off + n <= b.size(); };
    auto rd = [&](void* dst, size_t n) { if (!need(n)) return false; memcpy(dst, b.data() + off, n); off += n; return true; };
    char magic[4]; uint32_t nv = 0, ni = 0, nj = 0;
    if (!rd(magic, 4) || memcmp(magic, "LKM1", 4) != 0) { AGR_WARN("model: bad magic"); return false; }
    if (!rd(&nv, 4) || !rd(&ni, 4) || !rd(&nj, 4) || !rd(&height, 4)) return false;
    if (nv == 0 || nv > 200000 || ni == 0 || ni > 1200000 || ni % 3 || nj > 64) { AGR_WARN("model: bad header"); return false; }
    size_t total = (size_t)nj * 12 + (size_t)nv * (12 + 3 + 4 + 3) + (size_t)ni * 4;
    if (!need(total)) { AGR_WARN("model: truncated file"); return false; }
    joints.resize(nj);
    for (uint32_t i = 0; i < nj; i++) { float f[3]; rd(f, 12); joints[i] = {f[0], f[1], f[2]}; }
    verts.assign(nv, Vertex{});
    const uint8_t* pos = b.data() + off; off += (size_t)nv * 12;
    const int8_t* nrm = (const int8_t*)(b.data() + off); off += (size_t)nv * 3;
    const uint8_t* uv = b.data() + off; off += (size_t)nv * 4;
    const uint8_t* sk = b.data() + off; off += (size_t)nv * 3;
    for (uint32_t i = 0; i < nv; i++) {
        Vertex& v = verts[i];
        float p[3]; memcpy(p, pos + i * 12, 12);
        v.px = p[0]; v.py = p[1]; v.pz = p[2];
        v.nx = nrm[i * 3]; v.ny = nrm[i * 3 + 1]; v.nz = nrm[i * 3 + 2]; v.nw = 0;
        v.r = v.g = v.b = v.a = 255;
        // 16-bit UVs packed into the material bytes (read back by the TEXTURED shader variant)
        v.rough = uv[i * 4]; v.metal = uv[i * 4 + 1]; v.emis = uv[i * 4 + 2]; v.wind = uv[i * 4 + 3];
        v.b0 = sk[i * 3]; v.b1 = sk[i * 3 + 1]; v.w0 = sk[i * 3 + 2]; v.ex = 0;
        if (v.b0 >= nj || v.b1 >= nj) { AGR_WARN("model: bone index out of range"); return false; }
    }
    idx.resize(ni);
    memcpy(idx.data(), b.data() + off, (size_t)ni * 4);
    for (uint32_t i : idx) if (i >= nv) { AGR_WARN("model: index out of range"); verts.clear(); idx.clear(); return false; }
    return true;
}

bool SkinnedModelData::loadTexture(const std::vector<uint8_t>& enc) {
    int w, h, n;
    unsigned char* px = stbi_load_from_memory(enc.data(), (int)enc.size(), &w, &h, &n, 4);
    if (!px) { AGR_WARN("model: texture decode failed"); return false; }
    rgba.assign(px, px + (size_t)w * h * 4);
    texW = w; texH = h;
    stbi_image_free(px);
    return true;
}

}  // namespace agr
