/* A session (issue #11, the protocol's "session" command): one name for
   the files a long AUTO run is picked back up from, <base>.set (as File/
   Write set) and, when there is a diagram, <base>.auto (as AUTO's File/
   Save diagram, orbits included). */
#include "xpp_session.h"
#include "xpp_ui.h"
#include "lunch-new.h"
#include "diagram.h"    /* redraw_diagram; pulls in auto_nox.h */
#include "load_eqn.h"   /* XPP_MAX_NAME */
#include <stdio.h>
#include <string.h>

extern int NBifs;       /* diagram.c: >1 once a diagram has a point in it */
extern BIFUR Auto;
extern char this_file[XPP_MAX_NAME];

static char session_set[XPP_MAX_NAME];
static char session_auto[XPP_MAX_NAME];

const char *xpp_session_set_file(void) { return session_set; }

static void keep(char *dst, const char *name)
{
    size_t len = strlen(name);
    if (len >= XPP_MAX_NAME) len = XPP_MAX_NAME - 1;
    memcpy(dst, name, len);
    dst[len] = 0;
}
const char *xpp_session_auto_file(void) { return session_auto; }

/* base may be NULL: ask for one the way do_lunch's Write/Read set does,
   returning it (without its .set) in buf. Returns 0 on cancel. */
static int ask_base(const char *title, char *buf, size_t n)
{
    char *dot;
    snprintf(buf, n, "%s.set", this_file);
    ping();
    /* file_selector only displays title; it does not write through it. */
    if (!file_selector((char *)title, buf, "*.set")) return 0;
    dot = strrchr(buf, '.');
    if (dot && strcmp(dot, ".set") == 0) *dot = 0;
    return 1;
}

int xpp_session_save(const char *base)
{
    /* +10/+20 headroom so snprintf can provably never truncate the
       ".set"/".auto" suffix appended to a name up to XPP_MAX_NAME-1
       long. */
    char basebuf[XPP_MAX_NAME+10], set_name[XPP_MAX_NAME+20], auto_name[XPP_MAX_NAME+20];
    FILE *fp;

    if (base == NULL || base[0] == 0) {
        if (!ask_base("Save session", basebuf, sizeof basebuf)) return 0;
        base = basebuf;
    }

    snprintf(set_name, sizeof set_name, "%s.set", base);
    fp = fopen(set_name, "w");
    if (fp == NULL) {
        err_msg("Cannot open file");
        return 0;
    }
    redraw_params(); /* as do_lunch's Write set does, before write_lunch */
    write_lunch(fp);
    fclose(fp);
    keep(session_set, set_name);
    session_auto[0] = 0;

    if (NBifs > 1) { /* a diagram exists (save_diagram's own empty check) */
        snprintf(auto_name, sizeof auto_name, "%s.auto", base);
        fp = fopen(auto_name, "w");
        if (fp == NULL) {
            err_msg("Cannot open AUTO file");
            return 0;
        }
        if (save_auto_file(fp) != 1) {
            fclose(fp);
            err_msg("Empty diagram -- nothing to save");
            return 0;
        }
        fclose(fp);
        keep(session_auto, auto_name);
    }
    return 1;
}

int xpp_session_load(const char *base)
{
    char basebuf[XPP_MAX_NAME+10], set_name[XPP_MAX_NAME+20], auto_name[XPP_MAX_NAME+20];
    FILE *fp;

    if (base == NULL || base[0] == 0) {
        if (!ask_base("Load session", basebuf, sizeof basebuf)) return 0;
        base = basebuf;
    }

    snprintf(set_name, sizeof set_name, "%s.set", base);
    fp = fopen(set_name, "r");
    if (fp == NULL) {
        err_msg("Cannot open file");
        return 0;
    }
    if (!read_lunch(fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    keep(session_set, set_name);
    session_auto[0] = 0;

    snprintf(auto_name, sizeof auto_name, "%s.auto", base);
    fp = fopen(auto_name, "r");
    if (fp != NULL) {
        if (NBifs > 1) yes_reset_auto(); /* as load_auto does, without its confirmation ask */
        if (!Auto.exist) do_auto_win(); /* the diagram needs a window to draw into */
        if (load_auto_file(fp) != 1) {
            fclose(fp);
            err_msg("Bad AUTO file");
            return 0;
        }
        fclose(fp);
        if (Auto.exist) redraw_diagram(); /* load_auto leaves this to the caller */
        keep(session_auto, auto_name);
    }
    return 1;
}
