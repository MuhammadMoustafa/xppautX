#!/bin/sh
# Pack the built X11-free programs for a release (used by .github/workflows/release.yml).
# Usage: tools/package_release.sh PLATFORM     e.g. linux-x64, windows-x64, macos-arm64
cd "$(dirname "$0")/.." || exit 1
platform=$1
[ -n "$platform" ] || { echo "usage: $0 PLATFORM"; exit 2; }
version=${XPP_VERSION:-$(git describe --tags --always 2>/dev/null || echo dev)}
name="xppautX-$version-$platform"
rm -rf "build/$name" && mkdir -p "build/$name" || exit 1
case "$platform" in
  windows-*) suffix=.exe ;;
  *) suffix= ;;
esac
[ -f "xppautX$suffix" ] || { echo "package_release: xppautX$suffix has not been built" >&2; exit 1; }
cp "xppautX$suffix" "build/$name/"
# strip debug info from the archived copy only (the build itself, and any
# local build, keeps -g); macOS's strip needs -x to keep it a valid,
# re-signable Mach-O executable (it has no other symbol table to trim).
case "$platform" in
  macos-*) strip -x "build/$name/xppautX$suffix" ;;
  *) strip "build/$name/xppautX$suffix" ;;
esac
cp LICENSE "build/$name/"
[ -f CITATION.cff ] && cp CITATION.cff "build/$name/"
mkdir -p "build/$name/examples" && cp examples/ode/lecar.ode "build/$name/examples/"
# per-user .ode file association (tools/associate/, W13b): the Windows and
# macOS pieces are plain text, small enough for every platform's archive;
# Linux also needs the icons install-linux.sh installs into the hicolor
# theme (tools/make_icons.py's assets/icons/hicolor/).
mkdir -p "build/$name/tools/associate" && cp tools/associate/* "build/$name/tools/associate/"
case "$platform" in
  linux-*)
    [ -d assets/icons/hicolor ] || { echo "package_release: no assets/icons/hicolor (run tools/make_icons.py)" >&2; exit 1; }
    mkdir -p "build/$name/assets" && cp -r assets/icons "build/$name/assets/icons"
    ;;
esac
cat > "build/$name/README.txt" <<EOF
xppautX $version ($platform)

One program, and like xppaut it takes what to do from the command line:

  xppautX MODEL.ode            the front end in a window of its own (the
                               system's web view): on Windows with nothing
                               else needed, on macOS the same (WKWebView),
                               on Linux when WebKitGTK 4.1 is installed
                               (without it xppautX says how to install it
                               and uses your browser)
  xppautX --browser MODEL.ode  the same page in your browser, its address
                               printed
  xppautX MODEL.ode -silent    a headless run that writes output.dat, the
                               same switch upstream xppaut uses
  xppautX --server MODEL.ode   the JSON protocol on stdin/stdout, for a
                               program that embeds it

Try:  ./xppautX examples/lecar.ode
Options: --port N (default 8765, 0 for any free port), --no-open (browser mode,
printing the address only). xppautX --version prints this release's tag,
xppautX --help the modes.
Only this machine can reach it, and the address carries a one-time token.
Every xppaut option still works; xppautX's own options have to come first.

To open a .ode file by double-clicking it, run the matching script in
tools/associate/ once (per user, no admin rights): xppautx-associate.ps1
-Register on Windows, install-linux.sh on Linux; each has an
-Unregister/--uninstall counterpart.

The macOS and Windows binaries are not signed, so the system asks you to
allow them the first time: on macOS, run "xattr -dr com.apple.quarantine"
on this unpacked folder, or right-click xppautX and choose Open; on
Windows, SmartScreen's "More info" then "Run anyway". Neither warning
means anything is wrong with the file.

XPPAUT is by Bard Ermentrout; xppautX is a fork that runs without X11.
GPL v2: see LICENSE. The source of these binaries is the
xppautX-$version-source.tar.gz of the same release, also at
https://github.com/MuhammadMoustafa/xppautX
EOF
case "$platform" in
  windows-*) ( cd build && zip -qr "../$name.zip" "$name" ) && sha256sum "$name.zip" > "$name.zip.sha256" ;;
  *) tar -czf "$name.tar.gz" -C build "$name" && shasum -a 256 "$name.tar.gz" > "$name.tar.gz.sha256" 2>/dev/null ||
     sha256sum "$name.tar.gz" > "$name.tar.gz.sha256" ;;
esac
ls -l "$name".*
