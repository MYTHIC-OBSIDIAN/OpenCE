#!/usr/bin/env python3
"""Dump the Xbox game's menu pictures that this build does not draw.

The port's menus are the PC version's (port/assets/menus/ce), drawn from
their own PNGs and from a few of the Xbox ui.map's bitmaps, which the menu
files name (<frame map="ui\\..." index=...>, bitmap="ui\\...",
widget="ui\\..."). The game's code draws some more of the map's own (the
in-game button pictures, the level pictures, the error screens, the
virtual keyboard). Every other picture of the Xbox's menus is in
ui.map but never shown. This writes those out as PNGs, with a list.

    python3 tools/xbox_menu_images.py <maps folder or ui.map> --out <folder>

Which pictures the build draws:

- the menu files' (port/assets/menus/ce/*.xml): a bitmap they name whole,
  and the widgets they name with everything those widgets use; of a
  bitmap they take single frames of (<frame map= index=>), only those
  frames;
- every tag the game's and the port's C sources name ("ui\\\\..."), with
  everything it uses (the error screens, the virtual keyboard, the button
  pictures), but the main menu's screens (ui\\shell\\main_menu\\...), which
  the PC menus stand in for;
- everything the map's scenario and globals use other than through the
  Xbox menus' widgets (fonts, the HUD, the rasterizer's).

The Xbox menus' pictures are the bitmaps their widgets use, and any other
bitmap under ui\\shell\\. --all dumps every bitmap of the map that is not
drawn. --list writes only the list.

A tag's references are found in its data (a tag reference: the group, the
name's address and the tag's index, as the cache file holds them), which
the cache file lays out one tag after another.

Needs NumPy and Pillow. Reads the Xbox's ui.map (or a PC one, for its
list only: the PC's pictures are in bitmaps.map).
"""

import argparse
import bisect
import re
import struct
import sys
import zlib
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hud_assets  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
MENUS = ROOT / "port/assets/menus/ce"
SOURCES = (ROOT / "source", ROOT / "port")

# where each version's tag data is loaded (cache_files.c)
XBOX_VERSION, PC_VERSION, CUSTOM_EDITION_VERSION = 5, 7, 609
TAG_BASES = {XBOX_VERSION: 0x803A6000, PC_VERSION: 0x40440000, CUSTOM_EDITION_VERSION: 0x40440000}
WIDGET_GROUP = "DeLa"
# (and their collections: a level's, and the main menu's)
COLLECTION_GROUP = "Soul"
BITMAP_GROUP = "bitm"
# the map's own roots: its scenario and its globals
ROOT_GROUPS = ("scnr", "matg")
MENU_FOLDER = "ui\\shell\\"
# the Xbox menus' screens the PC menus stand in for, all of them, and their
# collection (ui\\shell\\main_menu): the code's names of these (the main
# menu's, display.menus = "xbox"'s; the screens pc_menus_screen gives the PC
# menus' for) open the PC menus' screens, so only the widgets the menu files
# name are drawn
REPLACED_FOLDER = "ui\\shell\\main_menu\\"

BITMAP_TYPES = {0: "2d", 1: "3d", 2: "cube", 3: "white"}
FORMATS = dict(hud_assets.FORMATS)
FORMATS[17] = "p8"


class CacheTags:
    """A cache file's tags: their groups, names, indices and data."""

    def __init__(self, path: Path):
        raw = path.read_bytes()
        if raw[:4] != b"daeh":
            raise SystemExit(f"{path}: not a Halo cache file")
        self.version, = struct.unpack_from("<i", raw, 4)
        if self.version not in TAG_BASES:
            raise SystemExit(f"{path}: cache file version {self.version} is not the Xbox's, the PC's or Custom Edition's")
        self.xbox = self.version == XBOX_VERSION
        self.data = raw[:0x800] + zlib.decompressobj().decompress(raw[0x800:]) if self.xbox else raw
        self.base = TAG_BASES[self.version]
        offset, size = struct.unpack_from("<ii", self.data, 0x10)
        self.tags_data = self.data[offset:offset + size]
        instances, = struct.unpack_from("<I", self.tags_data, 0)
        count, = struct.unpack_from("<I", self.tags_data, 0xC)
        self.tags = []
        for index in range(count):
            group, _, _, datum, name, address, external, _ = struct.unpack(
                "<4sIIIIIII", self.read(instances + index * 0x20, 0x20))
            self.tags.append({
                "group": group[::-1].decode("latin-1"),
                "group_value": struct.unpack("<I", group)[0],
                "datum": datum,
                "name_address": name,
                "name": self.read(name, 256).split(b"\0")[0].decode("latin-1"),
                "address": address,
                # (a PC map's bitmaps and sounds kept in bitmaps.map and sounds.map)
                "external": bool(external) and not self.xbox,
            })
        self.by_name = {(tag["group"], tag["name"].lower()): number for number, tag in enumerate(self.tags)}
        self.references = self.find_references()

    def read(self, address: int, size: int) -> bytes:
        offset = address - self.base
        return self.tags_data[offset:offset + size] if 0 <= offset < len(self.tags_data) else b""

    def find_references(self) -> list:
        """Each tag's references to others, as found in its data."""
        words = np.frombuffer(self.tags_data[:len(self.tags_data) // 4 * 4], "<u4")
        by_datum = {tag["datum"]: number for number, tag in enumerate(self.tags)}
        datums = np.array(sorted(by_datum), np.uint32)
        # (a reference: the group, the name's address, the name's length, the index)
        candidates = np.nonzero(np.isin(words[3:], datums))[0]
        owners = sorted((tag["address"], number) for number, tag in enumerate(self.tags)
                        if not tag["external"] and self.read(tag["address"], 1))
        starts = [address for address, _ in owners]
        references = [set() for _ in self.tags]
        for at in candidates:
            target = by_datum[int(words[at + 3])]
            tag = self.tags[target]
            if int(words[at]) != tag["group_value"] or int(words[at + 1]) != tag["name_address"]:
                continue
            address = self.base + int(at) * 4
            owner = bisect.bisect_right(starts, address) - 1
            if owner >= 0 and owners[owner][1] != target:
                references[owners[owner][1]].add(target)
        return references

    def closure(self, roots, blocked: tuple = ()) -> set:
        """The tags the roots use, and the roots; not through a blocked group's."""
        seen, stack = set(), list(roots)
        while stack:
            number = stack.pop()
            if number in seen:
                continue
            seen.add(number)
            if self.tags[number]["group"] in blocked:
                continue
            stack.extend(self.references[number] - seen)
        return seen

    def find(self, name: str):
        """A tag by its name, whatever its group."""
        name = name.lower()
        for (group, tag_name), number in self.by_name.items():
            if tag_name == name:
                yield number

    def bitmaps(self, number: int) -> list:
        """A bitmap tag's bitmaps (hud_assets.XboxMap.bitmap_group's fields)."""
        header = self.read(self.tags[number]["address"], 0x6C)
        bitmap_count, bitmaps = struct.unpack_from("<II", header, 0x60)
        result = []
        for index in range(bitmap_count):
            bitmap = self.read(bitmaps + index * 0x30, 0x30)
            width, height, depth, kind, format, flags = struct.unpack_from("<hhhhhH", bitmap, 4)
            offset, size = struct.unpack_from("<ii", bitmap, 0x18)
            result.append({"width": width, "height": height, "depth": depth, "type": kind, "format": format,
                           "flags": flags, "offset": offset, "size": size})
        return result


# ---------- what the build draws


def menu_names() -> tuple:
    """The map's tags the menu files name whole, and the frames they take
    one by one: ({name}, {(bitmap name, index)})."""
    whole, frames = set(), set()
    for path in sorted(MENUS.glob("*.xml")):
        text = path.read_text(encoding="utf-8")
        for element in re.finditer(r"<(\w+)([^>]*)>", text):
            tag, attributes = element.group(1), dict(re.findall(r'(\w+)="([^"]*)"', element.group(2)))
            for key, value in attributes.items():
                if "\\" not in value:
                    continue
                if tag == "frame" and key == "map":
                    frames.add((value.lower(), int(attributes.get("index", "0"))))
                else:
                    whole.add(value.lower())
    return whole, frames


def code_names() -> set:
    """Every tag name the C sources hold ("ui\\\\..."): the ones the game
    and the port load themselves."""
    names = set()
    for folder in SOURCES:
        for path in folder.rglob("*"):
            if path.suffix not in (".c", ".h", ".inl"):
                continue
            for literal in re.findall(r'"((?:levels|ui|sound|effects|weapons|vehicles|characters|cinematics|scenery|'
                                      r'globals)\\\\[^"\n]+)"', path.read_text(encoding="utf-8", errors="replace")):
                names.add(literal.replace("\\\\", "\\").lower())
    return names


def drawn(cache: CacheTags) -> tuple:
    """The tags the build draws whole, and the frames it draws of others."""
    whole_names, frames = menu_names()
    roots = set()
    for name in whole_names:
        roots.update(cache.find(name))
    for name in code_names():
        roots.update(number for number in cache.find(name)
                     if not (cache.tags[number]["group"] in (WIDGET_GROUP, COLLECTION_GROUP) and
                             (name + "\\").startswith(REPLACED_FOLDER)))
    used = cache.closure(roots)
    used |= cache.closure((number for number, tag in enumerate(cache.tags) if tag["group"] in ROOT_GROUPS),
                          blocked=(WIDGET_GROUP,))
    return used, frames


def menu_bitmaps(cache: CacheTags, every: bool) -> list:
    """The Xbox menus' bitmaps (every bitmap: every), by name."""
    if every:
        chosen = {number for number, tag in enumerate(cache.tags) if tag["group"] == BITMAP_GROUP}
    else:
        widgets = [number for number, tag in enumerate(cache.tags) if tag["group"] == WIDGET_GROUP]
        chosen = {number for number in cache.closure(widgets) if cache.tags[number]["group"] == BITMAP_GROUP}
        chosen |= {number for number, tag in enumerate(cache.tags)
                   if tag["group"] == BITMAP_GROUP and tag["name"].lower().startswith(MENU_FOLDER)}
    return sorted(chosen, key=lambda number: cache.tags[number]["name"].lower())


# ---------- pictures


def decode(cache: CacheTags, bitmap: dict) -> np.ndarray:
    """A bitmap's first level (a 3D one's first slice, a cube map's first
    face) as RGBA, as the Xbox decodes it."""
    kind = FORMATS.get(bitmap["format"])
    width, height = bitmap["width"], bitmap["height"]
    pixels = cache.data[bitmap["offset"]:bitmap["offset"] + bitmap["size"]]
    if kind in ("r5g6b5", "a1r5g5b5"):
        order = (np.arange(width * height).reshape(height, width) if bitmap["flags"] & hud_assets.BITMAP_LINEAR_FLAG
                 else hud_assets.morton_order(width, height))
        value = np.frombuffer(pixels[:width * height * 2], "<u2")[order].astype(np.uint32)
        image = np.zeros((height, width, 4), np.uint8)
        if kind == "r5g6b5":
            image[..., 0] = (value >> 11 & 31) * 255 // 31
            image[..., 1] = (value >> 5 & 63) * 255 // 63
            image[..., 2] = (value & 31) * 255 // 31
            image[..., 3] = 255
        else:
            image[..., 0] = (value >> 10 & 31) * 255 // 31
            image[..., 1] = (value >> 5 & 31) * 255 // 31
            image[..., 2] = (value & 31) * 255 // 31
            image[..., 3] = (value >> 15) * 255
        return image
    if kind is None or kind == "p8":
        raise ValueError(f"no decoder for format {bitmap['format']}")
    return hud_assets.decode_bitmap({"width": width, "height": height, "format": bitmap["format"],
                                     "flags": bitmap["flags"], "pixels": pixels})


def file_name(name: str, index: int) -> Path:
    """ui\\shell\\bitmaps\\a_butn's frame 0: ui/shell/bitmaps/a_butn__0.png,
    as the menus' own PNGs are named."""
    parts = [re.sub(r'[<>:"|?*]', "_", part) for part in name.split("\\")]
    return Path(*parts[:-1]) / f"{parts[-1]}__{index}.png"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("map", type=Path, help="the Xbox's ui.map, or the maps folder that holds it")
    parser.add_argument("--out", type=Path, default=Path("unused_menu_images"), help="where to write them")
    parser.add_argument("--all", action="store_true", help="every bitmap of the map the build does not draw")
    parser.add_argument("--list", action="store_true", help="write the list only, no pictures")
    arguments = parser.parse_args()

    path = arguments.map / "ui.map" if arguments.map.is_dir() else arguments.map
    cache = CacheTags(path)
    if not cache.xbox and not arguments.list:
        print(f"{path} is the PC version's (its pictures are in bitmaps.map): writing the list only")
        arguments.list = True
    used, frames = drawn(cache)

    rows, written, problems = [], 0, []
    for number in menu_bitmaps(cache, arguments.all):
        tag = cache.tags[number]
        if number in used:
            continue
        name = tag["name"]
        bitmaps = cache.bitmaps(number)
        for index, bitmap in enumerate(bitmaps):
            if (name.lower(), index) in frames:
                continue
            some = any(frame_name == name.lower() for frame_name, _ in frames)
            why = "other frames of it are drawn" if some else "nothing the build shows draws it"
            output = file_name(name, index)
            details = (f"{bitmap['width']}x{bitmap['height']}", FORMATS.get(bitmap["format"], str(bitmap["format"])),
                       BITMAP_TYPES.get(bitmap["type"], str(bitmap["type"])))
            if not arguments.list:
                try:
                    image = decode(cache, bitmap)
                except (ValueError, KeyError) as problem:
                    problems.append(f"{name} frame {index}: {problem}")
                    output = None
                else:
                    (arguments.out / output).parent.mkdir(parents=True, exist_ok=True)
                    Image.fromarray(image, "RGBA").save(arguments.out / output)
                    written += 1
            rows.append((name, index, *details, why, output))

    arguments.out.mkdir(parents=True, exist_ok=True)
    lines = ["# The Xbox menus' pictures this build does not draw", "",
             f"From `{path.name}` (cache file version {cache.version}). "
             f"{len(rows)} pictures in {len({row[0] for row in rows})} bitmaps.", "",
             "| Bitmap | Frame | Size | Format | Type | Why | File |", "| --- | --- | --- | --- | --- | --- | --- |"]
    for name, index, size, kind, shape, why, output in rows:
        lines.append(f"| `{name}` | {index} | {size} | {kind} | {shape} | {why} | "
                     f"{'`' + output.as_posix() + '`' if output else ''} |")
    if problems:
        lines += ["", "## Not written", ""] + [f"- {problem}" for problem in problems]
    (arguments.out / "unused.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{len(rows)} unused pictures in {len({row[0] for row in rows})} bitmaps; "
          f"{written} PNGs written; the list is {arguments.out / 'unused.md'}")
    for problem in problems:
        print(f"not written: {problem}")


if __name__ == "__main__":
    main()
