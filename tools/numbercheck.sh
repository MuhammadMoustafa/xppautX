#!/bin/sh
# One way to write a number on the page (W640): web2/src calls toPrecision, toExponential and toFixed only in
# store/values.ts, the owner of number text (shownNumber, inputText, exactText, steppedText, fixedText).
# Comments are stripped first. tools/sourcecheck.sh runs this. Usage: tools/numbercheck.sh
cd "$(dirname "$0")/.." || exit 1
bad=0
for f in $(find web2/src -name '*.ts' -o -name '*.tsx' | grep -v '^web2/src/store/values.ts$'); do
  hits=$(sed -e 's://.*$::' -e 's:/\*.*\*/::g' "$f" | grep -nE '\.(toPrecision|toExponential|toFixed)[ 	]*\(')
  if [ -n "$hits" ]; then
    echo "$f: number text outside store/values.ts:"
    echo "$hits"
    bad=1
  fi
done
if [ "$bad" -ne 0 ]; then
  echo "numbercheck: write numbers through store/values.ts (shownNumber, ui/CutText.tsx)"
  exit 1
fi
echo "numbercheck: ok"
