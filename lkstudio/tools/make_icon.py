#!/usr/bin/env python3
"""LK Studio launcher icon: a golden gem over a dark leather disc, with a small hammer (the maker)."""
import os
from PIL import Image, ImageDraw, ImageFilter
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "android", "res")
S = 512
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
d.ellipse([16, 16, S - 16, S - 16], fill=(34, 26, 19, 255), outline=(216, 176, 88, 255), width=14)
glow = Image.new("RGBA", (S, S), (0, 0, 0, 0)); g = ImageDraw.Draw(glow)
g.ellipse([120, 110, 392, 382], fill=(216, 176, 88, 120)); glow = glow.filter(ImageFilter.GaussianBlur(40))
img = Image.alpha_composite(img, glow); d = ImageDraw.Draw(img)
cx, cy, r = 256, 236, 132
d.polygon([(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)], fill=(232, 190, 92, 255))
d.polygon([(cx, cy - r), (cx + r, cy), (cx, cy)], fill=(255, 236, 170, 255))
d.polygon([(cx - r, cy), (cx, cy + r), (cx, cy)], fill=(150, 104, 38, 255))
d.polygon([(cx, cy - r * 0.45), (cx + r * 0.45, cy), (cx, cy + r * 0.45), (cx - r * 0.45, cy)], fill=(255, 248, 220, 255))
# hammer
d.line([(300, 400), (400, 300)], fill=(120, 80, 40, 255), width=22)
d.polygon([(370, 250), (450, 330), (420, 360), (340, 280)], fill=(170, 170, 180, 255), outline=(60, 60, 70, 255))
for name, px in [("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)]:
    os.makedirs(os.path.join(ROOT, "mipmap-" + name), exist_ok=True)
    img.resize((px, px), Image.LANCZOS).save(os.path.join(ROOT, "mipmap-" + name, "ic_launcher.png"))
img.save(os.path.join(ROOT, "..", "..", "icon_512.png"))
print("icons written")
