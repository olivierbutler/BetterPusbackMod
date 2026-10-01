#!/usr/bin/env python3
"""Compile the real disconnect controller against deterministic SDK stubs."""

from pathlib import Path
import os
import re
import subprocess
import tempfile


def function_source(source, name):
    match = re.search(
        r"^static\s+(?:void|bool_t|int)\s+" + re.escape(name)
        + r"\([^)]*\)\s*\{", source, re.MULTILINE
    )
    if match is None:
        raise AssertionError(f"Controller function missing: {name}")
    depth = 1
    tokens = re.finditer(
        r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\}|\{',
        source[match.end():], re.DOTALL
    )
    for token in tokens:
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():match.end() + token.end()]
    raise AssertionError(f"Unterminated controller function: {name}")


repo = Path(__file__).resolve().parent.parent
source = (repo / "src/bp.c").read_text()
upstream = subprocess.check_output(
    ["git", "show", "377fddf968d0008cc9ef45b8e91e52bba2987369:src/bp.c"],
    cwd=repo, text=True
)
for name in ("pbrake_is_set", "brakes_set"):
    current_lines = [line.rstrip() for line in function_source(source, name).splitlines()]
    upstream_lines = [line.rstrip() for line in function_source(upstream, name).splitlines()]
    assert current_lines == upstream_lines, name

lowering = function_source(source, "pb_step_lowering")
assert "pb_enter_ungrabbing(B_TRUE);" in lowering
assert "bp_fast_brake_handoff_reset" not in function_source(source, "pb_enter_ungrabbing")
assert source.count("pb_enter_ungrabbing(B_FALSE);") == 1
assert re.search(
    r"if \(bp.awaiting_plan\) \{[^{}]*pb_enter_ungrabbing\(B_FALSE\);",
    source
)
assert not re.search(r"bp\.step\s*=\s*PB_STEP_UNGRABBING", source.replace(
    function_source(source, "pb_enter_ungrabbing"), ""
))

functions = (
    "pbrake_is_set", "brakes_set", "fast_brake_handoff_active",
    "fast_brake_handoff_ready", "bp_complete",
    "pb_enter_ungrabbing", "pb_step_stopped", "pb_step_lowering",
    "pb_step_ungrabbing_grab", "pb_step_ungrabbing_winch",
    "pb_step_ungrabbing", "recon_handler"
)
reference_names = (
    "pb_step_stopped", "pb_step_lowering", "pb_step_ungrabbing_grab",
    "pb_step_ungrabbing_winch", "pb_step_ungrabbing"
)
reference_functions = []
for name in reference_names:
    body = function_source(upstream, name)
    for original in reference_names:
        body = re.sub(r"\b" + original + r"\b", "reference_" + original, body)
    reference_functions.append(body)
with tempfile.TemporaryDirectory(prefix="bpb-fast-brake-controller-") as temp:
    test_dir = Path(temp)
    (test_dir / "fast_brake_handoff_controller.inc").write_text(
        "\n\n".join([function_source(source, name) for name in functions]
            + reference_functions)
    )
    binary = test_dir / "controller-test"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror",
        f"-I{repo / 'src'}", f"-I{test_dir}",
        str(repo / "tests/fast_brake_handoff_controller_test.c"),
        "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
