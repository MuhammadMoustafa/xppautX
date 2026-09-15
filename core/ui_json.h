#ifndef XPP_UI_JSON_H
#define XPP_UI_JSON_H

/* line-delimited JSON front end (ui_json.c, docs/protocol.md) */
void json_ui_install(void);        /* protocol on the current stdout */
void json_ui_hello(char *title);   /* hello, palette, main window, state */
void json_ui_handle(const char *line);
void json_ui_loop(void);           /* read and run commands until EOF */

#endif
