#!/usr/bin/env python3
"""Convert a binary PPM (P6) to PNG using only the Python standard library.

The engine's `--capture-frame --capture-out out.ppm` writes a top-down P6 PPM.
This tiny converter lets you view those captures in VS Code / a browser.

Usage:
    python tools/ppm_to_png.py in.ppm [out.png]
"""

import struct
import sys
import zlib


def read_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    # Header: magic, then whitespace-separated width/height/maxval, then a
    # single whitespace before the binary raster.
    pos = 0
    tokens = []
    while len(tokens) < 4:
        while data[pos:pos + 1].isspace():
            pos += 1
        start = pos
        while pos < len(data) and not data[pos:pos + 1].isspace():
            pos += 1
        tok = data[start:pos]
        if tok.startswith(b"#"):
            # comment: skip to end of line
            while pos < len(data) and data[pos:pos + 1] not in (b"\n", b"\r"):
                pos += 1
            continue
        tokens.append(tok)
    assert tokens[0] == b"P6", f"expected P6, got {tokens[0]!r}"
    width, height, maxval = (int(t) for t in tokens[1:4])
    while data[pos:pos + 1].isspace():
        pos += 1
    raster = data[pos:]
    expected = width * height * 3
    assert len(raster) >= expected, "raster shorter than expected"
    if maxval != 255:
        # Scale to 8-bit per channel.
        raster = bytes(v * 255 // maxval for v in raster[:expected])
    else:
        raster = raster[:expected]
    return width, height, raster


def write_png(path, width, height, rgb):
    def chunk(tag, payload):
        c = tag + payload
        return struct.pack(">I", len(payload)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    def paeth(a, b, c):
        p = a + b - c
        pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
        if pa <= pb and pa <= pc:
            return a
        return b if pb <= pc else c

    # Convert to 8-bit RGBA rows with per-row filter byte 0 (None) and byte 2
    # (Up) alternating is overkill; use filter 0 for simplicity.
    raw = bytearray()
    stride = width * 3
    for y in range(height):
        row = rgb[y * stride:(y + 1) * stride]
        cur = bytearray(row)
        # Sub filter (1): subtract the left neighbor -> better compression.
        raw.append(1)
        for x in range(stride):
            left = cur[x - 3] if x >= 3 else 0
            raw.append((cur[x] - left) & 0xFF)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)  # 8-bit RGB
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", ihdr)
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 6))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else src.rsplit(".", 1)[0] + ".png"
    width, height, rgb = read_ppm(src)
    write_png(dst, width, height, rgb)
    print(f"{src} ({width}x{height}) -> {dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
