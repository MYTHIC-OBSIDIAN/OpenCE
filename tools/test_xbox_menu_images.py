"""xbox_menu_images.py on a small Xbox ui.map made here: which pictures it
finds unused, and how it writes them."""
import struct
import subprocess
import sys
import zlib
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import hud_assets  # noqa: E402
import xbox_menu_images  # noqa: E402

BASE = 0x803A6000
A8R8G8B8 = 11


def fake_ui_map(path: Path) -> dict:
    """An Xbox cache file (version 5, its body compressed) of a scenario, two
    widgets and four bitmaps: one a main menu screen's (replaced by the PC
    menus), one an error screen's (the game's code opens it), one the code
    names itself, one nothing uses. Returns each bitmap's pixels."""
    tags = [("scnr", "levels\\ui\\ui"),
            ("DeLa", "ui\\shell\\main_menu\\fake\\fake_screen"),
            ("bitm", "ui\\shell\\fake\\fake_screen_picture"),
            ("DeLa", "ui\\shell\\error\\error_modal_fullscreen"),
            ("bitm", "ui\\shell\\fake\\error_picture"),
            ("bitm", "ui\\shell\\bitmaps\\menu_bkds"),
            ("bitm", "ui\\shell\\fake\\nothing_uses_it")]
    uses = {1: [2], 3: [4]}
    count = len(tags)
    instances = 0x40
    names_at = instances + count * 0x20
    names = b""
    name_addresses = []
    for _, name in tags:
        name_addresses.append(BASE + names_at + len(names))
        names += name.encode() + b"\0"
    data_at = (names_at + len(names) + 15) & ~15
    datums = [0xE1740000 + i for i in range(count)]
    groups = [struct.unpack("<I", group[::-1].encode())[0] for group, _ in tags]

    body, addresses, pixels = b"", [], {}
    pixel_blobs = []
    for number, (group, name) in enumerate(tags):
        address = BASE + data_at + len(body)
        addresses.append(address)
        if group == "bitm":
            header = bytearray(0x6C)
            struct.pack_into("<II", header, 0x60, 1, address + 0x6C)
            image = np.random.default_rng(number).integers(0, 256, (4, 4, 4), np.uint8)
            pixels[name] = image
            swizzled = np.zeros((16, 4), np.uint8)
            order = hud_assets.morton_order(4, 4)
            swizzled[order.reshape(-1)] = image[..., [2, 1, 0, 3]].reshape(-1, 4)
            pixel_blobs.append((number, swizzled.tobytes()))
            entry = bytearray(0x30)
            struct.pack_into("<hhhhhH", entry, 4, 4, 4, 1, 0, A8R8G8B8, 0)
            body += bytes(header) + bytes(entry)
        else:
            chunk = bytearray(0x40)
            for slot, target in enumerate(uses.get(number, [])):
                struct.pack_into("<IIII", chunk, 0x10 + slot * 16, groups[target], name_addresses[target], 0,
                                 datums[target])
            body += bytes(chunk)
    tag_data = bytearray(data_at) + body
    struct.pack_into("<IIII", tag_data, 0, BASE + instances, datums[0], 0, count)
    for number in range(count):
        struct.pack_into("<4sIIIIIII", tag_data, instances + number * 0x20, tags[number][0][::-1].encode(),
                         0xFFFFFFFF, 0xFFFFFFFF, datums[number], name_addresses[number], addresses[number], 0, 0)
    tag_data[names_at:names_at + len(names)] = names

    full = bytearray(0x800)
    tag_offset = 0x800
    full += tag_data
    for number, blob in pixel_blobs:
        offset = len(full)
        full += blob
        entry = addresses[number] - BASE + 0x6C + tag_offset
        struct.pack_into("<ii", full, entry + 0x18, offset, len(blob))
    full[:4] = b"daeh"
    struct.pack_into("<ii", full, 4, 5, len(full))
    struct.pack_into("<ii", full, 0x10, tag_offset, len(tag_data))
    full[0x20:0x22] = b"ui"
    path.write_bytes(bytes(full[:0x800]) + zlib.compress(bytes(full[0x800:])))
    return pixels


def test_unused_pictures_found_and_written(tmp_path):
    pixels = fake_ui_map(tmp_path / "ui.map")
    out = tmp_path / "out"
    subprocess.run([sys.executable, str(ROOT / "tools/xbox_menu_images.py"), str(tmp_path), "--out", str(out)],
                   check=True)
    listed = (out / "unused.md").read_text()
    # the main menu screen's (the PC menus stand in for it) and nothing's
    assert "ui\\shell\\fake\\fake_screen_picture" in listed
    assert "ui\\shell\\fake\\nothing_uses_it" in listed
    # the error screen's (the code opens it) and the code's own
    assert "error_picture" not in listed and "menu_bkds" not in listed
    for name in ("fake_screen_picture", "nothing_uses_it"):
        written = np.asarray(Image.open(out / "ui/shell/fake" / f"{name}__0.png").convert("RGBA"))
        assert (written == pixels[f"ui\\shell\\fake\\{name}"]).all()
    assert not (out / "ui/shell/fake/error_picture__0.png").exists()


def test_references_found_in_tag_data(tmp_path):
    fake_ui_map(tmp_path / "ui.map")
    cache = xbox_menu_images.CacheTags(tmp_path / "ui.map")
    by_name = {tag["name"]: number for number, tag in enumerate(cache.tags)}
    screen = by_name["ui\\shell\\main_menu\\fake\\fake_screen"]
    assert cache.references[screen] == {by_name["ui\\shell\\fake\\fake_screen_picture"]}
    assert cache.references[by_name["levels\\ui\\ui"]] == set()


def test_frames_the_menus_take_are_drawn():
    whole, frames = xbox_menu_images.menu_names()
    # (the profile menu's pictures: the PC version's frames of its own, by index)
    assert any(name.endswith("player_profile_edit\\profile_options") for name, _ in frames)
    assert "ui\\shell\\bitmaps\\sp_levels" in whole
    assert "ui\\english" in xbox_menu_images.code_names()
