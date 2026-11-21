#include "agr/editor/UiGuides.hpp"

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

} // namespace agr::editor
