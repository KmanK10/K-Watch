"""Builds the K-Watch UI simulator with Zig's C compiler.

    python sim/build.py            build, then open the simulator window
    python sim/build.py --shots    build, then save screenshots of every screen to sim/shots
    python sim/build.py --build    only build

LVGL is compiled once and cached in sim/build; our own code is rebuilt every time.
"""

import concurrent.futures
import glob
import os
import shutil
import subprocess
import sys

SIM = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(SIM)
FW = os.path.join(ROOT, "firmware")
LVGL = os.path.join(FW, "managed_components", "lvgl__lvgl")
BUILD = os.path.join(SIM, "build")
EXE = os.path.join(BUILD, "k-watch-sim.exe")

INCLUDES = [SIM, os.path.join(SIM, "stubs"), os.path.join(FW, "src"), os.path.join(FW, "src", "hal"), LVGL]
CFLAGS = ["-target", "x86_64-windows-gnu", "-std=gnu11", "-O2", "-g0",
          "-DLV_CONF_INCLUDE_SIMPLE", "-Wno-everything"]
OUR_CFLAGS = ["-Wall", "-Wno-unused-function"]


def find_zig():
    zig = shutil.which("zig")
    if zig:
        return zig
    pattern = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Microsoft", "WinGet", "Packages",
                           "zig.zig_*", "*", "zig.exe")
    found = glob.glob(pattern)
    if not found:
        sys.exit("Zig not found. Install it with: winget install zig.zig")
    return found[0]


def obj_path(src):
    rel = os.path.relpath(src, ROOT).replace(os.sep, "_").replace("..", "up")
    return os.path.join(BUILD, "obj", rel[:-2] + ".o")


def compile_one(zig, src, ours):
    obj = obj_path(src)
    flags = CFLAGS[:-1] + OUR_CFLAGS if ours else CFLAGS
    cmd = [zig, "cc", *flags, *[f"-I{i}" for i in INCLUDES], "-c", src, "-o", obj]
    result = subprocess.run(cmd, capture_output=True, text=True)
    return src, result.returncode, result.stdout + result.stderr


def main():
    zig = find_zig()
    os.makedirs(os.path.join(BUILD, "obj"), exist_ok=True)

    lvgl_sources = glob.glob(os.path.join(LVGL, "src", "**", "*.c"), recursive=True)
    our_sources = (glob.glob(os.path.join(FW, "src", "ui", "*.c")) +
                   [os.path.join(FW, "src", "settings.c")] +
                   glob.glob(os.path.join(SIM, "*.c")))

    conf_time = os.path.getmtime(os.path.join(SIM, "lv_conf.h"))
    todo = [(s, False) for s in lvgl_sources
            if not os.path.exists(obj_path(s)) or os.path.getmtime(obj_path(s)) < conf_time]
    todo += [(s, True) for s in our_sources]
    if len(todo) > len(our_sources):
        print(f"Compiling LVGL ({len(todo) - len(our_sources)} files, only needed once)...")

    failed = False
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as pool:
        for src, code, output in pool.map(lambda t: compile_one(zig, *t), todo):
            if output.strip():
                print(output.strip())
            if code != 0:
                failed = True
    if failed:
        sys.exit("Build failed.")

    objs = [obj_path(s) for s in lvgl_sources + our_sources]
    rsp = os.path.join(BUILD, "link.rsp")
    with open(rsp, "w") as f:
        f.write("\n".join(o.replace("\\", "/") for o in objs))
    cmd = [zig, "cc", "-target", "x86_64-windows-gnu", "-o", EXE, f"@{rsp}",
           "-lgdi32", "-luser32", "-lwinmm"]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout + result.stderr)
        sys.exit("Link failed.")
    print(f"Built {os.path.relpath(EXE, ROOT)}")

    if "--build" in sys.argv:
        return
    if "--shots" in sys.argv:
        out = os.path.join(SIM, "shots")
        subprocess.run([EXE, "--shots", out], check=True)
    else:
        subprocess.run([EXE])


if __name__ == "__main__":
    main()
