#pragma once
// Right-to-left text (Arabic, Hebrew, Persian, Urdu) for the UI text layout: Arabic letters take
// their joined forms (initial / medial / final, lam-alef ligatures), and each line is reordered for
// display following a simplified Unicode bidirectional algorithm (paragraph direction from its first
// strong letter, numbers and Latin words kept left-to-right inside right-to-left text, neutrals
// between two runs of the same direction take it, brackets mirrored in right-to-left runs).
#include <vector>

#include "agr/core/Types.hpp"

namespace agr::bidi {

bool isRtl(u32 cp);     ///< Strong right-to-left letter (Hebrew, Arabic, Syriac, Thaana, N'Ko...).
bool isStrongLtr(u32 cp);
/// Arabic contextual forms in logical order; same length: a lam-alef pair becomes the ligature
/// followed by 0 (dropped by the layout, keeps indices aligned).
std::vector<u32> shapeArabic(const std::vector<u32>& cps);
/// Paragraph direction: true when its first strong letter is right-to-left.
bool paragraphRtl(const std::vector<u32>& cps);
/// Display order of one line (indices into `cps`, left to right) for a paragraph of direction
/// `rtl`. `mirrored` receives, per index, the mirrored code point of brackets in right-to-left
/// runs (0 = unchanged).
std::vector<u32> visualOrder(const std::vector<u32>& cps, bool rtl, std::vector<u32>* mirrored = nullptr);

} // namespace agr::bidi
