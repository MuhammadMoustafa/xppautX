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

Installable files of the same release: xppautX-$version-windows-x64.exe
(the bare program), xppautX-$version-linux-x64.deb (sudo apt install
./xppautX-*.deb: /usr/bin, menu entry, icons, .ode file type) and
xppautX-$version-macos-*.dmg (drag xppautX.app to Applications).

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

# Installable files beside the archive (W89): the bare .exe, a .deb, a .dmg.
# Built from the stripped binary of the archive's folder.
bin="build/$name/xppautX$suffix"
case "$platform" in
  windows-*)
    cp "$bin" "$name.exe" && sha256sum "$name.exe" > "$name.exe.sha256"
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
    # libc6 from the release runner's glibc floor; libstdc++/libgcc only
    # when the binary links them dynamically
    deps="libc6 (>= 2.39)"
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
    sha256sum "$name.deb" > "$name.deb.sha256"
    ;;
  macos-*)
    # unsigned xppautX.app (Info.plist from the template the Makefile's
    # "app" target also uses, icon from assets/icon.icns) and an
    # /Applications link, in a compressed disk image
    app="build/$name-dmg/xppautX.app"
    rm -rf "build/$name-dmg" && mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources" || exit 1
    cp "$bin" "$app/Contents/MacOS/xppautX"
    cp assets/icon.icns "$app/Contents/Resources/icon.icns"
    sed "s/@XPPAUTX_VERSION@/$version/g" tools/associate/Info.plist.in > "$app/Contents/Info.plist"
    ln -s /Applications "build/$name-dmg/Applications"
    hdiutil create -volname "xppautX $version" -srcfolder "build/$name-dmg" -ov -format UDZO "$name.dmg" >/dev/null || exit 1
    shasum -a 256 "$name.dmg" > "$name.dmg.sha256"
    ;;
esac
ls -l "$name".*
