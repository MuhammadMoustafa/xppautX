#!/bin/bash
# Run a heavy command (a build, a check, a timing run) only when no other
# heavy command runs on this machine (W289): every agent, Claude or
# Codex, in any worktree, waits here for its turn, since the laptop
# overheats and reset twice under builds run side by side.
# Usage (Git Bash on Windows, or WSL), from anywhere:
#   tools/heavy.sh COMMAND [ARGS...]
# The lock is a folder (mkdir is atomic) in the user's local temp folder,
# which Codex's sandbox may write too, holding the holder's Windows PID
# and command; a holder that died without removing it is noticed by its
# PID and its lock taken over. tools/wslrun.sh takes the same lock, so
# a check run through it needs no heavy.sh of its own. XPP_HEAVY_HELD=1
# (set inside) lets a heavy command run another without waiting on
# itself.
set -u
if [ $# -lt 1 ]; then
  echo "usage: tools/heavy.sh COMMAND [ARGS...]" >&2
  exit 2
fi
if [ -n "${XPP_HEAVY_HELD:-}" ]; then
  exec "$@"
fi
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    root=$(cygpath -u "${LOCALAPPDATA:-$TEMP}")/Temp
    [ -d "$root" ] || root=$(cygpath -u "$TEMP")
    me=$(cat /proc/$$/winpid)
    # alive unless tasklist answers that it is not (a tasklist that fails,
    # in a sandbox say, never lets a live lock be taken over)
    alive() { local out; out=$(tasklist //FI "PID eq $1" //NH 2>/dev/null) || return 0; grep -q " $1 " <<<"$out"; }
    ;;
  *)
    # WSL: Windows' temp folder, so both sides share one lock
    win=$(cmd.exe /c 'echo %LOCALAPPDATA%' 2>/dev/null | tr -d '\r')
    root=$(wslpath -u "$win")/Temp
    me=$$
    alive() { kill -0 "$1" 2>/dev/null; }
    ;;
esac
lock="$root/xppautx-heavy.lock"
# First come, first served: each waiter files a ticket (the time it asked,
# its side and PID) and only the oldest live ticket may take the lock, so
# a run that asks again the moment it lets go cannot win every time over
# runs that poll (W275 waited hours behind W290's back-to-back steps).
queue="$root/xppautx-heavy.queue"
mkdir -p "$queue"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) mark=w ;; *) mark=l ;; esac
ticket="$(date +%s%N)-$mark-$me"
: > "$queue/$ticket"
trap 'rm -f "$queue/$ticket"' EXIT INT TERM
# the oldest ticket whose run is alive; a dead one of this side is removed
first_in_line() {
  local t pid
  for t in $(ls "$queue" | sort); do
    pid=${t##*-}
    if [ "${t#*-}" = "$mark-$pid" ] && [ "$t" != "$ticket" ] && ! alive "$pid"; then
      rm -f "$queue/$t"
      continue
    fi
    echo "$t"
    return
  done
}
# Poll interval while another heavy command runs (a wait for a condition).
POLL_SECONDS=5
waited=0
while ! { [ "$(first_in_line)" = "$ticket" ] && mkdir "$lock" 2>/dev/null; }; do
  holder=$(cat "$lock/pid" 2>/dev/null || true)
  side=$(cat "$lock/side" 2>/dev/null || true)
  # A holder on the other side (WSL vs Windows) cannot be checked by its
  # PID from here; only a lock of this side is ever taken over.
  if [ -n "$holder" ] && [ "$side" = "$(uname -s)" ] && ! alive "$holder"; then
    echo "heavy: taking over the lock of PID $holder, which has ended" >&2
    rm -rf "$lock"
    continue
  fi
  if [ $waited -eq 0 ]; then
    echo "heavy: waiting for $(cat "$lock/what" 2>/dev/null || echo 'another heavy run'), $(ls "$queue" | wc -l) in line" >&2
  fi
  waited=1
  sleep "$POLL_SECONDS"
done
rm -f "$queue/$ticket"
printf '%s' "$me" > "$lock/pid"
uname -s > "$lock/side"
printf '%s (%s)\n' "$*" "$PWD" > "$lock/what"
trap 'rm -rf "$lock"' EXIT INT TERM
XPP_HEAVY_HELD=1 "$@"
