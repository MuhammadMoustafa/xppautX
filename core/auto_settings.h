#ifndef AUTO_SETTINGS_H
#define AUTO_SETTINGS_H
#include <stddef.h>
#include <array>
#include <string>
#include "xpplim.h"

/* AUTO's settings as data (docs/ui-v2.md T22, docs/protocol.md "AUTO's
   settings as data"): what the AUTO window's Numerics, Parameter, Axes
   (its AutoPlot form) and Mark values forms edit, sent as the
   "autosettings" event and set by the `auto` `set` command, so a front end
   edits them in forms of its own, at any time.

   The forms in auto_nox.cpp stay (scripts replay them); this is a second
   path that writes the same fields (Auto, aauto, SuppressBP, AutoPar and
   the user points' copies), after checking every value: a set with one
   bad value changes nothing.

   auto_settings.cpp; nothing escapes it. */

namespace xpp {

struct Session; /* session.h */

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

#define AUTO_SETTINGS_PARS 8  /* AUTO's parameters (AutoPar) */
#define AUTO_SETTINGS_MARKS 9 /* Mark values (the form's Uzr1..Uzr9) */

/* the event: {"ev":"autosettings",...} with the settings now, whole; an
   empty string when the model is too big for AUTO (no settings) */
typedef void (*AutoSettingsEmit)(const char *line, size_t len);
void auto_settings_init(AutoSettingsEmit emit);

/* {"cmd":"data"} with or without "autosettings": sent (s's) at once, and
   then at each update, whatever it holds */
void auto_settings_subscribe(const Session &s, int on);

/* send s's event if the settings changed since the one sent last */
void auto_settings_update(const Session &s);

/* One `auto` `set`: what it changes, each part only when given (a
   default-constructed set gives nothing). A name left empty keeps what is
   there. Filled by json_auto.cpp, read by auto_settings.cpp. */
struct AutoSettingsSet {
    std::array<int, AUTO_NUM_N> has_num{};
    std::array<double, AUTO_NUM_N> num{};
    int npars = -1; /* -1: no "pars"; else the first npars of AutoPar */
    std::array<std::string, AUTO_SETTINGS_PARS> pars;
    int has_plot = 0;
    int plot = 0;
    std::string var, par1, par2;
    std::array<int, 4> has_range{}; /* xmin, xmax, ymin, ymax */
    std::array<double, 4> range{};
    int fit = 0; /* Axes/Fit after the axes are set */
    int view = -1; /* the view whose axes these are (W50), made the active one; -1: the active one */
    int nmarks = -1; /* -1: no "marks"; else how many (0 to 9) */
    std::array<std::string, AUTO_SETTINGS_MARKS> mark_name;
    std::array<double, AUTO_SETTINGS_MARKS> mark_value{};
};

/* the settings now, whole: every Numerics field, AUTO's parameters, the
   axes and the Mark values, names empty where there is none (the event's
   null). What the event sends and an .autox's settings.txt saves (autox.h);
   applied with auto_settings_apply, it gives the settings back. */
AutoSettingsSet auto_settings_now(const Session &s);
/* view `view`'s axes alone (W50, AutoState::views; it must be one), as the
   axes keys and `view`: applied, they give the view its axes back */
AutoSettingsSet auto_settings_view(const Session &s, int view);

/* 1 when a value of field i is AUTO's (an integer where the field is one,
   in the field's range); why names the field and its range otherwise.
   Pure: no model state. Nothing is thrown. */
int auto_settings_num_ok(int i, double v, std::string &why);

/* check the set against the model and apply it all, or nothing: 0 when
   applied, -1 with why (a sentence naming the bad value) when not. Axes
   (plot type, names, ranges, fit) draw the diagram again when AUTO's
   window is open, as the AutoPlot form does. Nothing is thrown. */
int auto_settings_apply(Session &s, const AutoSettingsSet &set, std::string &why);

} // namespace xpp
#endif
