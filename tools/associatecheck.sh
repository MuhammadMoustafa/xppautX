#!/bin/sh
# The file associations register .odex beside .ode (W74) and the session
# file .snapx (W57), checked without
# touching the user's own settings: tools/associate/install-linux.sh
# installs into a temp prefix (never $HOME) and uninstalls again, and
# xppautx-associate.ps1 runs with -WhatIf (no registry write) where
# PowerShell is. tools/verify.sh runs this.
# Usage: tools/associatecheck.sh
set -u
cd "$(dirname "$0")/.." || exit 1
fail=0
check() { # check <what> <command...>
  what=$1
  shift
  if "$@" >/dev/null 2>&1; then echo "PASS $what"; else echo "FAIL $what"; fail=1; fi
}

prefix=$(mktemp -d)
exe=$(mktemp)
chmod +x "$exe"
HOME=/nonexistent sh tools/associate/install-linux.sh --prefix "$prefix" --exe "$exe" >/dev/null 2>&1
check "linux: *.odex has a MIME type" grep -q 'glob pattern="\*.odex"' "$prefix/share/mime/packages/xppautx-ode.xml"
check "linux: *.ode keeps its MIME type" grep -q 'glob pattern="\*.ode"' "$prefix/share/mime/packages/xppautx-ode.xml"
check "linux: *.snapx has a MIME type" grep -q 'glob pattern="\*.snapx"' "$prefix/share/mime/packages/xppautx-ode.xml"
check "linux: the desktop entry opens all three" grep -q '^MimeType=text/x-xpp-ode;text/x-xpp-odex;application/x-xppautx-session;' "$prefix/share/applications/xppautx.desktop"
HOME=/nonexistent sh tools/associate/install-linux.sh --prefix "$prefix" --uninstall >/dev/null 2>&1
check "linux: uninstall removes them" test ! -e "$prefix/share/mime/packages/xppautx-ode.xml"
rm -rf "$prefix" "$exe"

check "macOS: Info.plist declares odex" grep -q '<string>odex</string>' tools/associate/Info.plist.in
check "macOS: Info.plist declares snapx" grep -q '<string>snapx</string>' tools/associate/Info.plist.in

ps=
if command -v pwsh >/dev/null 2>&1; then ps=pwsh
elif command -v powershell.exe >/dev/null 2>&1; then ps=powershell.exe
fi
if [ -n "$ps" ]; then
  script=tools/associate/xppautx-associate.ps1
  command -v cygpath >/dev/null 2>&1 && script=$(cygpath -w "$script")
  out=$("$ps" -NoProfile -ExecutionPolicy Bypass -File "$script" -WhatIf -ExePath "$script" 2>&1)
  check "windows: -WhatIf would register .odex" sh -c 'printf "%s" "$1" | grep -q "Classes.\.odex.(Default) = xppautX.Model"' _ "$out"
  check "windows: -WhatIf would register .snapx" sh -c 'printf "%s" "$1" | grep -q "Classes.\.snapx.(Default) = xppautX.Model"' _ "$out"
fi
if [ $fail -ne 0 ]; then echo "associatecheck FAILED"; exit 1; fi
echo "associatecheck ok: .ode, .odex and .snapx are registered"
