#include "agr/ui/Bidi.hpp"

#include <algorithm>

namespace agr::bidi {

namespace {

bool isArabicDigit(u32 cp) { return (cp >= 0x0660 && cp <= 0x0669) || (cp >= 0x06F0 && cp <= 0x06F9); }
bool isDigit(u32 cp) { return (cp >= '0' && cp <= '9') || isArabicDigit(cp); }
bool isTransparent(u32 cp) { return (cp >= 0x064B && cp <= 0x065F) || cp == 0x0670 || (cp >= 0x0591 && cp <= 0x05C7); }

/// Presentation forms of the basic Arabic letters: first form (isolated) and count (1, 2 or 4:
/// isolated, final, initial, medial).
struct Forms {
    u32 base = 0;
    u8 count = 0;
};
Forms formsOf(u32 cp) {
    static const Forms table[] = {
        {0xFE80, 1}, {0xFE81, 2}, {0xFE83, 2}, {0xFE85, 2}, {0xFE87, 2}, {0xFE89, 4}, {0xFE8D, 2}, {0xFE8F, 4},  // 0621-0628
        {0xFE93, 2}, {0xFE95, 4}, {0xFE99, 4}, {0xFE9D, 4}, {0xFEA1, 4}, {0xFEA5, 4}, {0xFEA9, 2}, {0xFEAB, 2},  // 0629-0630
        {0xFEAD, 2}, {0xFEAF, 2}, {0xFEB1, 4}, {0xFEB5, 4}, {0xFEB9, 4}, {0xFEBD, 4}, {0xFEC1, 4}, {0xFEC5, 4},  // 0631-0638
        {0xFEC9, 4}, {0xFECD, 4},                                                                                // 0639-063A
    };
    if (cp >= 0x0621 && cp <= 0x063A) return table[cp - 0x0621];
    static const Forms late[] = {{0xFED1, 4}, {0xFED5, 4}, {0xFED9, 4}, {0xFEDD, 4}, {0xFEE1, 4}, {0xFEE5, 4},
                                 {0xFEE9, 4}, {0xFEED, 2}, {0xFEEF, 2}, {0xFEF1, 4}};  // 0641-064A
    if (cp >= 0x0641 && cp <= 0x064A) return late[cp - 0x0641];
    switch (cp) {  // Persian / Urdu letters
        case 0x067E: return {0xFB56, 4};  // peh
        case 0x0686: return {0xFB7A, 4};  // tcheh
        case 0x0698: return {0xFB8A, 2};  // jeh
        case 0x06A9: return {0xFB8E, 4};  // keheh
        case 0x06AF: return {0xFB92, 4};  // gaf
        case 0x06CC: return {0xFBFC, 4};  // farsi yeh
        default: return {};
    }
}
bool joinsNext(u32 cp) { return cp == 0x0640 || formsOf(cp).count == 4; }  // tatweel joins both sides
bool joinsPrev(u32 cp) { return cp == 0x0640 || formsOf(cp).count >= 2; }
bool isAlefForLam(u32 cp) { return cp == 0x0622 || cp == 0x0623 || cp == 0x0625 || cp == 0x0627; }

u32 mirror(u32 cp) {
    switch (cp) {
        case '(': return ')';
        case ')': return '(';
        case '[': return ']';
        case ']': return '[';
        case '{': return '}';
        case '}': return '{';
        case '<': return '>';
        case '>': return '<';
        case 0x00AB: return 0x00BB;
        case 0x00BB: return 0x00AB;
        default: return 0;
    }
}

} // namespace

bool isRtl(u32 cp) {
    if (isArabicDigit(cp)) return false;
    return (cp >= 0x0590 && cp <= 0x08FF) || (cp >= 0xFB1D && cp <= 0xFDFF) || (cp >= 0xFE70 && cp <= 0xFEFF) || (cp >= 0x10800 && cp <= 0x10FFF) ||
           (cp >= 0x1E800 && cp <= 0x1EFFF);
}

bool isStrongLtr(u32 cp) {
    if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) return true;
    if (cp < 0xC0 || isRtl(cp) || isDigit(cp) || isTransparent(cp)) return false;
    if (cp == 0xD7 || cp == 0xF7) return false;                      // × ÷
    if (cp >= 0x2000 && cp <= 0x2BFF) return false;                  // punctuation, symbols, arrows
    if (cp >= 0x3000 && cp <= 0x303F) return false;                  // CJK punctuation
    if ((cp >= 0xFE00 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF20)) return false;
    return true;
}

std::vector<u32> shapeArabic(const std::vector<u32>& cps) {
    std::vector<u32> out = cps;
    const usize n = cps.size();
    auto neighbor = [&](usize i, int dir) -> u32 {  // nearest non-transparent letter
        for (std::ptrdiff_t k = static_cast<std::ptrdiff_t>(i) + dir; k >= 0 && k < static_cast<std::ptrdiff_t>(n); k += dir)
            if (!isTransparent(cps[static_cast<usize>(k)])) return cps[static_cast<usize>(k)];
        return 0;
    };
    for (usize i = 0; i < n; ++i) {
        const u32 cp = cps[i];
        const Forms f = formsOf(cp);
        if (f.count == 0) continue;
        const u32 prev = neighbor(i, -1);
        const bool fromPrev = prev && joinsNext(prev) && joinsPrev(cp);
        // Lam + alef: one ligature (isolated or final), the alef slot becomes 0.
        if (cp == 0x0644) {
            usize k = i + 1;
            while (k < n && isTransparent(cps[k])) ++k;
            if (k < n && isAlefForLam(cps[k])) {
                const u32 lig = cps[k] == 0x0622 ? 0xFEF5 : cps[k] == 0x0623 ? 0xFEF7 : cps[k] == 0x0625 ? 0xFEF9 : 0xFEFB;
                out[i] = lig + (fromPrev ? 1u : 0u);
                out[k] = 0;
                i = k;  // the alef is done
                continue;
            }
        }
        const u32 next = neighbor(i, 1);
        const bool toNext = f.count == 4 && next && joinsPrev(next);
        u32 form = 0;  // isolated
        if (fromPrev && toNext) form = 3;
        else if (fromPrev && f.count >= 2) form = 1;
        else if (toNext) form = 2;
        out[i] = f.base + form;
    }
    return out;
}

bool paragraphRtl(const std::vector<u32>& cps) {
    for (u32 cp : cps) {
        if (isRtl(cp)) return true;
        if (isStrongLtr(cp)) return false;
    }
    return false;
}

std::vector<u32> visualOrder(const std::vector<u32>& cps, bool rtl, std::vector<u32>* mirrored) {
    const usize n = cps.size();
    enum : u8 { L, R, EN, N };
    std::vector<u8> type(n);
    for (usize i = 0; i < n; ++i) type[i] = isRtl(cps[i]) ? R : isStrongLtr(cps[i]) ? L : isDigit(cps[i]) ? EN : N;
    // Separators inside numbers belong to them ("3.14", "1,000", "12:30").
    for (usize i = 1; i + 1 < n; ++i)
        if (type[i] == N && type[i - 1] == EN && type[i + 1] == EN && (cps[i] == '.' || cps[i] == ',' || cps[i] == ':' || cps[i] == '/')) type[i] = EN;
    const u8 p = rtl ? 1 : 0;
    std::vector<u8> level(n, p);
    u8 prevStrong = rtl ? R : L;
    for (usize i = 0; i < n; ++i) {
        if (type[i] == L) level[i] = rtl ? 2 : 0, prevStrong = L;
        else if (type[i] == R) level[i] = 1, prevStrong = R;
        else if (type[i] == EN) level[i] = (rtl || prevStrong == R) ? 2 : 0;
    }
    auto dirAt = [&](usize i) -> u8 {  // R or L for strong letters and numbers
        if (type[i] == EN) return level[i] == 2 ? R : L;
        return type[i];
    };
    // Neutrals (and transparent marks) take the direction shared by both sides, else the paragraph's.
    for (usize i = 0; i < n;) {
        if (type[i] != N) {
            ++i;
            continue;
        }
        usize j = i;
        while (j < n && type[j] == N) ++j;
        const u8 before = i == 0 ? (rtl ? R : L) : dirAt(i - 1);
        const u8 after = j == n ? (rtl ? R : L) : dirAt(j);
        u8 lv = p;
        if (before == after) lv = before == R ? 1 : (rtl ? 2 : 0);
        // Combining marks follow their letter.
        for (usize k = i; k < j; ++k) level[k] = isTransparent(cps[k]) && k > 0 ? level[k - 1] : lv;
        i = j;
    }
    // Trailing spaces at the paragraph level.
    for (usize i = n; i-- > 0 && (cps[i] == ' ' || cps[i] == '\t');) level[i] = p;
    std::vector<u32> order(n);
    for (usize i = 0; i < n; ++i) order[i] = static_cast<u32>(i);
    const u8 maxLevel = n ? *std::max_element(level.begin(), level.end()) : 0;
    for (u8 lv = maxLevel; lv >= 1; --lv)
        for (usize i = 0; i < n;) {
            if (level[order[i]] < lv) {
                ++i;
                continue;
            }
            usize j = i;
            while (j < n && level[order[j]] >= lv) ++j;
            std::reverse(order.begin() + static_cast<std::ptrdiff_t>(i), order.begin() + static_cast<std::ptrdiff_t>(j));
            i = j;
        }
    if (mirrored) {
        mirrored->assign(n, 0);
        for (usize i = 0; i < n; ++i)
            if (level[i] % 2 == 1) (*mirrored)[i] = mirror(cps[i]);
    }
    return order;
}

} // namespace agr::bidi
