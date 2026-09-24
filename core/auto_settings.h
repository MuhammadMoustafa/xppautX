#ifndef AUTO_SETTINGS_H
#define AUTO_SETTINGS_H
#include <stddef.h>
#include "xpplim.h"
#ifdef __cplusplus
extern "C" {
#endif

/* AUTO's settings as data (docs/ui-v2.md T22, docs/protocol.md "AUTO's
   settings as data"): what the AUTO window's Numerics, Parameter, Axes
   (its AutoPlot form) and Mark values forms edit, sent as the
   "autosettings" event and set by the `auto` `set` command, so a front end
   edits them in forms of its own, at any time.

   The forms in auto_nox.c stay (scripts replay them); this is a second
   path that writes the same fields (Auto, aauto, SuppressBP, AutoPar and
   the user points' copies), after checking every value: a set with one
   bad value changes nothing.

   auto_settings.cpp; C++ with a C API, nothing escapes it. */

/* the Numerics form's fields, in its order (auto_num_par) */
enum {
    AUTO_NUM_NTST, AUTO_NUM_NMX, AUTO_NUM_NPR, AUTO_NUM_NCOL,
    AUTO_NUM_DS, AUTO_NUM_DSMIN, AUTO_NUM_DSMAX,
    AUTO_NUM_RL0, AUTO_NUM_RL1, AUTO_NUM_A0, AUTO_NUM_A1,
    AUTO_NUM_EPSL, AUTO_NUM_EPSU, AUTO_NUM_EPSS,
    AUTO_NUM_IAD, AUTO_NUM_MXBF, AUTO_NUM_IID, AUTO_NUM_ITMX, AUTO_NUM_ITNW,
    AUTO_NUM_NWTN, AUTO_NUM_IADS, AUTO_NUM_SUPPBP,
    AUTO_NUM_N
};

/* the key of Numerics field i in the event and the command ("ntst", ...),
   and its label on the core's form ("Ntst", "Par Min", ...); NULL out of range */
const char *auto_settings_num_key(int i);
const char *auto_settings_num_label(int i);

/* 1 when a value of field i is AUTO's (an integer where the field is one,
   in the field's range); why names the field and its range otherwise.
   Pure: no model state. */
int auto_settings_num_ok(int i, double v, char *why, size_t n);

#define AUTO_SETTINGS_PARS 8  /* AUTO's parameters (AutoPar) */
#define AUTO_SETTINGS_MARKS 9 /* Mark values (the form's Uzr1..Uzr9) */

/* One `auto` `set`: what it changes, each part only when given. A name
   left empty keeps what is there. */
typedef struct AutoSettingsSet {
    int has_num[AUTO_NUM_N];
    double num[AUTO_NUM_N];
    int npars; /* -1: no "pars"; else the first npars of AutoPar */
    char pars[AUTO_SETTINGS_PARS][XPP_NAME_MAX + 1];
    int has_plot;
    int plot;
    char var[XPP_NAME_MAX + 1], par1[XPP_NAME_MAX + 1], par2[XPP_NAME_MAX + 1];
    int has_range[4]; /* xmin, xmax, ymin, ymax */
    double range[4];
    int fit; /* Axes/Fit after the axes are set */
    int nmarks; /* -1: no "marks"; else how many (0 to 9) */
    char mark_name[AUTO_SETTINGS_MARKS][XPP_NAME_MAX + 1];
    double mark_value[AUTO_SETTINGS_MARKS];
} AutoSettingsSet;

/* an empty set: nothing given */
void auto_settings_set_init(AutoSettingsSet *s);

/* check the set against the model and apply it all, or nothing: 0 when
   applied, -1 with why (a sentence naming the bad value) when not. Axes
   (plot type, names, ranges, fit) draw the diagram again when AUTO's
   window is open, as the AutoPlot form does. */
int auto_settings_apply(const AutoSettingsSet *s, char *why, size_t n);

/* the event: {"ev":"autosettings",...} with the settings now, whole; an
   empty string when the model is too big for AUTO (no settings) */
typedef void (*AutoSettingsEmit)(const char *line, size_t len);
void auto_settings_init(AutoSettingsEmit emit);

/* {"cmd":"data"} with or without "autosettings": sent at the next update
   whatever it holds */
void auto_settings_subscribe(int on);

/* send the event if the settings changed since the one sent last */
void auto_settings_update(void);

#ifdef __cplusplus
}
#endif
#endif
