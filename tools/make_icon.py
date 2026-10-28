"""Generates resources/DrDirectory.ico (a cushion-shaded mini treemap) with no third-party deps."""
import math, struct, zlib, os

BLOCKS = [  # x0, y0, x1, y1 in unit space, color
    (0.00, 0.00, 0.58, 0.62, (34, 150, 230)),
    (0.58, 0.00, 1.00, 0.38, (26, 188, 180)),
    (0.58, 0.38, 1.00, 0.62, (90, 112, 200)),
    (0.00, 0.62, 0.36, 1.00, (40, 200, 235)),
    (0.36, 0.62, 0.70, 1.00, (80, 100, 128)),
    (0.70, 0.62, 1.00, 1.00, (30, 110, 170)),
]
BG = (12, 20, 30)
EDGE = (34, 211, 238)
L = (-1.0, -1.0, 10.0)
LN = math.sqrt(sum(c * c for c in L))
L = tuple(c / LN for c in L)

def sample(u, v):
    # rounded-square mask
    r = 0.2
    cx = min(max(u, r), 1 - r); cy = min(max(v, r), 1 - r)
    d = math.sqrt((u - cx) ** 2 + (v - cy) ** 2)
    if d > r:
        return None
    edge = min(u, v, 1 - u, 1 - v) if d == 0 else r - d
    if edge < 0.035:
        return EDGE
    m = 0.10; g = 0.025
    if u < m or v < m or u > 1 - m or v > 1 - m:
        return BG
    iu = (u - m) / (1 - 2 * m); iv = (v - m) / (1 - 2 * m)
    for x0, y0, x1, y1, col in BLOCKS:
        if x0 + g / 2 <= iu < x1 - g / 2 and y0 + g / 2 <= iv < y1 - g / 2:
            a = (iu - x0) / (x1 - x0); b = (iv - y0) / (y1 - y0)
            h = 0.9
            nx = -h * 4 * (1 - 2 * a); ny = -h * 4 * (1 - 2 * b)
            cosa = (-nx * L[0] - ny * L[1] + L[2]) / math.sqrt(nx * nx + ny * ny + 1)
            p = 0.18 + 0.82 * max(0.0, min(1.0, cosa))
            p *= 1.08
            return tuple(min(255, int(c * p)) for c in col)
    return BG

def render(size):
    ss = 4 if size <= 64 else 2
    rows = []
    for y in range(size):
        row = bytearray()
        for x in range(size):
            acc = [0, 0, 0, 0]
            for sy in range(ss):
                for sx in range(ss):
                    c = sample((x + (sx + 0.5) / ss) / size, (y + (sy + 0.5) / ss) / size)
                    if c is not None:
                        acc[0] += c[0]; acc[1] += c[1]; acc[2] += c[2]; acc[3] += 255
            n = ss * ss
            a = acc[3] / n
            if a > 0:
                k = 255.0 / acc[3] * n / n
                row += bytes([int(acc[0] / (acc[3] / 255)), int(acc[1] / (acc[3] / 255)), int(acc[2] / (acc[3] / 255)), int(a)])
            else:
                row += bytes([0, 0, 0, 0])
        rows.append(row)
    return rows

def png(size, rows):
    raw = b''.join(b'\x00' + bytes(r) for r in rows)
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

sizes = [256, 64, 48, 32, 24, 16]
images = [png(s, render(s)) for s in sizes]
header = struct.pack('<HHH', 0, 1, len(images))
offset = 6 + 16 * len(images)
entries = b''; data = b''
for s, img in zip(sizes, images):
    entries += struct.pack('<BBBBHHII', 0 if s >= 256 else s, 0 if s >= 256 else s, 0, 0, 1, 32, len(img), offset)
    offset += len(img); data += img
out = os.path.join(os.path.dirname(__file__), '..', 'resources', 'DrDirectory.ico')
with open(out, 'wb') as f:
    f.write(header + entries + data)
with open(os.path.join(os.path.dirname(__file__), '..', 'resources', 'icon_preview.png'), 'wb') as f:
    f.write(images[0])
print('wrote', os.path.abspath(out))
