#!/bin/bash
# Run a check script (tools/verify.sh, tools/asancheck.sh, ...) in WSL
# against this checkout's HEAD, from a copy on WSL's own filesystem
# (W84): read over /mnt/c, verify.sh took 22-25 minutes, from ext4 about
# 6.5. The copy is a git clone per checkout in
# ~/.cache/xppautx-verify/<checkout folder>, brought to HEAD by a fetch of
# the checkout's HEAD (a worktree's too, detached or not); its build/ is
# kept between runs, so builds stay incremental.
# Usage (Git Bash on Windows, or WSL itself), from the checkout's root:
#   tools/wslrun.sh tools/verify.sh [ARGS...]
# Only what is committed is checked: with uncommitted changes to tracked
# files it refuses (commit first; amend if the check fails).
set -u
if [ $# -lt 1 ]; then
  echo "usage: tools/wslrun.sh SCRIPT [ARGS...]" >&2
  exit 2
fi
src=$(git rev-parse --show-toplevel) || exit 2
if [ -z "${XPP_WSLRUN_HEAD:-}" ]; then
  if [ -n "$(git -C "$src" status --porcelain --untracked-files=no)" ]; then
    echo "wslrun: $src has uncommitted changes; only HEAD would be checked, so commit first" >&2
    exit 2
  fi
  XPP_WSLRUN_HEAD=$(git -C "$src" rev-parse HEAD) || exit 2
  case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
      # the rest runs in WSL, from the checkout as WSL sees it
      wsrc=$(wsl -e wslpath -a "$(cygpath -w "$src")" | tr -d '\r') || exit 2
      exec wsl -e bash -lc "cd $(printf '%q' "$wsrc") && XPP_WSLRUN_HEAD=$XPP_WSLRUN_HEAD bash tools/wslrun.sh $(printf '%q ' "$@")"
      ;;
  esac
fi
dst="$HOME/.cache/xppautx-verify/$(basename "$src")"
if [ ! -d "$dst/.git" ]; then
  mkdir -p "$dst" && git init -q "$dst" || exit 2
fi

# Clean up stale clones whose checkout no longer exists (W105): the live
# set is every worktree `git worktree list` names (the main checkout
# first), by folder name. Nothing is removed when that list cannot be
# read, and never the clone this run uses.
live=$(git -C "$src" worktree list --porcelain 2>/dev/null | sed -n 's/^worktree //p' | sed 's#.*[/\]##')
if [ -n "$live" ]; then
  removed=""
  for clone in "$HOME/.cache/xppautx-verify"/*/; do
    name=$(basename "$clone")
    [ "$HOME/.cache/xppautx-verify/$name" = "$dst" ] && continue
    printf '%s\n' "$live" | grep -qxF "$name" && continue
    [ -d "$clone.git" ] && rm -rf -- "${clone%/}" && removed="$removed $name"
  done
  [ -n "$removed" ] && echo "wslrun: removed the clones of checkouts that are gone:$removed"
fi

git -C "$dst" fetch -q "$src" HEAD || exit 2
if [ "$(git -C "$dst" rev-parse FETCH_HEAD)" != "$XPP_WSLRUN_HEAD" ]; then
  echo "wslrun: $src moved on while it was being fetched; run again" >&2
  exit 2
fi
# -f and clean: nothing of an earlier run's tree is left but what git
# ignores (build/, the binary), which the next build reuses
git -C "$dst" checkout -q -f --detach FETCH_HEAD && git -C "$dst" clean -fdq || exit 2
echo "wslrun: $1 on $(git -C "$dst" log --oneline -1) in $dst"
cd "$dst" && exec "$@"
