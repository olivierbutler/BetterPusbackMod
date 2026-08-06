#!/usr/bin/env python3

import re
import struct
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
ASSET_DIR = REPO / "objects" / "wing_walker"
OBJ_PATH = ASSET_DIR / "wing_walker.obj"
SIGNAL_DATAREF = "bp/anim/wing_walker_signal"
EXPECTED_TEXTURES = ("wing_walker.png", "wing_walker_lit.png")
EXPECTED_POSES = {
    (0.75, 1.25),
    (1.75, 2.25),
    (2.75, 3.25),
}


def png_dimensions(path):
    data = path.read_bytes()[:24]
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    assert data[12:16] == b"IHDR", path
    return struct.unpack(">II", data[16:24])


def main():
    lines = OBJ_PATH.read_text(encoding="ascii").splitlines()
    assert lines[:3] == ["I", "800", "OBJ"]

    point_counts = next(line for line in lines
                        if line.startswith("POINT_COUNTS"))
    _, vertex_count, _, _, index_count = point_counts.split()
    vertex_count = int(vertex_count)
    index_count = int(index_count)

    vertex_lines = [line for line in lines if line.startswith("VT\t")]
    assert len(vertex_lines) == vertex_count

    indices = []
    for line in lines:
        if line.startswith("IDX10\t") or line.startswith("IDX\t"):
            indices.extend(int(value) for value in line.split()[1:])
    assert len(indices) == index_count
    assert min(indices) == 0
    assert max(indices) == vertex_count - 1

    show_ranges = set()
    hide_ranges = []
    tris = []
    for line in lines:
        if line.startswith("ANIM_hide\t"):
            fields = line.split()
            assert fields[3] == SIGNAL_DATAREF
            hide_ranges.append((float(fields[1]), float(fields[2])))
        elif line.startswith("ANIM_show\t"):
            fields = line.split()
            assert fields[3] == SIGNAL_DATAREF
            show_ranges.add((float(fields[1]), float(fields[2])))
        elif line.startswith("TRIS\t"):
            _, start, count = line.split()
            start, count = int(start), int(count)
            assert start >= 0 and count > 0 and count % 3 == 0
            assert start + count <= index_count
            tris.append((start, count))
    assert show_ranges == EXPECTED_POSES
    assert hide_ranges == [(0.0, 3.0)] * 3
    assert tris == [(0, 21216), (21216, 21216), (42432, 21216)]

    assert "TEXTURE\twing_walker.png" in lines
    assert "TEXTURE_LIT\twing_walker_lit.png" in lines
    assert any(re.match(r"LIGHT_PARAM\s+full_custom_halo\s+", line)
               for line in lines)

    for texture_name in EXPECTED_TEXTURES:
        texture_path = ASSET_DIR / texture_name
        width, height = png_dimensions(texture_path)
        assert width > 0 and height > 0
        assert width & (width - 1) == 0
        assert height & (height - 1) == 0

    attribution = (ASSET_DIR / "ATTRIBUTION.txt").read_text(
        encoding="utf-8")
    assert "Jungle Jim" in attribution
    assert "creativecommons.org/licenses/by/4.0" in attribution
    assert "Changes made for BetterPushback" in attribution

    header = (REPO / "src" / "wing_walker.h").read_text(encoding="utf-8")
    runtime = (REPO / "src" / "wing_walker.c").read_text(encoding="utf-8")
    plugin = (REPO / "src" / "xplane.c").read_text(encoding="utf-8")
    assert (f'#define WING_WALKER_SIGNAL_DATAREF "{SIGNAL_DATAREF}"'
            in header)
    assert "#define WING_WALKER_NOSE_CLEARANCE 27.432 /* 30 yards */" in runtime
    assert "WING_WALKER_SIGNAL_DATAREF," in runtime
    assert "dr_create_f(&wing_walker_signal_dr" in plugin
    assert "dr_delete(&wing_walker_signal_dr);" in plugin

    print("wing walker asset tests passed")


if __name__ == "__main__":
    main()
