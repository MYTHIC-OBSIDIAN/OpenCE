#!/usr/bin/env python3
"""Writes the pictures of Halo PC's own multiplayer maps (Gearbox's six:
Danger Canyon, Death Island, Gephyrophobia, Ice Fields, Infinity and
Timberland) as the <name>.bmp files the map lists show beside each map
(port/linux/game/custom_edition_maps.c), from the player's own Halo PC or
Halo Custom Edition install:

    python tools/pc_map_pictures.py <install>/maps <game>/custom_maps

reads <install>/maps/ui.map (and its bitmaps.map, where the pictures' pixels
or tags are kept there) and writes icefields.bmp, deathisland.bmp and so on
into the second folder, where the maps themselves go. Nothing is shipped
with the game: the pictures are the player's own, as the Xbox maps' are.

The menus' level pictures are ui\\shell\\bitmaps\\mp_map_grafix, one frame
for each map in the order of the menus' map names (mp_map_list): the Xbox's
13, then Halo PC's six (FRAMES). --all writes every frame as
mp_map_grafix_<n>.bmp instead, to check them by eye.

Python 3 and Pillow. The maps are read as untrusted input: every offset is
checked against the file before it is used.
"""

import argparse
import struct
import sys
from pathlib import Path

from PIL import Image

# Halo PC's own maps' frames in mp_map_grafix (their names' indices in the
# PC menus' mp_map_list: port/assets/menus/ce/strings.xml)
FRAMES = {
    "icefields": 13, "deathisland": 14, "dangercanyon": 15,
    "infinity": 16, "timberland": 17, "gephyrophobia": 18,
}
PICTURES_TAG = "ui\\shell\\bitmaps\\mp_map_grafix"

HEAD, FOOT = 0x68656164, 0x666F6F74
# the address the PC and CE versions load tag data at
TAG_DATA_ADDRESS = 0x40440000
VERSIONS = {7: "Halo PC", 609: "Halo Custom Edition"}

# bitmap data flags
FLAG_SWIZZLED = 1 << 3
FLAG_EXTERNAL = 1 << 8

# bitmap formats (bitmaps.h)
A8, Y8, AY8, A8Y8 = 0, 1, 2, 3
R5G6B5, A1R5G5B5, A4R4G4B4 = 6, 8, 9
X8R8G8B8, A8R8G8B8 = 10, 11
DXT1, DXT3, DXT5 = 14, 15, 16
P8 = 17


class MapError(Exception):
    pass


class Data:
    """bytes read with bounds checks"""

    def __init__(self, data: bytes, name: str):
        self.data = data
        self.name = name

    def take(self, offset: int, size: int) -> bytes:
        if offset < 0 or size < 0 or offset + size > len(self.data):
            raise MapError(f"{self.name}: {size} bytes at {offset:#x} are past its end")
        return self.data[offset:offset + size]

    def u16(self, offset: int) -> int:
        return struct.unpack_from("<H", self.take(offset, 2))[0]

    def u32(self, offset: int) -> int:
        return struct.unpack_from("<I", self.take(offset, 4))[0]

    def cstring(self, offset: int, limit: int = 256) -> str:
        raw = self.take(offset, min(limit, len(self.data) - offset))
        return raw.split(b"\0", 1)[0].decode("latin-1")


class Cache:
    """a PC or CE cache file's tags"""

    def __init__(self, path: Path):
        self.file = Data(path.read_bytes(), path.name)
        if self.file.u32(0) != HEAD or self.file.u32(0x7FC) != FOOT:
            raise MapError(f"{path}: not a Halo cache file")
        self.version = self.file.u32(4)
        if self.version not in VERSIONS:
            raise MapError(f"{path}: version {self.version}, not Halo PC's or Custom Edition's (an Xbox map?)")
        self.tag_data_offset = self.file.u32(0x10)
        self.tag_data_size = self.file.u32(0x14)
        header = self.tag_data_offset
        self.tag_array = self.offset(self.file.u32(header))
        self.tag_count = self.file.u32(header + 0x0C)
        if self.tag_count > 65535:
            raise MapError(f"{path}: {self.tag_count} tags")

    def offset(self, address: int) -> int:
        """a tag data address's offset in the file"""
        offset = address - TAG_DATA_ADDRESS
        if offset < 0 or offset >= self.tag_data_size:
            raise MapError(f"{self.file.name}: the address {address:#x} is outside its tag data")
        return self.tag_data_offset + offset

    def find(self, group: bytes, name: str):
        """(data address or resource index, indexed) of a tag, else None"""
        group_value = struct.unpack(">I", group)[0]
        for index in range(self.tag_count):
            entry = self.tag_array + index * 32
            if self.file.u32(entry) != group_value:
                continue
            if self.file.cstring(self.offset(self.file.u32(entry + 0x10))).lower() == name.lower():
                return self.file.u32(entry + 0x14), self.file.u32(entry + 0x18) & 1
        return None


class Resources:
    """bitmaps.map: its resources (CE's indexed tags) and pixel data"""

    def __init__(self, path: Path):
        self.file = Data(path.read_bytes(), path.name)
        self.offsets = []
        count = self.file.u32(0x0C)
        table = self.file.u32(0x08)
        if count > 1 << 20:
            raise MapError(f"{path}: {count} resources")
        for index in range(count):
            entry = table + index * 12
            self.offsets.append((self.file.u32(entry + 8), self.file.u32(entry + 4)))


def bitmap_tag(cache: Cache, resources):
    """(the tag's bytes, the address its pointers are relative to, the file
    its pixels are in) of the level pictures"""
    found = cache.find(b"bitm", PICTURES_TAG)
    if not found:
        raise MapError(f"{cache.file.name} has no {PICTURES_TAG}")
    pointer, indexed = found
    if not indexed:
        return cache.file, cache.offset(pointer), None
    # (Custom Edition: the whole tag kept in bitmaps.map, its pointers
    # offsets from its start)
    if resources is None:
        raise MapError("the pictures are kept in bitmaps.map, which is not beside ui.map")
    if pointer >= len(resources.offsets):
        raise MapError(f"bitmaps.map has no resource {pointer}")
    start, _size = resources.offsets[pointer]
    return resources.file, start, start


def frames(cache: Cache, resources):
    """each frame's bitmap data entry's fields, in frame order"""
    data, start, relative = bitmap_tag(cache, resources)

    def to_offset(address: int) -> int:
        return relative + address if relative is not None else cache.offset(address)

    sequence_count, sequence_address = data.u32(start + 0x54), data.u32(start + 0x58)
    bitmap_count, bitmap_address = data.u32(start + 0x60), data.u32(start + 0x64)
    if bitmap_count > 4096 or sequence_count > 4096:
        raise MapError("the pictures tag is not a bitmap tag this tool reads")
    entries = []
    for index in range(bitmap_count):
        entry = to_offset(bitmap_address) + index * 0x30
        entries.append({
            "width": data.u16(entry + 0x04), "height": data.u16(entry + 0x06),
            "type": data.u16(entry + 0x0A), "format": data.u16(entry + 0x0C), "flags": data.u16(entry + 0x0E),
            "offset": data.u32(entry + 0x18), "size": data.u32(entry + 0x1C),
        })
    # (a frame is one of sequence 0's bitmaps, as bitmap_group_get_bitmap_from_sequence has it)
    if sequence_count:
        sequence = to_offset(sequence_address)
        first, count = data.u16(sequence + 0x20), data.u16(sequence + 0x22)
        entries = entries[first:first + count]
    return entries, data


def unswizzle(pixels: list, width: int, height: int) -> list:
    out = [None] * (width * height)
    for y in range(height):
        for x in range(width):
            index, bit, sx, sy = 0, 0, x, y
            w, h = width, height
            while w > 1 or h > 1:
                if w > 1:
                    index |= (sx & 1) << bit
                    sx >>= 1
                    w >>= 1
                    bit += 1
                if h > 1:
                    index |= (sy & 1) << bit
                    sy >>= 1
                    h >>= 1
                    bit += 1
            out[y * width + x] = pixels[index]
    return out


def rgb565(value: int):
    return ((value >> 11 & 31) * 255 // 31, (value >> 5 & 63) * 255 // 63, (value & 31) * 255 // 31)


def dxt_colors(block: bytes, alpha_one_bit: bool):
    c0, c1, bits = struct.unpack_from("<HHI", block)
    a, b = rgb565(c0), rgb565(c1)
    if c0 > c1 or not alpha_one_bit:
        palette = [a, b, tuple((2 * p + q) // 3 for p, q in zip(a, b)), tuple((p + 2 * q) // 3 for p, q in zip(a, b))]
    else:
        palette = [a, b, tuple((p + q) // 2 for p, q in zip(a, b)), (0, 0, 0)]
    return [palette[bits >> (2 * i) & 3] for i in range(16)]


def decode(raw: bytes, width: int, height: int, fmt: int, swizzled: bool) -> Image.Image:
    if fmt in (DXT1, DXT3, DXT5):
        block_size = 8 if fmt == DXT1 else 16
        image = Image.new("RGB", (width, height))
        put = image.load()
        blocks_wide = max(1, (width + 3) // 4)
        for block_index in range(blocks_wide * max(1, (height + 3) // 4)):
            block = raw[block_index * block_size:(block_index + 1) * block_size]
            if len(block) < block_size:
                break
            colors = dxt_colors(block[block_size - 8:], fmt == DXT1)
            bx, by = block_index % blocks_wide * 4, block_index // blocks_wide * 4
            for i, color in enumerate(colors):
                x, y = bx + i % 4, by + i // 4
                if x < width and y < height:
                    put[x, y] = color
        return image
    sizes = {A8: 1, Y8: 1, AY8: 1, A8Y8: 2, R5G6B5: 2, A1R5G5B5: 2, A4R4G4B4: 2, X8R8G8B8: 4, A8R8G8B8: 4}
    if fmt not in sizes:
        raise MapError(f"a picture of format {fmt}, which this tool does not read")
    step = sizes[fmt]
    pixels = []
    for index in range(width * height):
        chunk = raw[index * step:(index + 1) * step]
        if len(chunk) < step:
            chunk = bytes(step)
        if step == 4:
            pixels.append((chunk[2], chunk[1], chunk[0]))
        elif fmt == R5G6B5:
            pixels.append(rgb565(struct.unpack("<H", chunk)[0]))
        elif fmt == A1R5G5B5:
            v = struct.unpack("<H", chunk)[0]
            pixels.append(((v >> 10 & 31) * 255 // 31, (v >> 5 & 31) * 255 // 31, (v & 31) * 255 // 31))
        elif fmt == A4R4G4B4:
            v = struct.unpack("<H", chunk)[0]
            pixels.append(((v >> 8 & 15) * 17, (v >> 4 & 15) * 17, (v & 15) * 17))
        elif fmt == A8Y8:
            pixels.append((chunk[0],) * 3)
        else:
            pixels.append((chunk[0],) * 3)
    if swizzled:
        pixels = unswizzle(pixels, width, height)
    image = Image.new("RGB", (width, height))
    image.putdata(pixels)
    return image


def pixel_bytes(entry: dict, tag_file: Data, cache: Cache, resources) -> bytes:
    """the first mipmap's bytes, from the file the entry says"""
    if entry["flags"] & FLAG_EXTERNAL or tag_file is not cache.file:
        if resources is None:
            raise MapError("the pictures' pixels are in bitmaps.map, which is not beside ui.map")
        return resources.file.take(entry["offset"], entry["size"])
    return cache.file.take(entry["offset"], entry["size"])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("maps", type=Path, help="the Halo PC or Custom Edition install's maps folder")
    parser.add_argument("out", type=Path, help="the folder to write the pictures to (the game's custom_maps)")
    parser.add_argument("--all", action="store_true", help="write every frame, numbered, to check them")
    args = parser.parse_args()

    try:
        cache = Cache(args.maps / "ui.map")
        resources = Resources(args.maps / "bitmaps.map") if (args.maps / "bitmaps.map").exists() else None
        entries, tag_file = frames(cache, resources)
        print(f"{VERSIONS[cache.version]} ui.map: {len(entries)} level pictures")
        wanted = {f"mp_map_grafix_{n}": n for n in range(len(entries))} if args.all else FRAMES
        args.out.mkdir(parents=True, exist_ok=True)
        for name, frame in wanted.items():
            if frame >= len(entries):
                print(f"  {name}: no frame {frame}", file=sys.stderr)
                continue
            entry = entries[frame]
            image = decode(pixel_bytes(entry, tag_file, cache, resources), entry["width"], entry["height"],
                           entry["format"], bool(entry["flags"] & FLAG_SWIZZLED))
            image.save(args.out / f"{name}.bmp", "BMP")
            print(f"  {name}.bmp ({entry['width']}x{entry['height']})")
    except (MapError, OSError, struct.error) as problem:
        print(f"pc_map_pictures: {problem}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
