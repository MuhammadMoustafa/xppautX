#!/bin/sh
# Assemble xppautX.app (macOS): the one place the bundle is laid out, used by
# the Makefile's "app" target and tools/package_release.sh's .dmg (W89).
# Usage: tools/make_app.sh BINARY APP_DIR VERSION
cd "$(dirname "$0")/.." || exit 1
bin=$1 app=$2 version=$3
[ -f "$bin" ] && [ -n "$app" ] && [ -n "$version" ] || { echo "usage: $0 BINARY APP_DIR VERSION" >&2; exit 2; }
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources/examples" || exit 1
cp "$bin" "$app/Contents/MacOS/xppautX" &&
cp assets/icon.icns "$app/Contents/Resources/icon.icns" &&
# the bundled examples, where files::program_dir finds them (xpp_examples.h); the same ones package_release.sh ships
cp examples/ode/lecar.odex "$app/Contents/Resources/examples/" &&
sed "s/@XPPAUTX_VERSION@/$version/g" tools/associate/Info.plist.in > "$app/Contents/Info.plist"
