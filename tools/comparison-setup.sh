#!/bin/bash
# W413 (#475): set up the machine the comparison runs on (W354 stage 2, W378).
# Written for whoever runs it on the Ubuntu machine; docs/comparison-machine.md has the steps.
#
#   tools/comparison-setup.sh [--root DIR] MODE
#
# MODE (the default is all, in this order; each stops the run at the first failure):
#   --check     the tools: names every missing apt package in one install command, and uv
#               (with its install line). The script never runs sudo, apt or an installer:
#               installing is the maintainer's.
#   --sources   rebuilds reference/ (git-ignored): xppaut-8.0 from this repository,
#               xppaut-master and auto-07p cloned at their pinned commits, each tree
#               checked against a recorded hash. A tree that has the right hash is kept;
#               one that has not is a refusal (nothing is deleted, move it away).
#   --python    `uv sync --locked`: the studies' Python modules (numpy, matplotlib, mpmath) at
#               the versions uv.lock holds, in .venv of this checkout. The drivers run with
#               `uv run python ...` (docs/comparison-machine.md).
#   --build     builds xppautX, XPPAUT 8.0 and AUTO-07p under ROOT/build (a build with
#               its stamp is kept).
#   --record    writes the machine record (CPU, governor, turbo, compilers, source hashes)
#               to ROOT/provenance/machine-<UTC time>.json: copy it beside the results.
#   --ready     run before each timed series: refuses unless the CPU governor is
#               "performance" and no other process uses the CPU (the series needs the
#               machine to itself); changing the governor is the maintainer's (root).
#   --hashes    prints the tree hash of reference/'s three trees (to update the pins).
#
# ROOT (default ~/xppautx-comparison) holds the builds and records, on the machine's own
# disk; the sources are reference/ in this checkout. Nothing outside the checkout and
# ROOT is written, and nothing is deleted.
set -u
cd "$(dirname "$0")/.." || exit 1
REPO=$(pwd)
ROOT=$HOME/xppautx-comparison

# Pins. A tree hash is sha256 over the sorted (sha256 of the file with CRs removed, path)
# lines, without .git: the same on a checkout with CRLF endings and on Linux.
XPPAUT8_COMMIT=c021b511eee95a43576227656c6530e9af78ca27   # this repository: git archive
XPPAUT8_TREE=bbe57bb07e573228c433cbd0116b393d8e28cf5947802eae9bad3885d845778e
MASTER_URL=https://github.com/ermentrout/xppaut
MASTER_COMMIT=9068d8ac481b315d6b8ad5d1920aeeedde2c0bfd    # master's tip, 2016-02-03
MASTER_TREE=f90e07f0cabab4129e2d2f34f4658c32b6cb4d680f008fa3e7b7f47e506aa808
AUTO_URL=https://github.com/auto-07p/auto-07p.git
AUTO_COMMIT=44cc1c42a888179bdbd608a40613caee9cbc4675      # 0.9.4 (CHANGELOG; no 0.9.4 tag, so git describe says v0.9.3-82)
AUTO_TREE=b9ad9f995b8678b0d4b2b12c4650c0f68298c483f1e19be4e033704498b4629d
# W269's builds (paper/benchmarks/five-model-screen/provenance-w269.json, auto/results provenance)
AUTO_CONFIGURE="--enable-gui=no --enable-plaut=no --enable-plaut04=no"
XPPAUTX_MAKE=(xppautx WINDOW=0 "OPT=-g -O2" CCACHE= XPPAUTX_VERSION=paper-comparison)
LEGACY_CC="gcc -std=gnu99 -D_XOPEN_SOURCE=600 -w -Wno-error=implicit-function-declaration -Wno-error=int-conversion -Wno-error=incompatible-pointer-types -Wno-error=implicit-int -Wno-error=return-mismatch"
LEGACY_CFLAGS="-g -O2 -ffp-contract=off -fcommon -DNOERRNO -DNON_UNIX_STDIO -DAUTO -DCVODE_YES -DHAVEDLL -DMYSTR1=8.0 -DMYSTR2=1"
LEGACY_LDFLAGS=-fcommon
. tools/jobs.sh
IDLE_LOAD=0.3   # --ready: a 1-minute load below this means nothing else ran for the last minute

die() { echo "comparison-setup: $*" >&2; exit 1; }

modes=()
while [ $# -gt 0 ]; do
  case $1 in
    --root) [ $# -ge 2 ] || die "--root needs a folder"; ROOT=$2; shift ;;
    --check|--sources|--python|--build|--record|--ready|--hashes) modes+=("${1#--}") ;;
    -h|--help) sed -n '2,/^set -u/p' "$0" | sed '$d'; exit 0 ;;
    *) die "unknown argument $1 (see --help)" ;;
  esac
  shift
done
[ ${#modes[@]} -gt 0 ] || modes=(check sources python build record)
[ "$(uname -s)" = Linux ] || die "this runs on Linux (Ubuntu); this is $(uname -s)"
case $REPO in /mnt/*) die "run it from a checkout on the machine's own disk, not $REPO (9P is slow and skews builds)" ;; esac
case $ROOT in /mnt/*) die "ROOT must be on the machine's own disk, not $ROOT" ;; esac

# one tree hash: python3 does it in one process
tree_hash() {
  python3 -I -c '
import hashlib, os, sys
root = sys.argv[1]
lines = []
for d, dirs, files in os.walk(root):
    dirs[:] = sorted(x for x in dirs if x != ".git")
    for f in sorted(files):
        p = os.path.join(d, f)
        if os.path.islink(p):
            sys.exit("link in tree: " + p)
        h = hashlib.sha256(open(p, "rb").read().replace(b"\r", b"")).hexdigest()
        lines.append(h + "  ./" + os.path.relpath(p, root).replace(os.sep, "/"))
print(hashlib.sha256((chr(10).join(lines) + chr(10)).encode()).hexdigest())
' "$1"
}

# --- --check -----------------------------------------------------------------------------
do_check() {
  local missing=() pkgs=() n no_uv=0
  need() { # need COMMAND PACKAGE
    command -v "$1" >/dev/null 2>&1 || { missing+=("$1"); pkgs+=("$2"); }
  }
  need gcc build-essential; need g++ build-essential; need make build-essential
  need gfortran gfortran; need git git; need tar tar; need sha256sum coreutils
  need python3 python3; need valgrind valgrind; need gdb gdb
  need Xvfb xvfb; need xdotool xdotool; need taskset util-linux; need lscpu util-linux
  need ps procps; need nproc coreutils
  # headers and libraries: a compile says more than a file name
  if command -v gcc >/dev/null 2>&1; then
    printf '#include <X11/Xlib.h>\nint main(void){return 0;}\n' | gcc -x c - -lX11 -o /dev/null 2>/dev/null \
      || { missing+=("X11 headers (XPPAUT 8.0)"); pkgs+=(libx11-dev); }
    printf 'int main(void){return 0;}\n' | gcc -x c -fopenmp - -o /dev/null 2>/dev/null \
      || { missing+=("OpenMP (AUTO-07p, xppautX threads)"); pkgs+=(libgomp1); }
  fi
  if command -v g++ >/dev/null 2>&1; then
    printf '#include <format>\n#include <expected>\nint main(){return std::format("{}",1).size()==1?0:1;}\n' \
      | g++ -std=c++23 -x c++ - -o /dev/null 2>/dev/null \
      || { missing+=("a g++ with C++23 <format> and <expected> (xppautX; W269 used gcc 15.2)"); pkgs+=(g++-15); }
  fi
  if command -v gfortran >/dev/null 2>&1; then
    printf 'program t\nend program t\n' | gfortran -x f95 - -o /dev/null 2>/dev/null \
      || { missing+=("a working gfortran (AUTO-07p)"); pkgs+=(gfortran); }
  fi
  # libtcc is vendored (third_party/tinycc, built by xppautX's Makefile): gcc is all it needs
  [ -f "$REPO/third_party/tinycc/libtcc.c" ] || missing+=("third_party/tinycc (this checkout is incomplete)")
  command -v uv >/dev/null 2>&1 || no_uv=1
  n=$(( ${#missing[@]} + no_uv ))
  if [ "$n" -gt 0 ]; then
    echo "comparison-setup: $n missing:" >&2
    [ ${#missing[@]} -gt 0 ] && printf '  %s\n' "${missing[@]}" >&2
    [ "$no_uv" = 1 ] && echo "  uv (the studies' locked Python modules)" >&2
    echo "install (yours to run):" >&2
    [ ${#pkgs[@]} -gt 0 ] && echo "  sudo apt install $(printf '%s\n' "${pkgs[@]}" | sort -u | tr '\n' ' ')" >&2
    [ "$no_uv" = 1 ] && echo "  curl -LsSf https://astral.sh/uv/install.sh | sh" >&2
    exit 1
  fi
  echo "check: all tools present ($(gcc --version | head -1); $(gfortran --version | head -1); $(python3 --version); $(uv --version))"
}

# --- --sources ---------------------------------------------------------------------------
# have_tree NAME DIR TREEHASH COMMIT: 0 kept, 1 absent; a wrong tree is a refusal
have_tree() {
  local name=$1 dir=$2 want=$3 commit=$4 got head
  [ -e "$dir" ] || return 1
  [ -d "$dir" ] || die "$dir is not a folder"
  if [ -d "$dir/.git" ]; then
    head=$(git -C "$dir" rev-parse HEAD 2>/dev/null) || die "$dir has a broken .git"
    [ "$head" = "$commit" ] || die "$dir is at $head, not the pinned $commit: move it away and rerun"
  fi
  got=$(tree_hash "$dir") || die "cannot hash $dir"
  [ "$got" = "$want" ] || die "$dir has tree hash $got, not the pinned $want (edited or another version): move it away and rerun"
  echo "sources: $name kept ($commit, tree ok)"
}
clone_at() { # clone_at URL COMMIT DIR
  git clone -q "$1" "$3" || die "cannot clone $1 (network?)"
  git -C "$3" checkout -q "$2" || die "$1 has no commit $2"
}
do_sources() {
  local ref=$REPO/reference
  mkdir -p "$ref" || die "cannot make $ref"
  have_tree xppaut-8.0 "$ref/xppaut-8.0" "$XPPAUT8_TREE" "$XPPAUT8_COMMIT" || {
    mkdir "$ref/xppaut-8.0" && git archive "$XPPAUT8_COMMIT" | tar -x -C "$ref/xppaut-8.0" || die "git archive $XPPAUT8_COMMIT failed (is this a full clone?)"
    have_tree xppaut-8.0 "$ref/xppaut-8.0" "$XPPAUT8_TREE" "$XPPAUT8_COMMIT" >/dev/null || die "xppaut-8.0 rebuilt with another hash"
    echo "sources: xppaut-8.0 made from $XPPAUT8_COMMIT"; }
  have_tree xppaut-master "$ref/xppaut-master" "$MASTER_TREE" "$MASTER_COMMIT" || {
    clone_at "$MASTER_URL" "$MASTER_COMMIT" "$ref/xppaut-master"
    have_tree xppaut-master "$ref/xppaut-master" "$MASTER_TREE" "$MASTER_COMMIT" >/dev/null || die "xppaut-master cloned with another hash"
    echo "sources: xppaut-master cloned at $MASTER_COMMIT"; }
  have_tree auto-07p "$ref/auto-07p" "$AUTO_TREE" "$AUTO_COMMIT" || {
    clone_at "$AUTO_URL" "$AUTO_COMMIT" "$ref/auto-07p"
    have_tree auto-07p "$ref/auto-07p" "$AUTO_TREE" "$AUTO_COMMIT" >/dev/null || die "auto-07p cloned with another hash"
    echo "sources: auto-07p cloned at $AUTO_COMMIT"; }
}

# --- --python ----------------------------------------------------------------------------
# --locked: a uv.lock that does not match pyproject.toml is a refusal, never a silent re-lock
do_python() {
  command -v uv >/dev/null 2>&1 || die "uv is missing (see --check)"
  uv sync --locked || die "uv sync --locked failed: uv.lock and pyproject.toml disagree, or the network is down"
  echo "python: $(uv pip freeze | tr '\n' ' ')"
}

# --- --build -----------------------------------------------------------------------------
# kept NAME DIR STAMP: the build is kept when its stamp file holds STAMP
kept() { [ -f "$2/.comparison-built" ] && [ "$(cat "$2/.comparison-built")" = "$3" ]; }
stamp() { printf '%s' "$2" > "$1/.comparison-built"; }
logrun() { # logrun LOG COMMAND...
  local log=$1; shift
  "$@" > "$log" 2>&1 || { tail -n 15 "$log" >&2; die "failed: $* (log $log)"; }
}
do_build() {
  local b=$ROOT/build logs=$ROOT/build/logs commit dir s
  mkdir -p "$logs" || die "cannot make $logs"
  # xppautX: HEAD of this checkout, which must be clean (a tracked change would not be built)
  [ -z "$(git status --porcelain --untracked-files=no)" ] || die "this checkout has uncommitted changes; the comparison builds HEAD"
  commit=$(git rev-parse HEAD)
  dir=$b/xppautX-${commit:0:12}; s="$commit ${XPPAUTX_MAKE[*]}"
  if kept xppautX "$dir" "$s" && [ -x "$dir/xppautX" ]; then echo "build: xppautX $commit kept"; else
    [ -e "$dir" ] && die "$dir exists without a matching stamp: move it away"
    mkdir "$dir" && git archive "$commit" | tar -x -C "$dir" || die "git archive failed"
    echo "build: xppautX $commit ..."
    (cd "$dir" && logrun "$logs/xppautX.log" make -j"$JOBS" "${XPPAUTX_MAKE[@]}" "XPPAUTX_COMMIT=${commit:0:12}") || exit 1
    [ -x "$dir/xppautX" ] || die "xppautX was not made"; stamp "$dir" "$s"; fi
  # XPPAUT 8.0: the reference tree, with W238's command
  dir=$b/xppaut-8.0; s="$XPPAUT8_TREE $LEGACY_CC $LEGACY_CFLAGS $LEGACY_LDFLAGS"
  if kept xppaut "$dir" "$s" && [ -x "$dir/xppaut" ]; then echo "build: XPPAUT 8.0 kept"; else
    [ -e "$dir" ] && die "$dir exists without a matching stamp: move it away"
    cp -a "$REPO/reference/xppaut-8.0" "$dir" || die "cannot copy xppaut-8.0"
    echo "build: XPPAUT 8.0 ..."
    (cd "$dir" && logrun "$logs/xppaut-8.0.log" make -j"$JOBS" "CC=$LEGACY_CC" "CFLAGS=$LEGACY_CFLAGS" "LDFLAGS=$LEGACY_LDFLAGS") || exit 1
    [ -x "$dir/xppaut" ] || die "xppaut was not made"; stamp "$dir" "$s"; fi
  # AUTO-07p: the reference tree without .git, W269's configure options
  dir=$b/auto-07p; s="$AUTO_TREE $AUTO_CONFIGURE"
  if kept auto "$dir" "$s" && [ -x "$dir/bin/auto" -o -x "$dir/bin/auto-07p" ]; then echo "build: AUTO-07p kept"; else
    [ -e "$dir" ] && die "$dir exists without a matching stamp: move it away"
    mkdir "$dir" && tar -C "$REPO/reference/auto-07p" --exclude=.git -c . | tar -x -C "$dir" || die "cannot copy auto-07p"
    echo "build: AUTO-07p ..."
    # shellcheck disable=SC2086
    (cd "$dir" && logrun "$logs/auto-07p-configure.log" ./configure $AUTO_CONFIGURE \
      && logrun "$logs/auto-07p-make.log" make) || exit 1   # serial: its Makefile races on support.mod under -j
    [ -d "$dir/bin" ] || die "AUTO-07p was not made"; stamp "$dir" "$s"; fi
  echo "build: done. The drivers use: --xppautx $b/xppautX-${commit:0:12}/xppautX --xppaut $b/xppaut-8.0/xppaut AUTO_DIR=$b/auto-07p"
}

# --- --record ----------------------------------------------------------------------------
cpu_files() { # cpu_files FILE: the first cpu's cpufreq value, or "n/a"
  cat "/sys/devices/system/cpu/cpu0/cpufreq/$1" 2>/dev/null || echo n/a
}
turbo_state() {
  if [ -r /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
    [ "$(cat /sys/devices/system/cpu/intel_pstate/no_turbo)" = 1 ] && echo off || echo on
  elif [ -r /sys/devices/system/cpu/cpufreq/boost ]; then
    [ "$(cat /sys/devices/system/cpu/cpufreq/boost)" = 1 ] && echo on || echo off
  else echo "unknown (no intel_pstate/no_turbo or cpufreq/boost)"; fi
}
do_record() {
  local out=$ROOT/provenance stamp
  stamp=$(date -u +%Y%m%dT%H%M%SZ)
  mkdir -p "$out" || die "cannot make $out"
  GOV=$(cpu_files scaling_governor) DRIVER=$(cpu_files scaling_driver) TURBO=$(turbo_state) \
  XPPAUT8_TREE=$XPPAUT8_TREE MASTER_TREE=$MASTER_TREE AUTO_TREE=$AUTO_TREE \
  XPPAUT8_COMMIT=$XPPAUT8_COMMIT MASTER_COMMIT=$MASTER_COMMIT AUTO_COMMIT=$AUTO_COMMIT \
  UV_VERSION=$(uv --version 2>&1) PY_LOCKED=$(uv pip freeze 2>&1) PY_PROJECT=$(uv run python --version 2>&1) \
  XPPAUTX_HEAD=$(git rev-parse HEAD) AUTO_CONFIGURE=$AUTO_CONFIGURE ROOTDIR=$ROOT \
  python3 -I - "$out/machine-$stamp.json" <<'PY' || die "cannot write the machine record"
import json, os, platform, subprocess, sys, datetime
def run(*a):
    try:
        return subprocess.run(a, capture_output=True, text=True, check=False).stdout.strip()
    except OSError as e:
        return "unavailable: %s" % e
e = os.environ
smt = ""
try:
    smt = open("/sys/devices/system/cpu/smt/control").read().strip()
except OSError:
    smt = "n/a"
rec = {
    "written_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    "host": platform.node(),
    "kernel": platform.release() + " " + platform.version(),
    "os": run("sh", "-c", ". /etc/os-release && echo $PRETTY_NAME"),
    "cpu_model": next((l.split(":", 1)[1].strip() for l in open("/proc/cpuinfo") if l.startswith("model name")), "unknown"),
    "cpus_online": os.cpu_count(),
    "lscpu": run("lscpu"),
    "lscpu_extended": run("lscpu", "-e"),
    "smt": smt,
    "governor": e["GOV"], "scaling_driver": e["DRIVER"], "turbo": e["TURBO"],
    "memory": run("sh", "-c", "grep -E 'MemTotal|SwapTotal' /proc/meminfo"),
    "load_average": open("/proc/loadavg").read().strip(),
    "compilers": {"gcc": run("gcc", "--version").splitlines()[0], "g++": run("g++", "--version").splitlines()[0],
                  "gfortran": run("gfortran", "--version").splitlines()[0], "make": run("make", "--version").splitlines()[0],
                  "python3": run("python3", "--version"),
                  "valgrind": run("valgrind", "--version"), "gdb": run("gdb", "--version").splitlines()[0],
                  "xdotool": run("xdotool", "version"), "Xvfb": run("sh", "-c", "Xvfb -version 2>&1 | head -1")},
    "uv": e["UV_VERSION"], "python_project": e["PY_PROJECT"],
    "python_modules": e["PY_LOCKED"].splitlines(),
    "sources": {
        "xppautX_head": e["XPPAUTX_HEAD"],
        "xppautX_clean": run("git", "status", "--porcelain", "--untracked-files=no") == "",
        "xppaut-8.0": {"commit": e["XPPAUT8_COMMIT"], "tree_sha256": e["XPPAUT8_TREE"]},
        "xppaut-master": {"commit": e["MASTER_COMMIT"], "tree_sha256": e["MASTER_TREE"]},
        "auto-07p": {"commit": e["AUTO_COMMIT"], "tree_sha256": e["AUTO_TREE"], "configure": e["AUTO_CONFIGURE"]},
        "libtcc": "third_party/tinycc, vendored: its version is the xppautX commit's",
    },
    "builds": {},
}
b = os.path.join(e["ROOTDIR"], "build")
for name in sorted(os.listdir(b)) if os.path.isdir(b) else []:
    st = os.path.join(b, name, ".comparison-built")
    if os.path.isfile(st):
        rec["builds"][name] = open(st).read()
json.dump(rec, open(sys.argv[1], "w"), indent=1)
print("record: " + sys.argv[1])
PY
}

# --- --ready -----------------------------------------------------------------------------
do_ready() {
  local gov load
  gov=$(cpu_files scaling_governor)
  [ "$gov" = performance ] || die "the CPU governor is '$gov', not performance. Set it (yours, root): sudo cpupower frequency-set -g performance   (or: echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor)"
  # the 1-minute load average: what ran recently, which ps's lifetime %CPU cannot see
  load=$(cut -d' ' -f1 /proc/loadavg)
  awk -v l="$load" -v max="$IDLE_LOAD" 'BEGIN { exit !(l < max) }' || {
    ps -eo pid=,pcpu=,comm= --sort=-pcpu | head -5 >&2
    die "the 1-minute load is $load, not below $IDLE_LOAD (busiest above): the series needs the machine to itself"; }
  echo "ready: governor performance, turbo $(turbo_state), 1-minute load $load"
}

do_hashes() {
  echo "XPPAUT8_TREE=$(tree_hash "$REPO/reference/xppaut-8.0")"
  echo "MASTER_TREE=$(tree_hash "$REPO/reference/xppaut-master")"
  echo "AUTO_TREE=$(tree_hash "$REPO/reference/auto-07p")"
}

for m in "${modes[@]}"; do "do_$m"; done
