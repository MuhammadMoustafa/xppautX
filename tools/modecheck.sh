#!/bin/sh
# xppautX's modes from the command line (W13a): --help lists them, and
# says whether this build has the desktop window; --browser still prints
# the page's address and hands that same address to the browser opener;
# --no-open prints it and opens nothing. The opener is a stand-in
# (xdg-open, or open on macOS, first on PATH) that writes down what it was
# given, so no browser starts. The window itself needs a display and a
# person: docs/manual/01-introduction.md "Starting it" has the manual
# steps. On Linux with the window (W13e): xppautX links neither GTK nor
# WebKitGTK (its embedded library does), a failed load of that library
# (XPP_WINDOW_FAIL_LOAD=1, as if WebKitGTK were missing) says what to
# install and opens the browser, and with no display the real library
# loads, finds no display and opens the browser too.
# On Windows (Git Bash; CI's windows-core job) --browser opens the page with
# ShellExecute, which no stand-in on PATH can catch: that run is skipped
# there rather than open a real browser; --help and --no-open are checked.
# tools/verify.sh runs this. Usage: tools/modecheck.sh [./xppautX]
# (default ./xppautX, or ./xppautX.exe where only that exists)
cd "$(dirname "$0")/.." || exit 1
default=xppautX
[ ! -e xppautX ] && [ -e xppautX.exe ] && default=xppautX.exe
case "${1:-$default}" in
  /*) BIN=${1:-$default} ;;
  *) BIN=$(pwd)/${1:-$default} ;;
esac
windows=0
case "$(uname -s)" in MINGW* | MSYS* | CYGWIN*) windows=1 ;; esac
fail=0
pass() { echo "PASS $1"; }
bad() { echo "FAIL $1"; fail=1; }

help=$("$BIN" --help)
missing=
for flag in --browser --web --no-open --server --script --port --verbose --debug --version -silent; do
  case "$help" in *" $flag"*) ;; *) missing="$missing $flag" ;; esac
done
if [ -z "$missing" ]; then pass "--help lists the modes and options"; else bad "--help lists$missing"; fi
linux_window=0
if [ "$(uname -s)" = Linux ]; then
  if pkg-config --exists webkit2gtk-4.1 gtk+-3.0 2>/dev/null && [ "${WINDOW:-1}" != 0 ]; then
    want="a window of its own"
    linux_window=1
  else
    want="has no window of its own"
  fi
  case "$help" in *"$want"*) pass "--help says: $want" ;; *) bad "--help says: $want" ;; esac
  # the one binary starts on a Linux without GTK or WebKitGTK
  needed=$(readelf -d "$BIN" 2>/dev/null | grep NEEDED | grep -Ei 'gtk|webkit|gdk|glib|gobject|soup')
  if [ -z "$needed" ]; then pass "xppautX needs no GTK or WebKitGTK to start"; else bad "xppautX NEEDED: $needed"; fi
fi

tmp=$(mktemp -d) || exit 1
trap 'kill $pid 2>/dev/null; rm -rf "$tmp"' EXIT
mkdir "$tmp/bin"
for opener in xdg-open open; do
  printf '#!/bin/sh\necho "$1" >> "%s/opened"\n' "$tmp" > "$tmp/bin/$opener"
  chmod +x "$tmp/bin/$opener"
done
cp examples/ode/lecar.odex "$tmp/"

# start xppautX with $@ in $tmp (and env's $ENVS), wait for its XPP: line (at most 10 s)
start() {
  rm -f "$tmp/out" "$tmp/opened"
  ( cd "$tmp" && exec env -u WSL_DISTRO_NAME $ENVS PATH="$tmp/bin:$PATH" "$BIN" "$@" lecar.odex > out 2>&1 ) &
  pid=$!
  i=0
  while [ $i -lt 100 ] && ! grep -q '^XPP: http' "$tmp/out" 2>/dev/null; do
    sleep 0.1
    i=$((i + 1))
  done
  url=$(sed -n 's/^XPP: \(http:[^ ]*\).*/\1/p' "$tmp/out" | head -1)
}
stop() {
  kill $pid 2>/dev/null
  wait $pid 2>/dev/null
}

if [ $windows -eq 1 ]; then
  echo "SKIP --browser: Windows opens it with ShellExecute, not a stand-in on PATH"
else
  start --browser --port 0
  sleep 1 # the opener runs in the background
  case "$url" in
    http://127.0.0.1:*/?t=*) pass "--browser prints the address ($(echo "$url" | cut -c1-30)...)" ;;
    *) bad "--browser prints the address: $(head -c 300 "$tmp/out")" ;;
  esac
  if [ -n "$url" ] && [ "$(head -1 "$tmp/opened" 2>/dev/null)" = "$url" ]; then
    pass "--browser opens that same address"
  else
    bad "--browser opens that same address (opened: $(cat "$tmp/opened" 2>/dev/null))"
  fi
  stop
fi

start --no-open --port 0
sleep 1
case "$url" in
  http://127.0.0.1:*/?t=*) pass "--no-open prints the address" ;;
  *) bad "--no-open prints the address" ;;
esac
if [ -e "$tmp/opened" ]; then bad "--no-open opens nothing"; else pass "--no-open opens nothing"; fi
stop

if [ $linux_window -eq 1 ]; then
  # the window's library cannot load: the WARN names what to install, and
  # the browser opens instead (the stand-in opener)
  ENVS="XPP_WINDOW_FAIL_LOAD=1" start --port 0
  sleep 1
  if grep -q 'the window needs WebKitGTK, which is not installed (libwebkit2gtk-4.1.so.0 not found)' "$tmp/out" &&
    grep -q 'Using the browser instead' "$tmp/out"; then
    pass "no WebKitGTK: says what to install ($(sed -n 's/.*install it with: \(.*\)\. Using.*/\1/p' "$tmp/out"))"
  else
    bad "no WebKitGTK: says what to install: $(head -c 300 "$tmp/out")"
  fi
  if [ -n "$url" ] && [ "$(head -1 "$tmp/opened" 2>/dev/null)" = "$url" ]; then
    pass "no WebKitGTK: opens the browser"
  else
    bad "no WebKitGTK: opens the browser (opened: $(cat "$tmp/opened" 2>/dev/null))"
  fi
  stop
  # the library itself loads (memfd, dlopen, its table); no display (GTK
  # would find a Wayland socket by its default name: X11 only) the browser
  ENVS="-u DISPLAY -u WAYLAND_DISPLAY GDK_BACKEND=x11" start --port 0
  sleep 1
  if grep -q 'the window cannot open (webview error -1: GTK init failed' "$tmp/out" && ! grep -q 'WebKitGTK' "$tmp/out" &&
    [ -n "$url" ] && [ "$(head -1 "$tmp/opened" 2>/dev/null)" = "$url" ]; then
    pass "no display: the window's library loads, then the browser opens"
  else
    bad "no display: the window's library loads, then the browser opens: $(head -c 300 "$tmp/out")"
  fi
  stop
  ENVS=
fi

# a model that does not load exits non-zero (QA INPUT-001): --server and
# -silent at once (browser and window mode keep the page open on the log)
cp tools/models/malformed_unbalanced.ode "$tmp/"
for mode in --server -silent; do
  ( cd "$tmp" && exec "$BIN" $mode malformed_unbalanced.ode < /dev/null > bad.out 2>&1 ) &
  bpid=$!
  ( sleep 20; kill $bpid 2>/dev/null ) &
  watchdog=$!
  wait $bpid
  status=$?
  kill $watchdog 2>/dev/null
  wait $watchdog 2>/dev/null
  if [ $status -ne 0 ] && [ $status -lt 128 ] && grep -q 'ERROR' "$tmp/bad.out"; then
    pass "$mode: a model that does not load exits with status $status"
  else
    bad "$mode: a model that does not load exits with status $status: $(head -c 300 "$tmp/bad.out")"
  fi
done

# compiled functions (export, dll_lib/dll_fun, a network's import; W55)
# are gone: a model that uses one does not load, and says why
printf "x'=xp\nxp=0\nexport {x} {xp}\ninit x=1\ndone\n" > "$tmp/c_export.ode"
printf "x'=-x\n@ dll_lib=ex.so, dll_fun=vdp\ndone\n" > "$tmp/c_dll.ode"
printf "x[0..3]'=-x[j]\nspecial k=import(a.so,f,4,x0)\ndone\n" > "$tmp/c_import.ode"
for m in c_export c_dll c_import; do
  ( cd "$tmp" && exec "$BIN" -silent $m.ode < /dev/null > $m.out 2>&1 ) &
  bpid=$!
  ( sleep 20; kill $bpid 2>/dev/null ) &
  watchdog=$!
  wait $bpid
  status=$?
  kill $watchdog 2>/dev/null
  wait $watchdog 2>/dev/null
  # one error, at its line (W140b): "c_dll.ode:2: dll_lib: compiled functions ..."
  if [ $status -ne 0 ] && [ $status -lt 128 ] &&
    grep -q "^$m\.ode:[0-9][0-9]*: .*compiled functions are not supported" "$tmp/$m.out"; then
    pass "$m: a model using compiled functions does not load, and says why"
  else
    bad "$m: a model using compiled functions does not load, and says why (status $status): $(head -c 300 "$tmp/$m.out")"
  fi
done

[ $fail -eq 0 ] && echo "modecheck: all passed"
exit $fail
