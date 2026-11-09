"""Make another of an executable's icons its first (tools/ci_build.py: halo1749.exe's).

halo.exe holds two icons (port/windows/halo.rc): its own, 1, and build 1749's, 2. halo1749.exe is a copy of it, and
Explorer, the taskbar and SDL's windows (SDL_HINT_WINDOWS_INTRESOURCE_ICON, port/linux/src/sdl_platform.c) show
icon 1, so in the copy the two icon groups' entries in the resource directory trade places: icon 1 is then build
1749's. Nothing else changes: the code and its debug directory are halo.exe's, so halo.pdb still reads its crashes,
and the header's checksum (a sum of the file's 16-bit words, which trading two of its aligned entries leaves as it
was) stays right.

    python tools/pe_icons.py halo1749.exe [first second]
"""

import struct
import sys
from pathlib import Path

RT_GROUP_ICON = 14
CHECKSUM_OFFSET = 64  # in the optional header, PE32 and PE32+ alike (test_pe_icons.py checks it)


class PeError(Exception):
    pass


def _resource_section(data: bytes):
    """The resource directory's file offset and RVA, and how RVAs in it map to the file."""
    if data[:2] != b"MZ":
        raise PeError("not an executable (no MZ)")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise PeError("not a PE executable")
    section_count, optional_size = struct.unpack_from("<H12xH", data, pe + 6)
    optional = pe + 24
    magic = struct.unpack_from("<H", data, optional)[0]
    directories = optional + {0x10B: 96, 0x20B: 112}.get(magic, -1)
    if directories < optional:
        raise PeError(f"unknown optional header {magic:#x}")
    resource_rva, resource_size = struct.unpack_from("<II", data, directories + 2 * 8)
    if not resource_rva:
        raise PeError("no resources")
    sections = optional + optional_size
    for index in range(section_count):
        virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from(
            "<IIII", data, sections + index * 40 + 8)
        if virtual_address <= resource_rva < virtual_address + max(virtual_size, raw_size):
            return raw_offset + resource_rva - virtual_address, optional
    raise PeError("the resources are in no section")


def _entries(data: bytes, directory: int):
    """A resource directory's entries: (file offset of the entry, id or None for a named one, offset to data)."""
    named, ids = struct.unpack_from("<HH", data, directory + 12)
    for index in range(named + ids):
        entry = directory + 16 + index * 8
        name, offset = struct.unpack_from("<II", data, entry)
        yield entry, (None if name & 0x80000000 else name), offset


def icon_groups(data: bytes) -> dict:
    """The executable's icon groups: id -> the offset its directory entry points to."""
    root, _ = _resource_section(data)
    for _, type_id, offset in _entries(data, root):
        if type_id == RT_GROUP_ICON:
            if not offset & 0x80000000:
                raise PeError("the icon groups are not a directory")
            groups = {}
            for entry, group_id, group_offset in _entries(data, root + (offset & 0x7FFFFFFF)):
                if group_id is not None:
                    groups[group_id] = (entry, group_offset)
            return groups
    raise PeError("no icon groups")


def pe_checksum(data: bytes, checksum_offset: int) -> int:
    """The PE header's checksum of data (its own field counted as 0)."""
    total = 0
    padded = data + b"\0" * (len(data) & 1)
    for (word,) in struct.iter_unpack("<H", padded):
        total += word
    total -= struct.unpack_from("<H", data, checksum_offset)[0] + struct.unpack_from("<H", data, checksum_offset + 2)[0]
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return (total + len(data)) & 0xFFFFFFFF


def swap_icons(path: Path, first: int = 1, second: int = 2) -> None:
    """Trades the icon groups first and second of the executable at path."""
    data = bytearray(path.read_bytes())
    groups = icon_groups(bytes(data))
    for group in (first, second):
        if group not in groups:
            raise PeError(f"{path.name} has no icon {group} (it has {sorted(groups)})")
    (first_entry, first_offset), (second_entry, second_offset) = groups[first], groups[second]
    struct.pack_into("<I", data, first_entry + 4, second_offset)
    struct.pack_into("<I", data, second_entry + 4, first_offset)
    path.write_bytes(bytes(data))


def main(argv: list) -> int:
    if len(argv) not in (2, 4):
        print(__doc__.strip().splitlines()[-1].strip(), file=sys.stderr)
        return 2
    first, second = (int(argv[2]), int(argv[3])) if len(argv) == 4 else (1, 2)
    try:
        swap_icons(Path(argv[1]), first, second)
    except (OSError, PeError) as error:
        print(f"pe_icons: {error}", file=sys.stderr)
        return 1
    print(f"{argv[1]}: icons {first} and {second} traded")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
