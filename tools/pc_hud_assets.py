"""Lay the Xbox's high-res HUD redraws out as Halo PC's own sheets.

Halo PC's retail maps (and so this LAN build's Gearbox maps) draw their
meters, counter digits and damage arrows from sheets laid out otherwise than
the Xbox's: four times the size, the sprites packed in other places
(hud_unit_meters and hud_ammo_meters 1024x512 where the Xbox's are 256x256).
The Xbox redraws were made from pictures in Halo PC's coordinates
(port/assets/hud/layout.json: each cell's `clip` is the PC sprite it was drawn
from, `source` where the Xbox cell's window onto it starts), so each cell goes
back to its PC sprite by a translation alone, at twice the PC sheet's size:
the texels, meter thresholds (red) and coverage (green) are copied as they
are, never resampled. Texels no cell covers take the colour of the nearest
one that is (as hud_assets.py bleeds), so that filtering at a sprite's edge
reads its own data.

    python tools/pc_hud_assets.py build

needs no game data: pc_layout.json gives each sheet's Halo PC size.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'port/assets/hud'
RECIPE = ASSETS / 'pc_layout.json'
LAYOUT = ASSETS / 'layout.json'


def build_asset(entry, sources):
    source_entry = sources[entry['from']]
    scale = entry['scale']
    image = np.zeros((entry['height'] * scale, entry['width'] * scale, 4), np.uint8)
    written = np.zeros(image.shape[:2], bool)
    with Image.open(ASSETS / (entry['from'] + '.png')) as picture:
        xbox = np.asarray(picture.convert('RGBA'))
    xbox_scale = source_entry['scale']
    for cell in source_entry['cells']:
        if 'clip' not in cell or 'source' not in cell:
            raise ValueError(f"{entry['from']}: a cell with no PC sprite to go back to")
        # (the redraw was drawn at the Xbox texture's scale over its PC
        # coordinates: that zoom must be this sheet's scale)
        if xbox_scale / cell['source_scale'] != scale:
            raise ValueError(f"{entry['from']}: drawn at {xbox_scale / cell['source_scale']}x, not {scale}x")
        left, top, right, bottom = [value * xbox_scale for value in cell['xbox']]
        cell_width, cell_height = right - left, bottom - top
        clip = [value * scale for value in cell['clip']]
        x_offset, y_offset = cell['source'][0] * scale, cell['source'][1] * scale
        # PC texel (x, y) in the clip is the cell's (x - x_offset, y - y_offset)
        x0, y0 = max(clip[0], x_offset), max(clip[1], y_offset)
        x1, y1 = min(clip[2], x_offset + cell_width), min(clip[3], y_offset + cell_height)
        if not (0 <= x0 < x1 <= image.shape[1] and 0 <= y0 < y1 <= image.shape[0]):
            raise ValueError(f"{entry['from']}: a cell outside the PC sheet")
        image[y0:y1, x0:x1] = xbox[top + y0 - y_offset:top + y1 - y_offset, left + x0 - x_offset:left + x1 - x_offset]
        written[y0:y1, x0:x1] = True
    if (~written).any():
        from scipy import ndimage

        _, (rows, columns) = ndimage.distance_transform_edt(~written, return_indices=True)
        bled = image[rows, columns]
        bled[..., 1] = 0  # (coverage: none where nothing is drawn)
        bled[..., 3] = 0
        image = np.where(written[..., None], image, bled)
    return image


def build():
    sources = {entry['name']: entry for entry in json.loads(LAYOUT.read_text())['assets']}
    for entry in json.loads(RECIPE.read_text())['assets']:
        image = build_asset(entry, sources)
        Image.fromarray(image, 'RGBA').save(ASSETS / (entry['name'] + '.png'), optimize=True)
        print(f"{entry['name']}.png: {image.shape[1]}x{image.shape[0]}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('command', choices=['build'])
    parser.parse_args()
    build()
