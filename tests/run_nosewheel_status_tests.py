#!/usr/bin/env python3
"""Exercise the real read-only publisher and capture/release producers."""

import os
from pathlib import Path
import hashlib
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

# An observational extension must leave the reviewed controller mutation sites
# untouched.  Keep the compact signatures in-tree so a fresh checkout can run
# this contract without fetching an object from a contributor's repository.
mutation_contract = (
    (r"\b(?:dr_set\w*|brakes_set|push_at_speed|turn_nosewheel)\s*\(",
     70, "9483ab540f2162edb5a9e24da3ce90c6cd4de6dcf806e710284f124b0ce294f6",
     "dr_setf(&drs.contract_probe, 1);"),
    (r"bp\.(?:step\s*(?:\+\+|=)|anim\.nosewheel_rot_spd\s*=)",
     44, "a256eee33495d59c8bc382d0b775d1c5526c06faa642fcedb48c2c76dfc3ce6e",
     "bp.step++;"),
    (r"dr_getvf32\(&drs\.tire_rot_spd",
     1, "e222170cb70e727ce77d6f61ab4ae8f166220a990c1a5d2c041500b0822e19b6",
     "dr_getvf32(&drs.tire_rot_spd, &contract_probe, 0, 1);"),
)

for pattern, expected_count, expected_digest, probe in mutation_contract:
    def mutation_sites(text):
        return [re.sub(r"\s+", " ", line.replace("rate_count = ", "")).strip()
                for line in text.splitlines() if re.search(pattern, line)]

    sites = mutation_sites(source)
    digest = hashlib.sha256("\n".join(sites).encode()).hexdigest()
    assert len(sites) == expected_count, (pattern, len(sites))
    assert digest == expected_digest, (pattern, digest)
    assert len(mutation_sites(source + "\n" + probe)) == expected_count + 1
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
