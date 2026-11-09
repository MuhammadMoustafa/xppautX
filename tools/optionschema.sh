#!/bin/sh
# The JSON Schema of a .odex's options block (docs/odex.md "Options", W281):
# docs/odex-options.schema.json is generated from the option table
# (core/model_options.cpp, the one source of every key, group and kind of
# value) by `xppautX --options-schema`, so the VS Code extension and linters
# read what the program accepts.
#   tools/optionschema.sh --check [--bin PATH]   fail when a committed file differs from the program
#   tools/optionschema.sh --update [--bin PATH]  rewrite the committed files (and docs/manual/15-settings-panels.md, tools/optionsmanual.py)
# --bin PATH is the program (default ./xppautX, or ./xppautX.exe); sourcecheck
# runs --check with build/lto/xppautX, which its ltocheck has just linked.
cd "$(dirname "$0")/.." || exit 1
mode=--check
bin=
while [ $# -gt 0 ]; do
  case "$1" in
    --check|--update) mode=$1 ;;
    --bin) bin=$2; shift ;;
    *) echo "usage: tools/optionschema.sh [--check|--update] [--bin PATH]" >&2; exit 2 ;;
  esac
  shift
done
if [ -z "$bin" ]; then
  if [ -x ./xppautX ]; then bin=./xppautX; else bin=./xppautX.exe; fi
fi
tmp=$(mktemp) || exit 1
trap 'rm -f "$tmp"' EXIT
status=0
schema=docs/odex-options.schema.json
"$bin" --options-schema > "$tmp" || { echo "optionschema: $bin --options-schema failed"; exit 1; }
if [ "$mode" = --update ]; then
  cp "$tmp" "$schema" && echo "optionschema: wrote $schema"
elif cmp -s "$tmp" "$schema"; then
  echo "optionschema ok: $schema matches the program"
else
  echo "optionschema: $schema differs from the program (tools/optionschema.sh --update)"
  diff "$schema" "$tmp" | head -20
  status=1
fi
# the manual's settings pages, from the same table (W616)
uv run --locked python tools/optionsmanual.py "$mode" "$tmp" || status=1
exit $status
