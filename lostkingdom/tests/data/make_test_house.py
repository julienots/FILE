#!/usr/bin/env python3
"""Builds tests/data/test_house.glb: a plain textured cottage (walls, gable roof, door on +Z) that stands in
for a Tripo building in the `buildings` scenario. It is NOT a game asset."""
import io, os, sys
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
from simplify_glb import write_glb

tex = Image.new("RGB", (256, 256)); d = ImageDraw.Draw(tex)
d.rectangle([0, 0, 255, 127], fill=(232, 222, 200))                  # plaster wall (top half)
for x in range(0, 256, 42): d.rectangle([x, 0, x + 6, 127], fill=(70, 46, 30))   # timber beams
d.rectangle([0, 128, 255, 255], fill=(150, 58, 40))                  # roof tiles (bottom half)
for y in range(128, 256, 12): d.line([0, y, 255, y], fill=(110, 40, 30), width=2)
d.rectangle([100, 40, 156, 127], fill=(90, 58, 34))                  # door (in the wall region)
P, UV, I = [], [], []
def quad(a, b, c, e, uv):
    n = len(P); P.extend([a, b, c, e]); UV.extend(uv); I.extend([[n, n + 1, n + 2], [n, n + 2, n + 3]])
W, D, H, R = 3.0, 4.0, 3.2, 2.2
wall = [(0, 0.5), (1, 0.5), (1, 0), (0, 0)]
quad((-W, 0, D), (W, 0, D), (W, H, D), (-W, H, D), [(0.2, 0.5), (0.8, 0.5), (0.8, 0.0), (0.2, 0.0)])   # front (door)
quad((W, 0, -D), (-W, 0, -D), (-W, H, -D), (W, H, -D), wall)
quad((W, 0, D), (W, 0, -D), (W, H, -D), (W, H, D), wall)
quad((-W, 0, -D), (-W, 0, D), (-W, H, D), (-W, H, -D), wall)
roof = [(0, 1), (1, 1), (1, 0.5), (0, 0.5)]
quad((W + 0.4, H - 0.3, D + 0.4), (W + 0.4, H - 0.3, -D - 0.4), (0, H + R, -D - 0.4), (0, H + R, D + 0.4), roof)
quad((-W - 0.4, H - 0.3, -D - 0.4), (-W - 0.4, H - 0.3, D + 0.4), (0, H + R, D + 0.4), (0, H + R, -D - 0.4), roof)
n = len(P); P.extend([(-W, H, D), (W, H, D), (0, H + R - 0.2, D)]); UV.extend([(0, 0.5), (1, 0.5), (0.5, 0.3)]); I.append([n, n + 1, n + 2])
n = len(P); P.extend([(W, H, -D), (-W, H, -D), (0, H + R - 0.2, -D)]); UV.extend([(0, 0.5), (1, 0.5), (0.5, 0.3)]); I.append([n, n + 1, n + 2])
P = np.array(P, float); UV = np.array(UV, float); I = np.array(I)
N = np.zeros_like(P)
for t in I:
    fn = np.cross(P[t[1]] - P[t[0]], P[t[2]] - P[t[0]]); N[t] += fn
N /= np.linalg.norm(N, axis=1, keepdims=True)
buf = io.BytesIO(); tex.save(buf, "JPEG", quality=90)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "test_house.glb")
write_glb(out, P, N, UV, I, buf.getvalue()); print("wrote", out)
