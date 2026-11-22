#!/usr/bin/env python3
"""Takes the black background off the achievement icons (assets\\icons\\<n>.png):
the black reached from the edges, through dark pixels only, becomes
transparent, with a soft matte; dark parts inside the picture stay.

    python3 clear_icons.py <in.png> <out.png>
"""
import sys
from PIL import Image
from collections import deque
LO, HI = 10, 40
src, dst = sys.argv[1], sys.argv[2]
im = Image.open(src).convert('RGB'); w, h = im.size; px = im.load()
reach = [[False]*w for _ in range(h)]
q = deque()
for x in range(w):
    for y in (0, h-1): q.append((x, y))
for y in range(h):
    for x in (0, w-1): q.append((x, y))
while q:
    x, y = q.popleft()
    if reach[y][x] or max(px[x, y]) > HI: continue
    reach[y][x] = True
    for dx, dy in ((1,0),(-1,0),(0,1),(0,-1)):
        nx, ny = x+dx, y+dy
        if 0 <= nx < w and 0 <= ny < h and not reach[ny][nx]: q.append((nx, ny))
out = Image.new('RGBA', (w, h)); op = out.load()
for y in range(h):
    for x in range(w):
        r, g, b = px[x, y]
        if reach[y][x]:
            m = max(r, g, b)
            a = 0.0 if m <= LO else (m - LO) / (HI - LO)
            if a <= 0: op[x, y] = (0, 0, 0, 0); continue
            # (un-matted from black: the colour as it would be, at that cover)
            op[x, y] = (min(255, int(r / a)), min(255, int(g / a)), min(255, int(b / a)), int(a * 255))
        else:
            op[x, y] = (r, g, b, 255)
out.save(dst)
