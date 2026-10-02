#!/bin/sh
# Build xppautX from source on a Linux the release does not run on.
#
# The released Linux binary is built on Ubuntu 26.04 and needs its glibc or
# newer (maintainer, 2026-10-01). On an older Ubuntu or Debian (22.04, 24.04,
# Debian 12, ...) run this from the source folder:
#
#     sh tools/build_linux.sh            # build ./xppautX
#     sh tools/build_linux.sh --install  # and copy it to ~/.local/bin
#     sh tools/build_linux.sh --no-apt   # install nothing: use what is there
#
# It installs what the build needs with apt (asking for your password through
# sudo, and printing each command first), picks a C++ compiler new enough,
# builds, and runs one model as a check. Nothing else needs installing:
# the page (web2/dist) is in the source already, so no Node.
#
# Another distribution: install gcc and g++ (MIN_GCC or newer), make,
# pkg-config and, for the desktop window, WebKitGTK 4.1's development package,
# then run `make -j4 xppautx` (CC=gcc-N CXX=g++-N when the default is older).

set -eu

# the oldest gcc the code builds with: the first with C++23's std::format, and
# the one the release was built with until 2026-10-01 (Ubuntu 24.04's gcc 13)
MIN_GCC=13
# the newest first: a newer compiler is preferred when several are installed
GCC_CANDIDATES="16 15 14 13"
# parallel jobs: enough to be quick, few enough not to overheat a laptop
JOBS=4

install=0
apt=1
for arg in "$@"; do
    case "$arg" in
    --install) install=1 ;;
    --no-apt) apt=0 ;;
    -h | --help) sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "build_linux: unknown option $arg (try --help)" >&2; exit 2 ;;
    esac
done

[ -f Makefile ] && [ -d core ] || {
    echo "build_linux: run this from the xppautX source folder" >&2; exit 2; }

top=$(pwd)
run() { echo "+ $*"; "$@"; }
# an apt command, skipped under --no-apt
pkg() { [ "$apt" -eq 0 ] || run $sudo "$@"; }

[ "$apt" -eq 0 ] || command -v apt-get >/dev/null 2>&1 || {
    echo "build_linux: no apt-get here; see the end of this script's header" >&2
    echo "for what to install on another distribution" >&2; exit 2; }

sudo=""
[ "$(id -u)" -eq 0 ] || sudo="sudo"

# the major version of compiler $1, or 0 when it is not there
gcc_major() {
    v=""
    command -v "$1" >/dev/null 2>&1 && v=$("$1" -dumpversion 2>/dev/null | cut -d. -f1)
    echo "${v:-0}"
}

# a g++ of MIN_GCC or newer: the default one, else a versioned one installed
find_gxx() {
    if [ "$(gcc_major g++)" -ge "$MIN_GCC" ] && [ "$(gcc_major gcc)" -ge "$MIN_GCC" ]; then
        echo "gcc g++"; return
    fi
    for v in $GCC_CANDIDATES; do
        if [ "$(gcc_major "g++-$v")" -ge "$MIN_GCC" ] && [ "$(gcc_major "gcc-$v")" -ge "$MIN_GCC" ]; then
            echo "gcc-$v g++-$v"; return
        fi
    done
}

echo "== what the build needs"
pkg apt-get update
pkg apt-get install -y make pkg-config gcc g++

compilers=$(find_gxx)
if [ -z "$compilers" ] && [ "$apt" -eq 1 ]; then
    # the system's own archive first, then (Ubuntu only) the toolchain PPA,
    # which carries newer gcc for older Ubuntu releases
    for v in $GCC_CANDIDATES; do
        if apt-cache show "g++-$v" >/dev/null 2>&1; then
            pkg apt-get install -y "gcc-$v" "g++-$v"; break
        fi
    done
    compilers=$(find_gxx)
fi
if [ -z "$compilers" ] && [ "$apt" -eq 1 ] && grep -qi '^ID=ubuntu' /etc/os-release 2>/dev/null; then
    echo "== gcc $MIN_GCC or newer from the ubuntu-toolchain-r PPA"
    pkg apt-get install -y software-properties-common
    pkg add-apt-repository -y ppa:ubuntu-toolchain-r/test
    pkg apt-get update
    pkg apt-get install -y "gcc-$MIN_GCC" "g++-$MIN_GCC"
    compilers=$(find_gxx)
fi
[ -n "$compilers" ] || {
    echo "build_linux: no gcc $MIN_GCC or newer could be installed; install one" >&2
    echo "and run: make -j$JOBS xppautx CC=gcc-N CXX=g++-N" >&2; exit 1; }
cc=${compilers% *}
cxx=${compilers#* }
echo "== compiler: $cxx ($("$cxx" -dumpfullversion 2>/dev/null || "$cxx" -dumpversion))"

# the desktop window needs WebKitGTK 4.1; without it xppautX opens in the browser
window=0
if [ "$apt" -eq 0 ]; then
    pkg-config --exists webkit2gtk-4.1 2>/dev/null && window=1
elif apt-cache show libwebkit2gtk-4.1-dev >/dev/null 2>&1; then
    pkg apt-get install -y libwebkit2gtk-4.1-dev
    window=1
else
    echo "== no WebKitGTK 4.1 here: building xppautX without its window (it uses the browser)"
fi

echo "== build"
run make -j"$JOBS" xppautx CC="$cc" CXX="$cxx" WINDOW="$window"

echo "== check: one model, no interface"
check_dir=$(mktemp -d)
trap 'rm -rf "$check_dir"' EXIT
model=examples/ode/lecar.odex
if [ -n "$model" ]; then
    cp "$model" "$check_dir/"
    (cd "$check_dir" && "$top/xppautX" "$(basename "$model")" --silent)
    rows=$(wc -l < "$check_dir/output.dat")
    [ "$rows" -gt 0 ] || { echo "build_linux: the check run wrote no rows" >&2; exit 1; }
    echo "ran $(basename "$model"): $rows rows"
fi
./xppautX --version

if [ "$install" -eq 1 ]; then
    run mkdir -p "$HOME/.local/bin"
    run cp xppautX "$HOME/.local/bin/xppautX"
    echo "installed: $HOME/.local/bin/xppautX (open a new terminal if it is not on your PATH yet)"
else
    echo "built: ./xppautX (sh tools/build_linux.sh --install copies it to ~/.local/bin)"
fi
