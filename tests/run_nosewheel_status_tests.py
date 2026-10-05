#!/usr/bin/env python3
"""Exercise the real read-only publisher and capture/release producers."""

import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(
        r"^(?:static\s+)?(?:void|bool_t|int)\s+" + re.escape(name)
        + r"\([^)]*\)\s*\{", source, re.MULTILINE
    )
    assert match is not None, name
    depth = 1
    for token in re.finditer(
        r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\}|\{',
        source[match.end():], re.DOTALL
    ):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():match.end() + token.end()]
    raise AssertionError(name)


repo = Path(__file__).resolve().parent.parent
source = (repo / "src/bp.c").read_text()
header = (repo / "src/bp.h").read_text()
plugin = (repo / "src/xplane.c").read_text()
metadata = source[source.index("/* Stable semantic values"):
                  source.index("static bool_t cfg_disco_when_done")]
reasons = header[header.index("/* Version-1 nosewheel status reasons"):
                 header.index("bool_t bp_init(void);")]
names = (
    "bp_boot_init", "bp_shut_fini", "pb_enter_ungrabbing",
    "pb_step_connect_grab", "pb_step_connect_winch", "pb_step_lift",
    "pb_step_lowering", "pb_step_ungrabbing_grab",
    "pb_step_ungrabbing_winch", "recon_handler"
)
roll_off = source[source.index("        case PB_STEP_MOVING_AWAY:",
                               source.index("static float\nbp_run(")):
                  source.index("        case PB_STEP_CLOSING_CRADLE:",
                               source.index("static float\nbp_run("))]
roll_off = "static void run_roll_off(void) { switch (bp.step) {\n" + roll_off \
    + "default: assert(false); } }\n"

# An observational extension must leave existing mutation sites untouched.
baseline = subprocess.check_output(
    ["git", "show", "f8cd6251a428e7fc16e2d9a1ced1980973fd7198:src/bp.c"],
    cwd=repo, text=True
)
for pattern in (
    r"\b(?:dr_set\w*|brakes_set|push_at_speed|turn_nosewheel)\s*\(",
    r"bp\.(?:step\s*(?:\+\+|=)|anim\.nosewheel_rot_spd\s*=)",
    r"dr_getvf32\(&drs\.tire_rot_spd"
):
    def mutation_sites(text):
        return [re.sub(r"\s+", " ", line.replace("rate_count = ", "")).strip()
                for line in text.splitlines() if re.search(pattern, line)]
    assert mutation_sites(source) == mutation_sites(baseline), pattern
assert "crc64_rand" not in metadata and "crc64_srand" not in metadata
for name in ("bp_start", "bp_stop", "bp_state_init", "bp_fini", "bp_complete"):
    assert "nw_" in function(source, name), name
assert source.index("nw_completed();", source.index("static void\nbp_complete(")) \
    < source.index("if (!bp_started)", source.index("static void\nbp_complete("))
assert re.search(r"if \(bp\.cur_t - bp\.last_t < MIN_STEP_TIME\) \{\s*"
                 r"nw_publish\(\);\s*return \(-1\);", source)
for reason in ("AIRCRAFT_RESET", "PROVIDER_DISABLED", "CORE_RELOAD",
               "INITIALIZATION_FAILED", "HARD_ABORT"):
    assert "bp_nosewheel_status_invalidate(BP_NW_" + reason + ");" in plugin

with tempfile.TemporaryDirectory(prefix="bpb-nosewheel-status-") as temp:
    temp = Path(temp)
    (temp / "nosewheel_reasons.inc").write_text(reasons)
    (temp / "nosewheel_producers.inc").write_text(
        metadata + "\n\n" + "\n\n".join(function(source, name) for name in names)
        + "\n\n" + roll_off
    )
    modes = ([], ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"])
    for index, flags in enumerate(modes):
        binary = temp / ("status-test-" + str(index))
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror",
            *flags, f"-I{repo / 'src'}", f"-I{temp}",
            str(repo / "tests/nosewheel_status_test.c"), "-lm", "-o", str(binary)
        ], check=True)
        subprocess.run([str(binary)], check=True)
print("Nosewheel producer contract and mutation-site parity passed.")
