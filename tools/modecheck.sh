#!/bin/sh
# xppautX's modes from the command line (W13a): --help lists them, and
# says whether this build has the desktop window; --browser still prints
# the page's address and hands that same address to the browser opener;
# --no-open prints it and opens nothing. The opener is a stand-in
# (xdg-open, or open on macOS, first on PATH) that writes down what it was
# given, so no browser starts. The window itself needs a display and a
# person: docs/manual/01-introduction.md "Starting it" has the manual
# steps. tools/verify.sh runs this. Usage: tools/modecheck.sh [./xppautX]
cd "$(dirname "$0")/.." || exit 1
BIN=$(pwd)/${1:-xppautX}
fail=0
pass() { echo "PASS $1"; }
bad() { echo "FAIL $1"; fail=1; }

help=$("$BIN" --help)
missing=
for flag in --browser --web --no-open --server --script --port --verbose --debug --version -silent; do
  case "$help" in *" $flag"*) ;; *) missing="$missing $flag" ;; esac
done
if [ -z "$missing" ]; then pass "--help lists the modes and options"; else bad "--help lists$missing"; fi
if [ "$(uname -s)" = Linux ]; then
  if pkg-config --exists webkit2gtk-4.1 gtk+-3.0 2>/dev/null && [ "${WINDOW:-1}" != 0 ]; then
    want="a window of its own"
  else
    want="has no window of its own"
  fi
  case "$help" in *"$want"*) pass "--help says: $want" ;; *) bad "--help says: $want" ;; esac
fi

tmp=$(mktemp -d) || exit 1
trap 'kill $pid 2>/dev/null; rm -rf "$tmp"' EXIT
mkdir "$tmp/bin"
for opener in xdg-open open; do
  printf '#!/bin/sh\necho "$1" >> "%s/opened"\n' "$tmp" > "$tmp/bin/$opener"
  chmod +x "$tmp/bin/$opener"
done
cp examples/ode/lecar.ode "$tmp/"

# start xppautX with $@ in $tmp, wait for its XPP: line (at most 10 s)
start() {
  rm -f "$tmp/out" "$tmp/opened"
  ( cd "$tmp" && exec env -u WSL_DISTRO_NAME PATH="$tmp/bin:$PATH" "$BIN" "$@" lecar.ode > out 2>&1 ) &
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

start --no-open --port 0
sleep 1
case "$url" in
  http://127.0.0.1:*/?t=*) pass "--no-open prints the address" ;;
  *) bad "--no-open prints the address" ;;
esac
if [ -e "$tmp/opened" ]; then bad "--no-open opens nothing"; else pass "--no-open opens nothing"; fi
stop

[ $fail -eq 0 ] && echo "modecheck: all passed"
exit $fail
