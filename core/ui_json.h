#ifndef XPP_UI_JSON_H
#define XPP_UI_JSON_H

/* line-delimited JSON front end (ui_json.c, docs/protocol.md) */
void json_ui_install(void);        /* protocol on the current stdout */
void json_ui_hello(char *title);   /* hello, palette, main window, state */
void json_ui_handle(const char *line);
void json_ui_loop(void);           /* read and run commands until EOF */

/* --script FILE: play FILE's lines instead of reading stdin (docs/protocol.md
   "Scripts"). Call before json_ui_install(), which then skips the stdin
   reader. Returns 0 when FILE cannot be opened. Once set, the process exits
   1 at end of file if a "message" "error" event was sent, or a script line
   could not be matched to the ask it was meant to answer; 0 otherwise. */
int json_ui_set_script(const char *path);

#endif
