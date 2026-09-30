#!/usr/bin/env python3
"""Execute the Classic route branches extracted from the production sources."""

from pathlib import Path
import os
import re
import subprocess
import tempfile


def block(source, start):
    opening = source.index("{", start)
    depth = 1
    tokens = re.finditer(
        r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\}|\{',
        source[opening + 1:], re.DOTALL
    )
    for token in tokens:
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[start:opening + 1 + token.end()]
    raise AssertionError("Unterminated source block")


def function(source, name):
    match = re.search(r"^(?:static\s+)?(?:void|bool_t|route_t\s*\*)\s*"
        + re.escape(name) + r"\([^)]*\)\s*\{", source, re.MULTILINE)
    assert match is not None, name
    return block(source, match.start())


repo = Path(__file__).resolve().parent.parent
camera = (repo / "src/bp_cam.c").read_text()
controller = (repo / "src/bp.c").read_text()
planner = function(camera, "bp_cam_start")
start = planner.index("if (!emergency_tow_allows_persistent_routes())")
emergency = block(planner, start)
classic_start = planner.index("if (bp_classic_mode())", start)
classic = block(planner, classic_start)
assert re.search(r"dr_t\s+lat, lon;", camera)
assert 'fdr_find(&drs.lat, "sim/flightmodel/position/latitude")' in camera
assert 'fdr_find(&drs.lon, "sim/flightmodel/position/longitude")' in camera

bp_start = function(controller, "bp_start")
start_save = block(bp_start, bp_start.index("if (bp_classic_mode()"))
lift = function(controller, "pb_step_lift")
late = block(lift, lift.index("if (late_plan_requested)"))
late_save = block(late, late.index("if (!slave_mode)"))
assert late.index("if (!late_plan_end_cond())") < late.index("route_save_legacy(")
assert block(late, late.index("if (!late_plan_end_cond())")).endswith("return;\n        }")
assert bp_start.index("if (!bp_can_start(") < bp_start.index("route_save_legacy(")
assert controller.count("route_save_legacy(&bp.segs);") == 2
assert "route_save_legacy(" not in function(camera, "bp_cam_stop")

# Keep the existing cache APIs unchanged; Classic uses its tolerant wrappers.
driving = (repo / "src/driving.c").read_text()
original = subprocess.check_output(
    ["git", "show", "v1.13:src/driving.c"], cwd=repo, text=True)
for name in ("route_load", "route_save"):
    assert function(driving, name) == function(original, name), name
upstream = subprocess.check_output(
    ["git", "show", "v1.14:src/driving.c"], cwd=repo, text=True)
assert block(driving, driving.index("static int\nroute_table_compar")) == block(
    upstream, upstream.index("static int\nroute_table_compar"))

extracted = "\n\n".join((
    "static void planner_open(void) {\n" + emergency + " else " + classic
        + " else { ++default_planner_calls; }\n}",
    "static void start_push(void) {\n" + start_save + "\n}",
    function(controller, "late_plan_end_cond"),
    "static void accept_late_plan(void) {\n" + late + "\n}",
))
with tempfile.TemporaryDirectory(prefix="bpb-classic-routes-") as temp:
    test_dir = Path(temp)
    (test_dir / "classic_routes.inc").write_text(extracted)
    binary = test_dir / "classic-routes-test"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror",
        f"-I{test_dir}", str(repo / "tests/classic_route_test.c"),
        "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
    (test_dir / "legacy_route_cache.inc").write_text("\n\n".join(
        function(driving, name) for name in (
            "legacy_route_matches", "legacy_route_find",
            "route_save_legacy", "route_load_legacy")))
    binary = test_dir / "legacy-route-cache-test"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror",
        f"-I{test_dir}", str(repo / "tests/legacy_route_cache_test.c"),
        "-lm", "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
