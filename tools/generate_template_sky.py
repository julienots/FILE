#!/usr/bin/env python3
"""Generates the original HDR sky of the editor's "Vitrine 3D" template (AGR ENGINE).

  * sky/afternoon.hdr - 1024 x 512 equirectangular (latitude / longitude) Radiance RGBE image:
    blue gradient sky, sun matching the template's Sun light (elevation 42 degrees), soft
    procedural clouds, warm ground below the horizon. Linear light values (the sun disc reaches
    ~60, well above 1: reflections and bloom need the range).

Procedural (no photo, no third-party data), deterministic. Requires numpy.
Equirect convention (engine): u = 0.5 + atan2(d.x, -d.z) / 2pi, v = acos(d.y) / pi.
"""
import math
import os

import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "editor", "resources", "templates", "assets", "sky")
W, H = 1024, 512
rng = np.random.default_rng(7)


def directions():
    u = (np.arange(W) + 0.5) / W
    v = (np.arange(H) + 0.5) / H
    phi = (u - 0.5) * 2.0 * math.pi
    theta = v * math.pi
    st, ct = np.sin(theta)[:, None], np.cos(theta)[:, None]
    return st * np.sin(phi)[None, :], np.broadcast_to(ct, (H, W)), -st * np.cos(phi)[None, :]


def tileable_noise(cells_x, cells_y, octaves=5):
    """Value noise that wraps horizontally (seamless at u = 0 / 1)."""
    total = np.zeros((H, W))
    amp, norm = 1.0, 0.0
    for o in range(octaves):
        cx, cy = cells_x * 2 ** o, cells_y * 2 ** o
        grid = rng.random((cy + 1, cx))
        gx = np.arange(W) / W * cx
        gy = np.arange(H) / H * cy
        x0 = np.floor(gx).astype(int)
        y0 = np.floor(gy).astype(int)
        tx = gx - x0
        ty = gy - y0
        tx = tx * tx * (3 - 2 * tx)
        ty = ty * ty * (3 - 2 * ty)
        x1 = (x0 + 1) % cx
        a = grid[y0][:, x0 % cx]
        b = grid[y0][:, x1]
        c = grid[np.minimum(y0 + 1, cy)][:, x0 % cx]
        d = grid[np.minimum(y0 + 1, cy)][:, x1]
        top = a + (b - a) * tx[None, :]
        bottom = c + (d - c) * tx[None, :]
        total += amp * (top + (bottom - top) * ty[:, None])
        norm += amp
        amp *= 0.5
    return total / norm


def build():
    dx, dy, dz = directions()
    img = np.zeros((H, W, 3))
    up = np.clip(dy, 0, 1)
    # Sky: deep blue at the zenith to a pale horizon (linear values).
    zenith = np.array([0.10, 0.24, 0.62])
    horizon = np.array([0.78, 0.86, 0.95])
    t = up ** 0.45
    sky = horizon[None, None, :] * (1 - t[..., None]) + zenith[None, None, :] * t[..., None]
    # Sun (direction of the template's light, reversed) with a glow.
    sun = np.array([0.457, 0.669, 0.586])
    sun /= np.linalg.norm(sun)
    cos_s = dx * sun[0] + dy * sun[1] + dz * sun[2]
    glow = np.clip(cos_s, 0, 1)
    sky += np.array([1.0, 0.85, 0.6])[None, None, :] * (0.35 * glow ** 12 + 0.8 * glow ** 200)[..., None]
    disc = np.clip((cos_s - 0.99985) / 0.00005, 0, 1)
    sky += np.array([1.0, 0.95, 0.85])[None, None, :] * (60.0 * disc)[..., None]
    # Clouds: noise thresholded, thicker toward the horizon, lit by the sun.
    n = tileable_noise(8, 4)
    cover = np.clip((n - 0.52) / 0.18, 0, 1) * np.clip(dy * 6.0, 0, 1) * (1 - 0.5 * up)
    cloud = np.array([0.95, 0.95, 0.97])[None, None, :] * (0.9 + 0.6 * glow ** 4)[..., None]
    sky = sky * (1 - cover[..., None]) + cloud * cover[..., None]
    # Ground: warm gray, darker toward the nadir, haze near the horizon.
    down = np.clip(-dy, 0, 1)
    ground = np.array([0.20, 0.17, 0.14])[None, None, :] * (1 - 0.5 * down[..., None])
    haze = np.exp(-down * 12.0)[..., None]
    ground = ground * (1 - haze) + horizon[None, None, :] * 0.6 * haze
    img = np.where((dy >= 0)[..., None], sky, ground)
    return img.astype(np.float32)


def rgbe(img):
    m = img.max(axis=2)
    out = np.zeros(img.shape[:2] + (4,), dtype=np.uint8)
    valid = m > 1e-32
    mant, exp = np.frexp(m)
    scale = np.where(valid, mant * 256.0 / np.where(valid, m, 1.0), 0.0)
    out[..., :3] = np.clip(img * scale[..., None], 0, 255).astype(np.uint8)
    out[..., 3] = np.where(valid, exp + 128, 0).astype(np.uint8)
    return out


def rle_component(data):
    """Radiance run-length encoding of one component of a scanline."""
    out = bytearray()
    i, n = 0, len(data)
    while i < n:
        run = 1
        while i + run < n and run < 127 and data[i + run] == data[i]:
            run += 1
        if run >= 4:
            out += bytes([128 + run, data[i]])
            i += run
            continue
        start = i
        while i < n and i - start < 128:
            if i + 3 < n and data[i] == data[i + 1] == data[i + 2] == data[i + 3]:
                break
            i += 1
        out += bytes([i - start]) + bytes(data[start:i])
    return out


def write_hdr(path, img):
    px = rgbe(img)
    out = bytearray(b"#?RADIANCE\n# AGR ENGINE template sky (procedural)\nFORMAT=32-bit_rle_rgbe\n\n")
    out += f"-Y {H} +X {W}\n".encode()
    for y in range(H):
        out += bytes([2, 2, W >> 8, W & 255])
        for c in range(4):
            out += rle_component(px[y, :, c].tobytes())
    with open(path, "wb") as f:
        f.write(out)
    return len(out)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    size = write_hdr(os.path.join(OUT, "afternoon.hdr"), build())
    print(f"afternoon.hdr: {W}x{H}, {size // 1024} KiB")
