#!/usr/bin/env python3
"""MEngine unattended smoke test (P5).

Runs the sandbox/editor headlessly against the verification scenes and
asserts on mengine.log markers right after each run (every process truncates
the log, so assertions must be read back immediately). Exit 0 = all green.

Usage:
    python tools/run_smoke.py [--preset windows-clang-debug]
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def run_read(exe, args, cwd=None, timeout=120):
    proc = subprocess.run([exe] + args, capture_output=True, timeout=timeout, cwd=cwd or ROOT)
    log_path = os.path.join(cwd or ROOT, "mengine.log")
    lines = []
    try:
        with open(log_path, encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
    except OSError:
        pass
    return proc, lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preset", default="windows-clang-debug")
    args = ap.parse_args()
    exe_dir = os.path.join(ROOT, "build", args.preset)
    sandbox = os.path.join(exe_dir, "sandbox", "sandbox.exe")
    editor = os.path.join(exe_dir, "editor", "editor.exe")
    ok = True

    def check(name, cond, detail=""):
        nonlocal ok
        ok = ok and cond
        print(f"[{'PASS' if cond else 'FAIL'}] {name}" + (f"  ({detail})" if detail else ""))

    # 1. physics test: sensor/impact evidence. Unthrottled windows render at
    # thousands of fps, so a frame budget gives almost no simulated time;
    # instead run for ~20 s of wall time (physics advances with real dt).
    try:
        run_read(sandbox, ["--scene", "assets/scenes/physics_test.scene", "--frames", "1000000", "--hidden"],
                 timeout=20)
    except subprocess.TimeoutExpired:
        pass  # expected: killed by the wall-clock budget after running fine
    lines = []
    try:
        with open(os.path.join(ROOT, "mengine.log"), encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
    except OSError:
        pass
    joined = "\n".join(lines)
    check("physics_test ran 20 s (wall clock)", len(lines) > 5)
    check("physics_test sensor enter events", "sensor:" in joined and "entered" in joined)
    check("physics_test impact events", "impact:" in joined)
    check("physics_test no FATAL/ERROR", "] [FATAL]" not in joined and "] [ERROR]" not in joined)

    # 2. stress render scene: stats sane (few draws; culled + visible == total).
    proc, lines = run_read(sandbox, ["--scene", "assets/scenes/stress_cull.scene", "--frames", "600", "--hidden"])
    joined = "\n".join(lines)
    check("stress_cull clean exit", proc.returncode == 0, f"rc={proc.returncode}")
    stats = [l for l in lines if "[RenderStats]" in l]
    m = re.search(r"drawcalls=(\d+) instanced=(\d+) triangles=(\d+) culled=(\d+) visible_main=(\d+)", stats[-1]) if stats else None
    if m:
        dc, inst, tri, culled, visible = map(int, m.groups())
        check("stress_cull batching stats", dc <= 16 and inst == dc and culled + visible == 1600,
              f"dc={dc} inst={inst} tris={tri} culled={culled} visible={visible}")
    else:
        check("stress_cull stats row", False)

    # 3. editor smoke (fonts/assets resolve relative to the exe directory).
    editor_cwd = os.path.join(exe_dir, "editor")
    proc, lines = run_read(editor, ["--frames", "200", "--hidden"], cwd=editor_cwd)
    joined = "\n".join(lines)
    check("editor clean exit", proc.returncode == 0, f"rc={proc.returncode}")
    check("editor initialized + stats", "Editor initialized" in joined and "[RenderStats]" in joined)

    print("SMOKE:", "ALL PASS" if ok else "FAILURES PRESENT")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
