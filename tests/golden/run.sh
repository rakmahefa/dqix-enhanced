#!/usr/bin/env bash
# Golden corpus capture (M3). One command: builds (if needed) and runs every
# case, writing frames + CRC fingerprints under tests/golden/corpus/<id>/.
#
# Usage:  tests/golden/run.sh [GAME=/path/to/game/dqix] [ROM=/path/to/dq9.nds]
# Requires a built game binary. If the game is missing this script prints the
# build command and exits 0 (so CI can skip gracefully).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
MANIFEST="$HERE/corpus.json"
GAME="${GAME:-$ROOT/game/dqix}"
ROM="${ROM:-$ROOT/rom/dq9.nds}"
EXTRACT="${EXTRACT:-$ROOT/game/data/code}"

if [[ ! -x "$GAME" ]]; then
  echo "golden corpus: game binary not found at $GAME"
  echo "build it first:  make build ROM=$ROM"
  exit 0
fi
if [[ ! -f "$ROM" ]]; then
  echo "golden corpus: ROM not found at $ROM" >&2
  exit 2
fi

python3 "$HERE/gen_cases.py"

# RTC fixed for reproducibility.
RTC=1760000000
CORPUS="$HERE/corpus"
mkdir -p "$CORPUS"

python3 - "$MANIFEST" "$HERE" "$GAME" "$ROM" "$EXTRACT" "$CORPUS" "$RTC" <<'PY'
import json, os, shutil, subprocess, sys
manifest, here, game, rom, extract, corpus, rtc = sys.argv[1:8]
m = json.load(open(manifest))
for case in m["cases"]:
    cid = case["id"]
    cdir = os.path.join(here, "cases", cid)
    out = os.path.join(corpus, cid)
    os.makedirs(out, exist_ok=True)
    frames = case.get("frames", m["default_frames"])
    ini = os.path.join(cdir, "settings.ini")
    inp = os.path.join(cdir, "input.txt")
    # copy settings.ini next to the game's working dir via DQIX_NO_SETTINGS? No:
    # the game reads ./settings.ini from its cwd, so run from a per-case dir.
    work = os.path.join(out, "work")
    os.makedirs(work, exist_ok=True)
    os.makedirs(os.path.join(work, "frames"), exist_ok=True)
    shutil.copyfile(ini, os.path.join(work, "settings.ini"))
    # empty input file is fine
    if os.path.getsize(inp) > 0:
        shutil.copyfile(inp, os.path.join(work, "input.txt"))
    else:
        open(os.path.join(work, "input.txt"), "w").close()
    # save file copy (optional)
    save = os.path.join(os.path.dirname(game), "dqix.sav")
    args = [game, rom, extract, str(frames + 1), "1"]
    if os.path.isfile(save):
        args.append(save)
    env = dict(os.environ)
    env["DQIX_RTC"] = rtc
    env["DQIX_INPUT_FILE"] = "input.txt"
    env["DQIX_CRC"] = "crc.txt"
    env["DQIX_DUMP"] = "frames,1"
    env["DQIX_DUMP_AT"] = ""  # capture every frame
    # remove DQIX_DUMP_AT (empty disables the filter, captures all)
    env.pop("DQIX_DUMP_AT", None)
    print(f"== running case {cid} ({frames} frames)")
    r = subprocess.run(args, cwd=work, env=env, capture_output=True, text=True)
    (open(os.path.join(out, "stdout.txt"), "w")).write(r.stdout)
    (open(os.path.join(out, "stderr.txt"), "w")).write(r.stderr)
    if r.returncode != 0:
        print(f"   case {cid} exited {r.returncode}", file=sys.stderr)
        continue
    # move artifacts out
    for name in ("crc.txt",):
        src = os.path.join(work, name)
        if os.path.isfile(src):
            shutil.copyfile(src, os.path.join(out, name))
    fsrc = os.path.join(work, "frames")
    fdst = os.path.join(out, "frames")
    if os.path.isdir(fsrc):
        if os.path.isdir(fdst):
            shutil.rmtree(fdst)
        shutil.move(fsrc, fdst)
    print(f"   case {cid}: done")
PY

echo "golden corpus captured under $CORPUS"
