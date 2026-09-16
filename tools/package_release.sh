#!/bin/sh
# Pack the built X11-free programs for a release (used by .github/workflows/release.yml).
# Usage: tools/package_release.sh PLATFORM     e.g. linux-x64, windows-x64, macos-arm64
cd "$(dirname "$0")/.." || exit 1
platform=$1
[ -n "$platform" ] || { echo "usage: $0 PLATFORM"; exit 2; }
version=${GITHUB_REF_NAME:-$(git describe --tags --always 2>/dev/null || echo dev)}
name="xppaut-web-$version-$platform"
rm -rf "build/$name" && mkdir -p "build/$name" || exit 1
case "$platform" in
  windows-*) suffix=.exe ;;
  *) suffix= ;;
esac
for f in xppaut-web xppcore-server xppcore-cli; do
  [ -f "$f$suffix" ] && cp "$f$suffix" "build/$name/"
done
cp LICENSE "build/$name/"
[ -f CITATION.cff ] && cp CITATION.cff "build/$name/"
mkdir -p "build/$name/examples" && cp examples/ode/lecar.ode "build/$name/examples/"
cat > "build/$name/README.txt" <<EOF
xppautX $version ($platform)

xppaut-web MODEL.ode      the front end in your browser (nothing else needed)
xppcore-server MODEL.ode  the same engine over the JSON protocol on stdin/stdout
xppcore-cli MODEL.ode     a headless run, writes output.dat

Try:  ./xppaut-web examples/lecar.ode
Options: --port N (default 8765), --no-open (print the address only).
Only this machine can reach it, and the address carries a one-time token.

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
