#!/bin/sh
# No current Session or Model is read (W47d6, CLAUDE.md "No global
# state"): a function takes the Session (or the Model, or the part it
# uses) from its caller. The one global that holds Sessions is the session
# list (core/session.h client_session): one Session per client. It is read
# only where a Session is chosen and passed down from there, in the owner
# files below, each with the number of reads it may make; the JSON front
# end's own asks and checkpoints, which core code reaches through the
# XppUi seam with no Session, take their client's through ui_json.cpp's
# client(), counted the same way. A read anywhere else, or one more in an
# owner, fails: pass the Session down instead. xpp::session() and
# xpp::model(), the current-one accessors W47d retired, fail everywhere.
# Comments are stripped first (tools/strip_comments.awk); line-based, like
# tools/literalcheck.sh. tools/sourcecheck.sh runs this.
# Usage: tools/sessioncheck.sh
cd "$(dirname "$0")/.." || exit 1

# file pattern reads reason
ALLOW='
core/session.h client_session 1 the session list: its declaration
core/session.cpp client_session 1 the session list: its definition
core/ui_json.cpp client_session 2 handle_line chooses a command'"'"'s Session; client() is the front end'"'"'s
core/xppautx_main.cpp client_session 2 the program'"'"'s start (AUTO'"'"'s scratch folder) and its exit
core/ui_json_internal.h client 1 the front end'"'"'s client(): its declaration
core/ui_json.cpp client 2 the front end'"'"'s client(): its definition, and json_ui_start_screen (the hello of a session with no model, W232)
core/json_prompts.cpp client 3 an ask (ask_wait), a checkpoint (j_check_abort) and the file selector, which starts in the dialog folder of the Session (W151)
core/json_io.cpp client 1 the pending AUTO points before any event (flush_pending)
core/json_state.cpp client 1 the state when it is dirty (send_state_if_dirty)
core/json_auto.cpp client 1 auto_data.cpp'"'"'s point lookup (diag_point_of_node)
'

# each file's reads, comments stripped: "R file line text" for a retired
# accessor, "C file pattern line" for a read of the list or client()
for f in core/*.cpp core/*.h; do
  [ -f "$f" ] || continue
  awk -f tools/strip_comments.awk "$f" | awk -v f="$f" '
    /xpp::(session|model)\(\)/ { print "R", f, FNR, $0 }
    {
      line = $0
      # a call, not a member (.client(), ->client()) nor a longer name
      while (match(line, /(^|[^._>A-Za-z0-9])(client_session|client)\(/)) {
        call = substr(line, RSTART, RLENGTH)
        sub(/^[^A-Za-z_]/, "", call)
        sub(/\($/, "", call)
        print "C", f, call, FNR
        line = substr(line, RSTART + RLENGTH)
      }
    }'
done | ALLOW="$ALLOW" awk '
  BEGIN {
    n = split(ENVIRON["ALLOW"], rows, "\n")
    for (i = 1; i <= n; i++) {
      split(rows[i], w, " ")
      if (w[1] != "") max[w[1] " " w[2]] = w[3]
    }
  }
  $1 == "R" {
    print $2 ":" $3 ": xpp::session() and xpp::model() are gone (W47d6): take the Session from the caller"
    bad = 1
    next
  }
  $1 == "C" { key = $2 " " $3; count[key]++; lines[key] = lines[key] " " $4 }
  END {
    for (key in count)
      if (count[key] > max[key] + 0) {
        split(key, k, " ")
        print k[1] ": " count[key] " reads of " k[2] "() (lines" lines[key] ") where " max[key] + 0 " are allowed: pass the Session down from where it is chosen"
        bad = 1
      }
    exit bad
  }'
if [ $? -ne 0 ]; then
  echo "sessioncheck: a current Session or Model read outside its owner (CLAUDE.md \"No global state\")"
  exit 1
fi
echo "sessioncheck ok: the session list is read only by its owners"
