#include "agr/editor/UiGuides.hpp"

#include <algorithm>
#include <cmath>

namespace agr::editor {

GuideSnap snapToGuides(const math::Vec4& m, const std::vector<math::Vec4>& targets, float threshold) {
    GuideSnap out;
    const float mx[3] = {m.x, m.x + m.z * 0.5f, m.x + m.z};
    const float my[3] = {m.y, m.y + m.w * 0.5f, m.y + m.w};
    float bestX = threshold + 1.0f, bestY = threshold + 1.0f;
    for (const math::Vec4& t : targets) {
        const float tx[3] = {t.x, t.x + t.z * 0.5f, t.x + t.z};
        const float ty[3] = {t.y, t.y + t.w * 0.5f, t.y + t.w};
        for (float a : mx)
            for (float b : tx)
                if (std::fabs(b - a) < std::fabs(bestX)) bestX = b - a;
        for (float a : my)
            for (float b : ty)
                if (std::fabs(b - a) < std::fabs(bestY)) bestY = b - a;
    }
    if (std::fabs(bestX) <= threshold) out.offset.x = bestX;
    if (std::fabs(bestY) <= threshold) out.offset.y = bestY;
    // Lines aligned after the snap (several targets may share them).
    for (const math::Vec4& t : targets) {
        const float tx[3] = {t.x, t.x + t.z * 0.5f, t.x + t.z};
        const float ty[3] = {t.y, t.y + t.w * 0.5f, t.y + t.w};
        if (std::fabs(bestX) <= threshold)
            for (float a : mx)
                for (float b : tx)
                    if (std::fabs(a + out.offset.x - b) < 0.01f) out.xs.push_back(b);
        if (std::fabs(bestY) <= threshold)
            for (float a : my)
                for (float b : ty)
                    if (std::fabs(a + out.offset.y - b) < 0.01f) out.ys.push_back(b);
    }
    return out;
}

std::vector<math::Vec2> alignRects(const std::vector<math::Vec4>& rects, AlignMode mode) {
    std::vector<math::Vec2> out(rects.size(), math::Vec2{0.0f, 0.0f});
    if (rects.size() < 2) return out;
    float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f;
    for (const math::Vec4& r : rects) {
        x0 = std::min(x0, r.x), x1 = std::max(x1, r.x + r.z), y0 = std::min(y0, r.y), y1 = std::max(y1, r.y + r.w);
    }
    if (mode == AlignMode::DistributeX || mode == AlignMode::DistributeY) {
        if (rects.size() < 3) return out;
        const bool horizontal = mode == AlignMode::DistributeX;
        std::vector<std::size_t> order(rects.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return horizontal ? rects[a].x < rects[b].x : rects[a].y < rects[b].y; });
        float total = 0.0f;
        for (const math::Vec4& r : rects) total += horizontal ? r.z : r.w;
        const float gap = ((horizontal ? x1 - x0 : y1 - y0) - total) / static_cast<float>(rects.size() - 1);
        float cursor = horizontal ? x0 : y0;
        for (std::size_t i : order) {
            const math::Vec4& r = rects[i];
            if (horizontal) out[i].x = cursor - r.x, cursor += r.z + gap;
            else out[i].y = cursor - r.y, cursor += r.w + gap;
        }
        return out;
    }
    for (std::size_t i = 0; i < rects.size(); ++i) {
        const math::Vec4& r = rects[i];
        switch (mode) {
            case AlignMode::Left: out[i].x = x0 - r.x; break;
            case AlignMode::CenterX: out[i].x = (x0 + x1) * 0.5f - (r.x + r.z * 0.5f); break;
            case AlignMode::Right: out[i].x = x1 - (r.x + r.z); break;
            case AlignMode::Top: out[i].y = y0 - r.y; break;
            case AlignMode::Middle: out[i].y = (y0 + y1) * 0.5f - (r.y + r.w * 0.5f); break;
            case AlignMode::Bottom: out[i].y = y1 - (r.y + r.w); break;
            default: break;
        }
    }
    return out;
}

math::Vec4 unionRect(const std::vector<math::Vec4>& rects) {
    if (rects.empty()) return {0, 0, 0, 0};
    float x0 = rects[0].x, y0 = rects[0].y, x1 = rects[0].x + rects[0].z, y1 = rects[0].y + rects[0].w;
    for (const math::Vec4& r : rects) {
        x0 = std::min(x0, r.x), y0 = std::min(y0, r.y);
        x1 = std::max(x1, r.x + r.z), y1 = std::max(y1, r.y + r.w);
    }
    return {x0, y0, x1 - x0, y1 - y0};
}

std::vector<math::Vec4> scaleRectsInBox(const std::vector<math::Vec4>& rects, const math::Vec4& from, const math::Vec4& to) {
    const float sx = from.z > 1e-6f ? to.z / from.z : 1.0f, sy = from.w > 1e-6f ? to.w / from.w : 1.0f;
    std::vector<math::Vec4> out;
    out.reserve(rects.size());
    for (const math::Vec4& r : rects) out.push_back({to.x + (r.x - from.x) * sx, to.y + (r.y - from.y) * sy, r.z * sx, r.w * sy});
    return out;
}

} // namespace agr::editor
