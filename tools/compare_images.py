#!/usr/bin/env python3
"""Compares two renders of the same frame (e.g. OpenGL vs Vulkan).

Usage: compare_images.py A.png B.png [--max-mean 1.0] [--max-pixels 0.5]
Fails when the mean absolute channel difference exceeds --max-mean (0..255) or
when more than --max-pixels percent of pixels differ by more than 16 on a channel.
Small differences are expected (rasterization rules, MSAA sample positions).
"""
import argparse
import sys

from PIL import Image, ImageChops


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--max-mean", type=float, default=1.0)
    ap.add_argument("--max-pixels", type=float, default=0.5)
    args = ap.parse_args()
    a = Image.open(args.a).convert("RGB")
    b = Image.open(args.b).convert("RGB")
    if a.size != b.size:
        print(f"size mismatch: {a.size} vs {b.size}")
        return 1
    raw = ImageChops.difference(a, b).tobytes()  # RGB, 3 bytes per pixel
    n = len(raw) // 3
    mean = sum(raw) / len(raw)
    big = sum(1 for i in range(0, len(raw), 3) if max(raw[i], raw[i + 1], raw[i + 2]) > 16) * 100.0 / n
    print(f"{args.a} vs {args.b}: mean |diff| = {mean:.3f}, pixels differing > 16: {big:.3f} %")
    return 0 if mean <= args.max_mean and big <= args.max_pixels else 1


if __name__ == "__main__":
    sys.exit(main())
