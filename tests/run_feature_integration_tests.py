#!/usr/bin/env python3
"""Test actual preference migration and post-push gates with SDK stubs."""

import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r"^(?:static\s+)?(?:void|bool_t)\s+" + re.escape(name)
                      + r"\([^)]*\)\s*\{", source, re.MULTILINE)
    assert match is not None, name
    depth = 1
    for token in re.finditer(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\}|\{',
                             source[match.end():], re.DOTALL):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():match.end() + token.end()]
    raise AssertionError(name)


repo = Path(__file__).resolve().parent.parent
controller = (repo / "src/bp.c").read_text()
config = (repo / "src/cfg.cpp").read_text()
upstream = subprocess.check_output([
    "git", "show", "377fddf968d0008cc9ef45b8e91e52bba2987369:src/bp.c"
], cwd=repo, text=True)
names = ("pb_step_waiting4ok2disco", "pb_step_clear_signal")
reference = []
for name in names:
    reference.append(function(upstream, name).replace(name, "reference_" + name))
assert "bp_classic_mode" not in config
assert 'conf_set_b(bp_conf, "legacy_route_recall", legacy_routes)' in config
assert 'conf_set_b(bp_conf, "fast_ground_handling", fast_ground_handling)' in config
assert config.index("migrate_classic_preferences() &&") < config.index("fetchGitVersion();")
camera = (repo / "src/bp_cam.c").read_text()
for msgid in (
    "Legacy route recall", "Fast Ground Handling", "Release brake pedals",
    "Waiting for brake pedals to be released (or abort pushback)"
):
    assert msgid in (repo / "data/po/strings.pot").read_text(), msgid
assert "bp_get_interface_mode" not in function(camera, "bp_cam_start")
assert "bp_fast_ground_handling" not in function(camera, "bp_cam_start")

with tempfile.TemporaryDirectory(prefix="bpb-feature-integration-") as temp:
    temp = Path(temp)
    (temp / "feature_preferences.inc").write_text("\n\n".join(
        function(config, name) for name in (
            "migrate_classic_preferences", "bp_legacy_routes",
            "bp_fast_ground_handling")))
    (temp / "post_push_controller.inc").write_text("\n\n".join(
        [function(controller, name) for name in names] + reference))
    for test in ("feature_preferences_test", "post_push_controller_test"):
        binary = temp / test
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror",
            f"-I{repo / 'src'}", f"-I{temp}", str(repo / "tests" / (test + ".c")),
            str(repo / "src/interface_mode.c"), "-lm", "-o", str(binary)
        ], check=True)
        subprocess.run([str(binary)], check=True)
