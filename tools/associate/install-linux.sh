#!/bin/sh
# Install (or remove) xppautX's .ode file association for the current user:
# no root needed, nothing outside $PREFIX is touched. Default $PREFIX is
# ~/.local, the XDG per-user data location (~/.local/share/applications,
# .../mime/packages, .../icons/hicolor); a package manager or a system-wide
# install would instead point --prefix at /usr/local or /usr and run as root
# (not this script's job -- it never asks for privileges).
#
# usage: install-linux.sh [--prefix DIR] [--uninstall] [--exe PATH]
#
# Installs tools/associate/xppautx.desktop (Exec rewritten to the resolved
# xppautX binary), tools/associate/xppautx-ode.xml (the text/x-xpp-ode MIME
# type for *.ode) and the hicolor PNG icons tools/make_icons.py wrote to
# assets/icons/hicolor/, then refreshes the desktop and MIME databases when
# their update tools are installed (optional: a fresh login picks them up
# either way).
set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
prefix="$HOME/.local"
action=install
exe=""

while [ $# -gt 0 ]; do
    case "$1" in
        --prefix) prefix=$2; shift 2 ;;
        --prefix=*) prefix=${1#--prefix=}; shift ;;
        --exe) exe=$2; shift 2 ;;
        --exe=*) exe=${1#--exe=}; shift ;;
        --uninstall) action=uninstall; shift ;;
        --install) action=install; shift ;;
        -h|--help)
            echo "usage: $0 [--prefix DIR] [--exe PATH] [--uninstall]"
            exit 0 ;;
        *) echo "install-linux: unknown argument: $1" >&2; exit 2 ;;
    esac
done

apps_dir="$prefix/share/applications"
mime_dir="$prefix/share/mime"
icons_dir="$prefix/share/icons/hicolor"
desktop_file="$apps_dir/xppautx.desktop"
mime_file="$mime_dir/packages/xppautx-ode.xml"

if [ "$action" = uninstall ]; then
    echo "install-linux: removing xppautX's .ode association from $prefix"
    rm -f "$desktop_file" "$mime_file"
    for size_dir in "$root"/assets/icons/hicolor/*/apps; do
        [ -d "$size_dir" ] || continue
        size=$(basename "$(dirname "$size_dir")")
        rm -f "$icons_dir/$size/apps/xppautx.png"
    done
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$apps_dir" 2>/dev/null || true
    command -v update-mime-database >/dev/null 2>&1 && update-mime-database "$mime_dir" 2>/dev/null || true
    echo "install-linux: done"
    exit 0
fi

if [ -z "$exe" ]; then
    if [ -x "$root/xppautX" ]; then
        exe="$root/xppautX"
    else
        echo "install-linux: no xppautX binary at $root/xppautX; pass --exe PATH" >&2
        exit 1
    fi
fi
exe=$(CDPATH= cd -- "$(dirname -- "$exe")" && pwd)/$(basename -- "$exe")

[ -d "$root/assets/icons/hicolor" ] || {
    echo "install-linux: no assets/icons/hicolor (run tools/make_icons.py first)" >&2
    exit 1
}

echo "install-linux: installing xppautX's .ode association into $prefix (exe: $exe)"
mkdir -p "$apps_dir" "$mime_dir/packages"
sed "s|^Exec=xppautX %f|Exec=$exe %f|; s|^TryExec=xppautX|TryExec=$exe|" \
    "$here/xppautx.desktop" > "$desktop_file"
cp "$here/xppautx-ode.xml" "$mime_file"

for size_dir in "$root"/assets/icons/hicolor/*/apps; do
    [ -d "$size_dir" ] || continue
    size=$(basename "$(dirname "$size_dir")")
    mkdir -p "$icons_dir/$size/apps"
    cp "$size_dir/xppautx.png" "$icons_dir/$size/apps/xppautx.png"
done

command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$apps_dir" 2>/dev/null || true
command -v update-mime-database >/dev/null 2>&1 && update-mime-database "$mime_dir" 2>/dev/null || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -f -t "$icons_dir" 2>/dev/null || true

echo "install-linux: done ($desktop_file, $mime_file, $icons_dir/*/apps/xppautx.png)"
