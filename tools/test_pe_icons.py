"""tools/pe_icons.py on a small Windows executable built as halo.exe's resources are (port/windows/halo.rc, with
LLVM's resource compiler and lld-link): its icons 1 and 2 trade places, nothing else changes, and a checksum stays
right. Skipped without clang, llvm-rc and lld-link."""

import shutil
import struct
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import pe_icons  # noqa: E402

TOOLS = ("clang", "llvm-rc", "lld-link")
pytestmark = pytest.mark.skipif(any(shutil.which(tool) is None for tool in TOOLS),
                                reason="needs clang, llvm-rc and lld-link")

ICONS = (ROOT / "port/windows/opence-icon.ico", ROOT / "port/windows/build-1749-icon.ico")


def build(directory: Path, icons, release: bool = False) -> Path:
    """An executable holding icons (1, 2, ...), as halo.exe holds its own."""
    rc = directory / "test.rc"
    rc.write_text("".join(f'{index} ICON "{icon.as_posix()}"\n' for index, icon in enumerate(icons, 1)))
    source = directory / "test.c"
    source.write_text("int start(void) { return 0; }\n")
    subprocess.run(["llvm-rc", "/no-preprocess", "/FO", str(directory / "test.res"), str(rc)], check=True)
    subprocess.run(["clang", "--target=i686-pc-windows-msvc", "-c", str(source), "-o", str(directory / "test.obj")],
                   check=True)
    exe = directory / "test.exe"
    subprocess.run(["lld-link", "/nologo", "/entry:start", "/subsystem:console", "/nodefaultlib", "/machine:x86",
                    *(["/release"] if release else []), f"/out:{exe}", str(directory / "test.obj"),
                    str(directory / "test.res")], check=True)
    return exe


def group_data(data: bytes, group: int) -> bytes:
    """An icon group's GRPICONDIR (which of the RT_ICON images it is made of)."""
    root, _ = pe_icons._resource_section(data)
    _, offset = pe_icons.icon_groups(data)[group]
    languages = root + (offset & 0x7FFFFFFF)
    _, _, entry_offset = next(pe_icons._entries(data, languages))
    rva, size = struct.unpack_from("<II", data, root + entry_offset)
    resource_rva = struct.unpack_from("<I", data, pe_icons._resource_section(data)[1] + 96 + 16)[0]
    return data[root + rva - resource_rva:root + rva - resource_rva + size]


def checksum_offset(data: bytes) -> int:
    return pe_icons._resource_section(data)[1] + pe_icons.CHECKSUM_OFFSET


def test_icons_trade_places(tmp_path):
    exe = build(tmp_path, ICONS)
    before = exe.read_bytes()
    first, second = group_data(before, 1), group_data(before, 2)
    assert first != second

    pe_icons.swap_icons(exe)
    after = exe.read_bytes()
    assert group_data(after, 1) == second, "icon 1 is not build 1749's"
    assert group_data(after, 2) == first
    changed = [index for index, (a, b) in enumerate(zip(before, after)) if a != b]
    assert len(before) == len(after) and len(changed) <= 8, f"{len(changed)} bytes changed, not just two entries"

    pe_icons.swap_icons(exe)
    assert exe.read_bytes() == before, "trading back is not the original"


def test_checksum_stays_right(tmp_path):
    exe = build(tmp_path, ICONS, release=True)
    data = exe.read_bytes()
    stored = struct.unpack_from("<I", data, checksum_offset(data))[0]
    assert stored and pe_icons.pe_checksum(data, checksum_offset(data)) == stored, "not lld-link's checksum"

    pe_icons.swap_icons(exe)
    data = exe.read_bytes()
    assert pe_icons.pe_checksum(data, checksum_offset(data)) == stored, "the checksum no longer matches"


def test_one_icon_refused(tmp_path):
    exe = build(tmp_path, ICONS[:1])
    before = exe.read_bytes()
    with pytest.raises(pe_icons.PeError, match="no icon 2"):
        pe_icons.swap_icons(exe)
    assert exe.read_bytes() == before
    assert pe_icons.main(["pe_icons.py", str(exe)]) == 1
