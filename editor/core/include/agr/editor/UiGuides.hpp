#pragma once
// Smart alignment guides of the UI Builder: a moved rectangle snaps its edges and center to those
// of the other elements and of its parent when they come close.
#include <vector>

#include "agr/math/Math.hpp"

namespace agr::editor {

struct GuideSnap {
    math::Vec2 offset{0.0f, 0.0f};  ///< Added to the moved rectangle (screen pixels).
    std::vector<float> xs;          ///< Vertical guide lines (x) matched.
    std::vector<float> ys;          ///< Horizontal guide lines (y) matched.
};

/// `moving` and `targets`: x, y, width, height (screen pixels). Left / center / right and top /
/// middle / bottom of `moving` snap to the same lines of every target within `threshold` pixels
/// (the closest wins per axis); the guides drawn are every line then aligned.
GuideSnap snapToGuides(const math::Vec4& moving, const std::vector<math::Vec4>& targets, float threshold);

enum class AlignMode { Left, CenterX, Right, Top, Middle, Bottom, DistributeX, DistributeY };
/// Offsets (screen pixels) that align or evenly space `rects` (x, y, w, h): edges / centers on
/// the first-to-last extremes, or equal gaps between the outermost ones (distribute, 3+ rects).
std::vector<math::Vec2> alignRects(const std::vector<math::Vec4>& rects, AlignMode mode);

/// Bounding box (x, y, w, h) of rectangles.
math::Vec4 unionRect(const std::vector<math::Vec4>& rects);
/// Group resize: the box `from` becomes `to`; every rectangle keeps its place and size relative to
/// the box (a group scaled from a corner, like in drawing tools).
std::vector<math::Vec4> scaleRectsInBox(const std::vector<math::Vec4>& rects, const math::Vec4& from, const math::Vec4& to);

} // namespace agr::editor
