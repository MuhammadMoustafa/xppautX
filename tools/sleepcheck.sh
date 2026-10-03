#!/bin/sh
# W166 (#218): synchronization observes a condition. Only the poll inside
# the shared wait helpers, and explicitly documented non-occurrence lower
# bounds, may delay a check. tools/sourcecheck.sh runs this.
cd "$(dirname "$0")/.." || exit 1

bad=0
for f in tools/*.mjs tools/*.py tools/*.sh; do
  [ -f "$f" ] || continue
  hits=$(awk -v file="$f" '
    /^export async function waitFor\(/ { helper = file == "tools/cdp.mjs" }
    /^def wait_until\(/ { helper = file == "tools/xppclient.py" }
    /^wait_for_output\(\)/ { helper = file == "tools/modecheck.sh" }
    /^}/ { helper = 0 }
    /^def / && !/^def wait_until\(/ { helper = 0 }
    /^class / { helper = 0 }
    /^[[:space:]]*(#|\/\/)/ { next }
    /lower bound:/ { next }
    file == "tools/cdp.mjs" && /^export const [s]leep = ms => new Promise\(r => setTimeout\(r, ms\)\);$/ { next }
    helper && /^[[:space:]]*await [s]leep\(POLL_MS\);$/ { next }
    helper && /^[[:space:]]*time\.[s]leep\(POLL_SECONDS\)$/ { next }
    helper && /^[[:space:]]*[s]leep "\$POLL_SECONDS"$/ { next }
    /(^|[^[:alnum:]_])[s]leep([^[:alnum:]_]|$)/ {
      printf "%s:%d: %s\n", file, NR, $0
    }
  ' "$f")
  [ -n "$hits" ] || continue
  printf '%s\n' "$hits"
  bad=1
done
if [ "$bad" -ne 0 ]; then
  echo "sleepcheck: wait for a condition; document an allowed lower bound beside its delay"
  exit 1
fi
echo "sleepcheck ok: checks wait for conditions"
