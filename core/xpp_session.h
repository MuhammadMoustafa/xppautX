#ifndef _xpp_session_h_
#define _xpp_session_h_

/* One "session" = the .set file write_lunch/read_lunch already handle
   (parameters, ICs, numerics, graphics) plus, when there is one, the AUTO
   diagram save_auto/load_auto already handle (a .auto file, which itself
   carries the branches and the orbit data of the .s file). Issue #11's
   second half: one name, one action, for both.

   xpp_session_save/xpp_session_load write/read "<base>.set" and, when a
   diagram exists (save) or a ".auto" file is found (load), "<base>.auto"
   too. base may be NULL/empty, in which case the existing file_selector
   ask (core/xpp_ui.h) is used to get one, the way do_lunch and save_auto
   already prompt (a headless front end that never answers the ask just
   gets 0 back, the same as any other file_selector call there).

   Return 1 on success, 0 on failure (err_msg names the problem). After a
   successful save or load, xpp_session_set_file()/xpp_session_auto_file()
   name the files involved (xpp_session_auto_file() is "" when the session
   has no diagram); core/ui_json.c reports them as the "session" member of
   the "state" event (docs/protocol.md). */
int xpp_session_save(const char *base);
int xpp_session_load(const char *base);
const char *xpp_session_set_file(void);
const char *xpp_session_auto_file(void);

#endif
