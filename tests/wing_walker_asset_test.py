#!/usr/bin/env python3

import re
import struct
import math
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
    vertices = [tuple(map(float,line.split()[1:])) for line in vertex_lines]
    for vertex in vertices:
        assert len(vertex) == 8 and all(math.isfinite(v) for v in vertex)
        assert abs(sum(v*v for v in vertex[3:6])-1) < .001
        assert all(0 <= v <= 1 for v in vertex[6:8])

    indices = []
    for line in lines:
        if line.startswith("IDX10\t") or line.startswith("IDX\t"):
            assert len(line.split()) == (11 if line.startswith("IDX10") else 2)
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
    assert len(tris) == 3
    next_index = 0
    for start, count in tris:
        assert start == next_index
        assert count // 3 <= 10000, "Keep each held pose within the low-poly budget"
        points = [vertices[i] for i in indices[start:start+count]]
        assert abs(min(p[1] for p in points)) < .001, "Feet must sit on the ground"
        assert 1.8 < max(p[1] for p in points) < 2.6
        next_index += count
    assert next_index == index_count
    assert vertex_count < index_count // 2, "Export should share identical vertex records"

    # Standby wands must both slope down at 45 degrees. Fit their cylindrical
    # tip/handle ends using the exported orange material and each side of X.
    start,count = tris[1]
    standby = [vertices[i] for i in set(indices[start:start+count])]
    for side in (-1,1):
        wand = [v for v in standby if v[0]*side > .3 and
                abs(v[6]-.84375)<1e-6 and abs(v[7]-.75)<1e-6]
        assert len(wand) >= 20
        # The projected cylinder bounds expand equally for a 45 degree axis.
        dx=max(v[0] for v in wand)-min(v[0] for v in wand)
        dy=max(v[1] for v in wand)-min(v[1] for v in wand)
        assert abs(dx-dy) < .002

    # Only the clear signal has a green wand; its raised tube is vertical.
    green_uv=(.90625,.75)
    for pose_index,(start,count) in enumerate(tris):
        points=[vertices[i] for i in set(indices[start:start+count])]
        green=[v for v in points if abs(v[6]-green_uv[0])<1e-6 and
               abs(v[7]-green_uv[1])<1e-6]
        if pose_index != 2:
            assert not green
        else:
            assert len(green) >= 20, "Clear signal needs a green wand"
            assert min(v[1] for v in green) > 2.0
            assert max(v[1] for v in green)-min(v[1] for v in green) > .28
            assert max(v[0] for v in green)-min(v[0] for v in green) < .04
            assert max(v[2] for v in green)-min(v[2] for v in green) < .04

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
