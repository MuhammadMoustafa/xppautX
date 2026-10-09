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
POLL_SECONDS=0.1 # yield between output observations without an event API
OUTPUT_POLLS=100 # safety ceiling (10 s) for process startup, never a speed assertion
EXIT_TIMEOUT_SECONDS=20 # safety ceiling for a malformed model to exit
TIMEOUT_RUNNER=$(pwd)/tools/run_timeout.py
pass() { echo "PASS $1"; }
bad() { echo "FAIL $1"; fail=1; }

help=$("$BIN" --help)
missing=
for flag in --browser --web --no-open --server --port --verbose --debug --version --silent; do
  case "$help" in *" $flag"*) ;; *) missing="$missing $flag" ;; esac
done
if [ -z "$missing" ]; then pass "--help lists the modes and options"; else bad "--help lists$missing"; fi
removed=$("$BIN" --script unused.jsonl 2>&1)
status=$?
script_in_help=0
case "$help" in *" --script"*) script_in_help=1 ;; esac
case "$script_in_help:$status:$removed" in
  0:2:*"no such option --script"*) pass "--script is absent from help and an ordinary unknown option" ;;
  *) bad "--script refusal: $status $removed" ;;
esac
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
# the user's own settings stay untouched: every start here keeps its recent models in $tmp (W232)
export XPP_CONFIG_DIR="$tmp/config"
trap '[ -z "${pid:-}" ] || kill "$pid" 2>/dev/null; rm -rf "$tmp"' EXIT
mkdir "$tmp/bin"
for opener in xdg-open open; do
  printf '#!/bin/sh\necho "$1" >> "%s/opened"\necho "$#" >> "%s/argc"\n' "$tmp" "$tmp" > "$tmp/bin/$opener"
  chmod +x "$tmp/bin/$opener"
done
cp examples/ode/lecar.odex "$tmp/"

# Check batch success and old spelling in foreground runs.
if (cd "$tmp" && "$BIN" --silent lecar.odex > silent.out 2>&1) && [ -s "$tmp/output.dat" ]; then
  pass "--silent writes batch output"
else
  bad "--silent: $(head -c 300 "$tmp/silent.out")"
fi
old_silent=$(printf '\055silent')
if "$BIN" "$old_silent" > "$tmp/refused.out" 2>&1; then
  bad "old silent spelling was accepted"
elif grep -q -- "$old_silent is --silent" "$tmp/refused.out"; then
  pass "old silent spelling stops and names --silent"
else
  bad "old silent spelling: $(head -c 300 "$tmp/refused.out")"
fi

# Poll only inside this output wait; both the address and the opener are observable.
wait_for_output() {
  i=0
  while [ "$i" -lt "$OUTPUT_POLLS" ]; do
    grep -q "$1" "$2" 2>/dev/null && return 0
    kill -0 "$pid" 2>/dev/null || return 1
    sleep "$POLL_SECONDS"
    i=$((i + 1))
  done
  return 1
}

# start xppautX with $@ in $tmp (and env's $ENVS), wait for its XPP: line (at most 10 s)
start() {
  rm -f "$tmp/out" "$tmp/opened"
  ( cd "$tmp" && exec env -u WSL_DISTRO_NAME $ENVS PATH="${OPENER_PATH:-$tmp/bin:$PATH}" "$BIN" "$@" lecar.odex > out 2>&1 ) &
  pid=$!
  wait_for_output '^XPP: http' "$tmp/out" || bad "startup did not print an address"
  url=$(sed -n 's/^XPP: \(http:[^ ]*\).*/\1/p' "$tmp/out" | head -1)
}
stop() {
  kill "$pid" 2>/dev/null
  wait "$pid" 2>/dev/null
  pid=
}

if [ $windows -eq 1 ]; then
  echo "SKIP --browser: Windows opens it with ShellExecute, not a stand-in on PATH"
else
  # W176: a nonzero opener exit must be observed, rather than a successful background shell.
  for opener in xdg-open open; do
    printf '#!/bin/sh\nexit 1\n' > "$tmp/bin/$opener"
  done
  start --browser --port 0
  if wait_for_output 'open http.*in a browser' "$tmp/out"; then
    pass "failed opener is reported"
  else
    bad "failed opener was reported as success"
  fi
  stop
  mkdir "$tmp/missing"
  OPENER_PATH="$tmp/missing"
  start --browser --port 0
  if wait_for_output 'open http.*in a browser' "$tmp/out"; then
    pass "missing opener is reported"
  else
    bad "missing opener was reported as success"
  fi
  stop
  unset OPENER_PATH
  for opener in xdg-open open; do
    printf '#!/bin/sh\necho "$1" >> "%s/opened"\necho "$#" >> "%s/argc"\n' "$tmp" "$tmp" > "$tmp/bin/$opener"
  done
  rm -f "$tmp/argc"
  start --browser --port 0
  wait_for_output . "$tmp/opened" || bad "the opener did not write its address"
  case "$url" in
    http://127.0.0.1:*/?t=*) pass "--browser prints the address ($(echo "$url" | cut -c1-30)...)" ;;
    *) bad "--browser prints the address: $(head -c 300 "$tmp/out")" ;;
  esac
  if [ -n "$url" ] && [ "$(head -1 "$tmp/opened" 2>/dev/null)" = "$url" ]; then
    pass "--browser opens that same address"
  else
    bad "--browser opens that same address (opened: $(cat "$tmp/opened" 2>/dev/null))"
  fi
  # W231: no shell reads the address; the opener gets it as exactly one argument
  if [ "$(head -1 "$tmp/argc" 2>/dev/null)" = 1 ]; then
    pass "--browser hands the opener one argument"
  else
    bad "--browser hands the opener one argument (argc: $(cat "$tmp/argc" 2>/dev/null))"
  fi
  stop
fi

start --no-open --port 0
# Startup printed XPP after the mode had decided whether to invoke an opener.
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
  wait_for_output . "$tmp/opened" || bad "the fallback opener did not write its address"
  # W121b: one wording for the window's fallback, "the window cannot open (...); using the browser instead"
  if grep -q 'the window cannot open (needs WebKitGTK, which is not installed: libwebkit2gtk-4.1.so.0 not found' "$tmp/out" &&
    grep -q '; using the browser instead' "$tmp/out"; then
    pass "no WebKitGTK: says what to install ($(sed -n 's/.*install it with: \(.*\)); using.*/\1/p' "$tmp/out"))"
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
  wait_for_output . "$tmp/opened" || bad "the fallback opener did not write its address"
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
# --silent at once (browser and window mode keep the page open on the log)
cp tools/models/malformed_unbalanced.ode "$tmp/"
for mode in --server --silent; do
  ( cd "$tmp" && exec python3 "$TIMEOUT_RUNNER" "$EXIT_TIMEOUT_SECONDS" "$BIN" $mode malformed_unbalanced.ode < /dev/null > bad.out 2>&1 )
  status=$?
  if [ $status -ne 0 ] && [ $status -lt 124 ] && grep -q 'ERROR' "$tmp/bad.out"; then
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
  ( cd "$tmp" && exec python3 "$TIMEOUT_RUNNER" "$EXIT_TIMEOUT_SECONDS" "$BIN" --silent $m.ode < /dev/null > $m.out 2>&1 )
  status=$?
  # one error, at its line (W140b): "c_dll.ode:2: dll_lib: compiled functions ..."
  if [ $status -ne 0 ] && [ $status -lt 124 ] &&
    grep -q "^$m\.ode:[0-9][0-9]*: .*compiled functions are not supported" "$tmp/$m.out"; then
    pass "$m: a model using compiled functions does not load, and says why"
  else
    bad "$m: a model using compiled functions does not load, and says why (status $status): $(head -c 300 "$tmp/$m.out")"
  fi
done

[ $fail -eq 0 ] && echo "modecheck: all passed"
exit $fail
