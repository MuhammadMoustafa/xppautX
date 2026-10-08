#!/bin/sh
# Pack the built X11-free programs for a release (used by .github/workflows/release.yml).
# Usage: tools/package_release.sh PLATFORM     e.g. linux-x64, windows-x64, macos-arm64
# Makes the release files of one platform (W221): linux-x64 a .tar.gz and a .deb,
# windows-x64 an .msi (WiX, $WIX or `wix` on PATH) and a portable .zip, macos-* a .dmg. No checksum
# files: the release job writes one SHA256SUMS.txt over all of them.
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
mkdir -p "build/$name/examples" && cp examples/ode/lecar.odex "build/$name/examples/"
# the .tar.gz and the .zip carry the per-user .ode file association
# (tools/associate/, W13b), Linux also the icons install-linux.sh installs
# into the hicolor theme (tools/make_icons.py's assets/icons/hicolor/); the
# MSI and the .dmg register the file types themselves
case "$platform" in
  linux-*|windows-*)
    mkdir -p "build/$name/tools/associate" && cp tools/associate/* "build/$name/tools/associate/"
    ;;
esac
case "$platform" in
  linux-*)
    [ -d assets/icons/hicolor ] || { echo "package_release: no assets/icons/hicolor (run tools/make_icons.py)" >&2; exit 1; }
    mkdir -p "build/$name/assets" && cp -r assets/icons "build/$name/assets/icons"
    ;;
esac
case "$platform" in
  linux-*) notes='
To open a .ode file by double-clicking it, run tools/associate/install-linux.sh
once (per user, no admin rights); install-linux.sh --uninstall removes it.' ;;
  windows-*) notes='
To open a .ode file by double-clicking it, run
powershell -File tools\associate\xppautx-associate.ps1 -Register once (per
user, no admin rights; -Unregister undoes it). The .msi does this itself.
The program is not signed, so Windows SmartScreen may warn the first time:
choose "More info" then "Run anyway". The warning means nothing is wrong
with the file.' ;;
  *) notes='
The program is not signed, so macOS asks you to allow it the first time:
right-click xppautX.app and choose Open, or run
"xattr -dr com.apple.quarantine" on it. The warning means nothing is wrong
with the file.' ;;
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
  xppautX MODEL.ode --silent    a headless run that writes output.dat, the
                               same switch upstream xppaut uses
  xppautX --server MODEL.ode   the JSON protocol on stdin/stdout, for a
                               program that embeds it

Try:  ./xppautX examples/lecar.odex
Options: --port N (default 8765, 0 for any free port), --no-open (browser mode,
printing the address only). xppautX --version prints this release's tag,
xppautX --help the modes.
Only this machine can reach it, and the address carries a one-time token.
Every xppaut option still works; xppautX's own options have to come first.

The release's installers: xppautX-$version-windows-x64.msi (Windows; the
.zip is for a machine where you cannot install, no administrator rights
needed),
xppautX-$version-linux-x64.deb (sudo apt install ./xppautX-*.deb: /usr/bin,
menu entry, icons, .ode file type) and xppautX-$version-macos-*.dmg (drag
xppautX.app to Applications). SHA256SUMS.txt on the release page lists the
checksum of every file.
$notes
XPPAUT is by Bard Ermentrout; xppautX is a fork that runs without X11.
GPL v2: see LICENSE. The source of these binaries is the source archive of
this release's tag on its release page, also at
https://github.com/MuhammadMoustafa/xppautX
EOF
# the staged folder as an archive: Linux's .tar.gz, Windows's portable .zip
case "$platform" in
  linux-*) tar -czf "$name.tar.gz" -C build "$name" || exit 1 ;;
  windows-*) ( cd build && zip -qr "../$name.zip" "$name" ) || exit 1 ;;
esac

# Installers (W89, W221): an .msi, a .deb, a .dmg.
# Built from the stripped binary of the archive's folder.
bin="build/$name/xppautX$suffix"
case "$platform" in
  windows-*)
    # The MSI's ProductVersion has three numbers that sort (Windows
    # Installer ignores the fourth), so the tag vMAJOR.MINOR.PATCH[-pre.N] is
    # MAJOR.MINOR.BUILD with BUILD = PATCH*1000 + N for a pre-release (beta.1
    # is 1) and PATCH*1000 + 999 for the final release: v0.1.0-beta.1 is
    # 0.1.1, v0.1.0-beta.2 0.1.2, v0.1.0 0.1.999, v0.1.1-beta.1 0.1.1001.
    # Needs PATCH <= 64 and N < 999 (BUILD < 65536); the pre-release words
    # (beta, rc) share one series, so their N must keep rising. A build that
    # is not a tag (a dry run's git describe) is 0.0.1: it is never published.
    msiver=$(printf '%s' "${version#v}" | awk -F'[.-]' '
      /^[0-9]+\.[0-9]+\.[0-9]+$/ { print $1 "." $2 "." $3 * 1000 + 999; next }
      /^[0-9]+\.[0-9]+\.[0-9]+-[a-z]+\.[0-9]+$/ { print $1 "." $2 "." $3 * 1000 + $5; next }
      { print "0.0.1" }')
    case "$msiver" in
      *.*.[0-9]*) [ "${msiver##*.}" -lt 65536 ] || { echo "package_release: $version does not fit an MSI version" >&2; exit 1; } ;;
    esac
    # relative paths only: MSYS2 rewrites absolute ones in a native program's arguments
    MSYS2_ARG_CONV_EXCL='*' "${WIX:-wix}" build -arch x64 -o "$name.msi"       -d "ProductVersion=$msiver" -d "SourceDir=build/$name" -d "IconFile=assets/icon.ico"       packaging/windows/xppautX.wxs || exit 1
    ;;
  linux-x64)
    command -v dpkg-deb >/dev/null 2>&1 || { echo "package_release: dpkg-deb not found" >&2; exit 1; }
    # a Debian version starts with a digit: v1.2-3-gabc -> 1.2-3-gabc, a bare hash -> 0~hash
    debver=${version#v}
    case "$debver" in [0-9]*) ;; *) debver="0~$debver" ;; esac
    debver=$(printf '%s' "$debver" | tr -c 'A-Za-z0-9.+~\n-' '.')
    root="build/$name-deb"
    rm -rf "$root" && mkdir -p "$root/DEBIAN" "$root/usr/bin" "$root/usr/share/applications" \
      "$root/usr/share/mime/packages" "$root/usr/share/doc/xppautx" || exit 1
    cp "$bin" "$root/usr/bin/xppautX" && chmod 755 "$root/usr/bin/xppautX"
    # what install-linux.sh does per user, system-wide
    cp tools/associate/xppautx.desktop "$root/usr/share/applications/xppautx.desktop"
    cp tools/associate/xppautx-ode.xml "$root/usr/share/mime/packages/xppautx-ode.xml"
    for d in assets/icons/hicolor/*/apps; do
      size=$(basename "$(dirname "$d")")
      mkdir -p "$root/usr/share/icons/hicolor/$size/apps"
      cp "$d/xppautx.png" "$root/usr/share/icons/hicolor/$size/apps/xppautx.png"
    done
    cp LICENSE "$root/usr/share/doc/xppautx/copyright"
    cp README.md "$root/usr/share/doc/xppautx/README.md"
    chmod 644 "$root"/usr/share/doc/xppautx/*
    # libc6 from the newest glibc symbol version the binary uses;
    # libstdc++/libgcc only when the binary links them dynamically
    glibc=$(objdump -T "$bin" | grep -o 'GLIBC_[0-9.]*' | sed 's/GLIBC_//' | sort -V | tail -1)
    [ -n "$glibc" ] || { echo "package_release: no GLIBC version in $bin" >&2; exit 1; }
    deps="libc6 (>= $glibc)"
    if command -v readelf >/dev/null 2>&1; then
      needed=$(readelf -d "$bin" 2>/dev/null)
      case "$needed" in *libstdc++.so.6*) deps="$deps, libstdc++6 (>= 13)" ;; esac
      case "$needed" in *libgcc_s.so.1*) deps="$deps, libgcc-s1" ;; esac
    fi
    size_kb=$(du -sk "$root/usr" | cut -f1)
    cat > "$root/DEBIAN/control" <<EOF
Package: xppautx
Version: $debver
Architecture: amd64
Maintainer: Muhammad Moustafa <engmuhammadmoustafa@gmail.com>
Installed-Size: $size_kb
Depends: $deps
Recommends: libwebkit2gtk-4.1-0
Section: science
Priority: optional
Homepage: https://github.com/MuhammadMoustafa/xppautX
Description: xppautX, ODE simulation and bifurcation analysis (XPPAUT without X11)
 A modernised fork of XPPAUT. Opens its front end in a window of its own
 (WebKitGTK 4.1, recommended; without it, in your browser).
 Registers the .ode and .odex file types.
EOF
    dpkg-deb --root-owner-group --build "$root" "$name.deb" >/dev/null || exit 1
    ;;
  macos-*)
    # unsigned xppautX.app (Info.plist from the template the Makefile's
    # "app" target also uses, icon from assets/icon.icns) and an
    # /Applications link, in a compressed disk image
    rm -rf "build/$name-dmg" && tools/make_app.sh "$bin" "build/$name-dmg/xppautX.app" "$version" || exit 1
    ln -s /Applications "build/$name-dmg/Applications"
    hdiutil create -volname "xppautX $version" -srcfolder "build/$name-dmg" -ov -format UDZO "$name.dmg" >/dev/null || exit 1
    ;;
esac
ls -l "$name".*
