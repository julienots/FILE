#!/usr/bin/env python3
"""Bake signed-distance-field font atlases for LOST KINGDOM.

Fonts (SIL Open Font License, see tools/fonts/OFL.txt):
  0 title  : Cinzel Decorative Bold   (logo, big headings)
  1 serif  : Cinzel (variable, regular) (headings, buttons)
  2 body   : Alegreya Sans Medium      (dialogue, descriptions)
  3 bold   : Alegreya Sans Bold        (numbers, emphasis)

Output: assets/ui/font.png (single channel SDF, 1024x1024) and assets/ui/font.txt
  font <idx> <name> <pxsize> <lineheight> <ascender>
  g <font> <codepoint> <x> <y> <w> <h> <xoff> <yoff> <advance>
  k <font> <cp1> <cp2> <adjust>   (kerning, optional)
"""
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy.ndimage import distance_transform_edt

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "ui")
os.makedirs(OUT, exist_ok=True)

FONTS = [
    ("title", "CinzelDecorative-Bold.ttf", 52),
    ("serif", "Cinzel[wght].ttf", 44),
    ("body", "AlegreyaSans-Medium.ttf", 44),
    ("bold", "AlegreyaSans-Bold.ttf", 44),
]
CHARS = [chr(c) for c in range(32, 127)] + list("àâäçéèêëîïôöùûüÿœæÀÂÄÇÉÈÊËÎÏÔÖÙÛÜŸŒÆ«»–—…’‘“”•°×·½¼¾©")
SPREAD = 6
ATLAS = 2048


def sdf(mask, spread):
    inside = distance_transform_edt(mask)
    outside = distance_transform_edt(~mask)
    d = inside - outside  # >0 inside
    return np.clip(0.5 + d / (2.0 * spread), 0.0, 1.0)


def main():
    atlas = np.zeros((ATLAS, ATLAS), dtype=np.float32)
    lines = []
    x = y = 1
    rowh = 0
    for fi, (name, file, size) in enumerate(FONTS):
        font = ImageFont.truetype(os.path.join(HERE, "fonts", file), size)
        asc, desc = font.getmetrics()
        lines.append(f"font {fi} {name} {size} {asc + desc} {asc}")
        for ch in CHARS:
            bbox = font.getbbox(ch)
            adv = font.getlength(ch)
            if ch == " " or bbox[2] <= bbox[0]:
                lines.append(f"g {fi} {ord(ch)} 0 0 0 0 0 0 {adv:.2f}")
                continue
            # render at 4x for accurate distance field, then downsample
            S = 4
            bfont = ImageFont.truetype(os.path.join(HERE, "fonts", file), size * S)
            bb = bfont.getbbox(ch)
            pad = SPREAD * S
            w, h = bb[2] - bb[0] + 2 * pad, bb[3] - bb[1] + 2 * pad
            img = Image.new("L", (w, h), 0)
            ImageDraw.Draw(img).text((pad - bb[0], pad - bb[1]), ch, font=bfont, fill=255)
            m = np.array(img) > 127
            d = sdf(m, SPREAD * S)
            gw, gh = (w + S - 1) // S, (h + S - 1) // S
            dimg = Image.fromarray((d * 255).astype(np.uint8)).resize((gw, gh), Image.LANCZOS)
            g = np.array(dimg).astype(np.float32) / 255.0
            if x + gw + 1 >= ATLAS:
                x = 1
                y += rowh + 1
                rowh = 0
            if y + gh >= ATLAS:
                sys.exit("font atlas full")
            atlas[y:y + gh, x:x + gw] = g
            xoff = bb[0] / S - SPREAD
            yoff = bb[1] / S - SPREAD
            lines.append(f"g {fi} {ord(ch)} {x} {y} {gw} {gh} {xoff:.2f} {yoff:.2f} {adv:.2f}")
            x += gw + 1
            rowh = max(rowh, gh)
        # kerning pairs for common capital combinations in titles
        for a in "AVTWYLPFK":
            for b in "AVTWYOaeo.,":
                try:
                    k = font.getlength(a + b) - font.getlength(a) - font.getlength(b)
                except Exception:
                    k = 0
                if abs(k) > 0.5:
                    lines.append(f"k {fi} {ord(a)} {ord(b)} {k:.2f}")
        x = 1
        y += rowh + 1
        rowh = 0
    H = 1024 if y < 1024 else ATLAS
    atlas = atlas[:H]
    Image.fromarray((atlas * 255).astype(np.uint8)).save(os.path.join(OUT, "font.png"), optimize=True)
    with open(os.path.join(OUT, "font.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("font atlas used rows up to", y, "/", ATLAS, "glyph lines", len(lines))


if __name__ == "__main__":
    main()
