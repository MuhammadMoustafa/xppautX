#!/bin/bash
# Run a heavy command (a build, a check, a timing run) only when a slot
# is free on this machine (W289): every agent, Claude or Codex, in any
# worktree, waits here for its turn, since the laptop overheats and reset
# under builds run side by side (three agents at once, 2026-10-10).
# HEAVY_SLOTS heavy commands may run at once (maintainer: two from
# 2026-10-11, three from 2026-10-14, each at -j4); --alone takes every slot, for a timing series no
# other run may skew.
# Usage (Git Bash on Windows, or WSL), from anywhere:
#   tools/heavy.sh [--alone] COMMAND [ARGS...]
# A slot is a folder (mkdir is atomic) in the user's local temp folder,
# which Codex's sandbox may write too, holding the holder's Windows PID
# and command; a holder that died without removing it is noticed by its
# PID and its slot taken over. tools/wslrun.sh takes a slot itself, so a
# check run through it needs no heavy.sh of its own. XPP_HEAVY_HELD=1
# (set inside) lets a heavy command run another without waiting on
# itself.
set -u
# One block, read whole before it runs: a copy of this script updated while
# it waits (a worktree given the new version) cannot change what runs.
{
# Heavy runs at once: three -j4 builds (the maintainer's call, 2026-10-14).
HEAVY_SLOTS=3
# A slot with no PID older than this lost its holder between mkdir and
# writing the PID (microseconds apart), so it is taken over.
EMPTY_SLOT_MINUTES=1
alone=0
if [ "${1:-}" = "--alone" ]; then alone=1; shift; fi
if [ $# -lt 1 ]; then
  echo "usage: tools/heavy.sh [--alone] COMMAND [ARGS...]" >&2
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
# slot 1 keeps the single lock's old name, so a run of the older script
# (one slot) still waits on it
slot_dir() { if [ "$1" -eq 1 ]; then echo "$root/xppautx-heavy.lock"; else echo "$root/xppautx-heavy.lock.$1"; fi; }
# First come, first served: each waiter files a ticket (the time it asked,
# its side and PID) and only the oldest live ticket may take a slot, so a
# run that asks again the moment it lets go cannot win every time over
# runs that poll (W275 waited hours behind W290's back-to-back steps),
# and a run --alone waiting for every slot is not overtaken.
queue="$root/xppautx-heavy.queue"
mkdir -p "$queue"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) mark=w ;; *) mark=l ;; esac
ticket="$(date +%s%N)-$mark-$me"
: > "$queue/$ticket"
taken=()
release() { local d; for d in "${taken[@]}"; do rm -rf "$d"; done; taken=(); }
trap 'rm -f "$queue/$ticket"; release' EXIT INT TERM
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
# a slot whose holder has ended (a reset, a killed agent) is freed; a
# holder on the other side (WSL vs Windows) cannot be checked by its PID
# from here, so only a slot of this side is ever taken over
free_dead() {
  local k d holder side
  for k in $(seq 1 $HEAVY_SLOTS); do
    d=$(slot_dir "$k")
    holder=$(cat "$d/pid" 2>/dev/null || true)
    side=$(cat "$d/side" 2>/dev/null || true)
    if [ -n "$holder" ] && [ "$side" = "$(uname -s)" ] && ! alive "$holder"; then
      echo "heavy: taking over the slot of PID $holder, which has ended" >&2
      rm -rf "$d"
    elif [ -d "$d" ] && [ -z "$holder" ] && [ -n "$(find "$d" -maxdepth 0 -mmin +$EMPTY_SLOT_MINUTES)" ]; then
      # its holder writes its PID the moment it takes the slot; one empty
      # this long was killed in between (2026-10-11: twice, the queue stuck)
      echo "heavy: taking over an empty slot ($d)" >&2
      rmdir "$d" 2>/dev/null
    fi
  done
}
# take one free slot, or (--alone) every slot or none
take() {
  local k d
  # --alone takes nothing while any slot is held: taking one and letting
  # it go again failed inside Codex's sandbox and left it empty (W290)
  if [ $alone -eq 1 ]; then
    for k in $(seq 1 $HEAVY_SLOTS); do [ -e "$(slot_dir "$k")" ] && return 1; done
  fi
  for k in $(seq 1 $HEAVY_SLOTS); do
    d=$(slot_dir "$k")
    if mkdir "$d" 2>/dev/null; then
      printf '%s' "$me" > "$d/pid"
      taken+=("$d")
      [ $alone -eq 1 ] || return 0
    elif [ $alone -eq 1 ]; then
      release
      return 1
    fi
  done
  [ ${#taken[@]} -gt 0 ]
}
# Poll interval while the slots are busy (a wait for a condition).
POLL_SECONDS=5
waited=0
while ! { [ "$(first_in_line)" = "$ticket" ] && take; }; do
  free_dead
  if [ $waited -eq 0 ]; then
    echo "heavy: waiting for a slot (busy: $(cat "$root"/xppautx-heavy.lock*/what 2>/dev/null | tr '\n' ';')), $(ls "$queue" | wc -l) in line" >&2
  fi
  waited=1
  sleep "$POLL_SECONDS"
done
rm -f "$queue/$ticket"
for d in "${taken[@]}"; do
  printf '%s' "$me" > "$d/pid"
  uname -s > "$d/side"
  printf '%s (%s)\n' "$*" "$PWD" > "$d/what"
done
XPP_HEAVY_HELD=1 "$@"
exit $?
}
