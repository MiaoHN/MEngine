#!/usr/bin/env python3
"""Compare two binary PPM (P6) screenshots pixel-by-pixel.

Usage: python tools/ppm_diff.py a.ppm b.ppm
Prints the mean/max absolute channel difference and the number of differing
pixels. Used for A/B regression checks (e.g. before/after a refactor).
"""
import sys


def read_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    # Header: P6 <width> <height> <maxval>, whitespace/comments separated.
    fields = []
    i = 0
    while len(fields) < 4:
        while i < len(data) and data[i:i + 1].isspace():
            i += 1
        if data[i:i + 1] == b"#":
            while i < len(data) and data[i:i + 1] != b"\n":
                i += 1
            continue
        start = i
        while i < len(data) and not data[i:i + 1].isspace():
            i += 1
        fields.append(data[start:i])
    w, h, maxval = int(fields[1]), int(fields[2]), int(fields[3])
    assert w > 0 and h > 0 and maxval == 255, f"unsupported PPM header {fields}"
    i += 1
    return w, h, data[i:i + w * h * 3]


def main():
    wa, ha, a = read_ppm(sys.argv[1])
    wb, hb, b = read_ppm(sys.argv[2])
    assert (wa, ha) == (wb, hb), f"size mismatch {(wa, ha)} vs {(wb, hb)}"
    diff = 0
    worst = 0
    changed = 0
    for i in range(0, len(a), 3):
        d = max(abs(a[i] - b[i]), abs(a[i + 1] - b[i + 1]), abs(a[i + 2] - b[i + 2]))
        diff += d
        worst = max(worst, d)
        if d > 0:
            changed += 1
    total = len(a) // 3
    print(f"{wa}x{ha}: mean_abs_diff={diff / total:.4f} max_diff={worst} "
          f"changed_pixels={changed}/{total} ({100.0 * changed / total:.3f}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
