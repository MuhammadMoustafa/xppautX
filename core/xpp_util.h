#pragma once

#include <stdio.h>
#include "xpp_types.h"
#include "many_pops.h"
#include "session.h"
#include "xpp_error.h"

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>


namespace xpp {

/* now in UTC as 2026-09-30T10:14:02Z: when a recording begins, when a file of
   values was saved */
std::string utc_now();

/* s without its white space, in place */
void de_space(char *s);

/* an equilibrium's stability from its eigenvalues' counts: UNSTABLE,
   NEUTRAL or STABLE */
const char *eq_stability(int cp, int rp, int im);


void man_ic(Session &s);
/* a slider names a parameter (*type PARAMBOX) or a variable (ICBOX) of
   m, *index its index; 0 if neither */
int find_par_or_var(const Model &m, std::string_view name, int *type, int *index);
/* why slider cannot be one of m's (the slider command and .snapx's
   sliders.set both ask); nullopt when it can, an empty name an empty slot */
std::optional<std::string> slider_wrong(const Model &m, const XppSlider &slider, bool check_names = true);
/* the least and greatest of s's browser column index, widened when equal */
void get_max(const Session &s, int index, double *vmin, double *vmax);
/* the values behind the parameter and IC boxes and the sliders of s */
void new_parameter(Session &s);
/* every parameter (or only entry `only`) back to the model's value, the tables redone once */
void set_default_params(Session &s, std::optional<int> only = std::nullopt);
void set_default_ics(Session &s, std::optional<int> only = std::nullopt);
void set_par_or_var(Session &s, std::string_view name, int type, int index, double val);
void eq_import(Session &s, double *y, int n);
/* the plot of s redrawn into the picture file begun (image_format.h's
   restore), which it then closes */
void vector_restore(Session &s);
/* the ICs box "xvst" (how 0) and "pp" (how 1) buttons: plot s's checked
   variables (isck, n entries) and uncheck them */
void plot_checked_vars(Session &s, int how, int *isck, int n);
/* a comment's action (the source's "# ... {action}"), run on s when it is
   picked */
void do_txt_action(Session &s, std::string_view action);
/* the model m's user functions, as lunch-new.cpp's file info writes them */
void user_fun_info(const Model &m, FILE *fp);

/* name, shortened for a fixed-width display of width characters: a longer
   one keeps its start and ends in '~' so it cannot pass for another name */
std::string short_name(std::string_view name, int width);

/* s without its white space (de_space's, up to a NUL in it) */
void de_space(std::string &s);

/* "name:formula" (do_calc's "set this name to that"): 1 with name the
   text before the first ':' (of any length) and *where the formula's
   start; 0 when z has no ':' */
int has_eq(std::string_view z, std::string &name, int *where);

/* the name of plotted column ind: T (0), a model variable, or (W77) a
   browser column data_add_col added; browse_column_name (browse.h) owns
   the naming, this is just its name for a plotted column */
std::string ind_to_sym(const Session &s, int ind);


/* f() on the active plot window of s, or under Simulplot on each open one
   in turn (made active with make_active(s, i, flag)), the active one made
   active again after */
template <class F>
void for_each_shown_window(Session &s, int flag, F f)
{
    if (s.plot_windows.simul == 0) {
        f();
        return;
    }
    const int ic = s.plot_windows.active;
    for (int i = 0; i < s.plot_windows.count; i++) {
        make_active(s, s.plot_windows.open[i], flag);
        f();
    }
    make_active(s, ic, flag);
}

/* a fingerprint of values, to tell whether they are what they were
   without keeping a copy (a phase record against what the client got, a
   session against what was saved): 64-bit FNV-1a over 8-byte words of its
   bits (a flow's breaks are NaN), so a change goes unseen with odds of
   one in 2^64 */
class Fingerprint {
public:
    void bytes(const void *p, std::size_t n)
    {
        const unsigned char *b = static_cast<const unsigned char *>(p);
        for (; n >= sizeof(std::uint64_t); n -= sizeof(std::uint64_t), b += sizeof(std::uint64_t)) {
            std::uint64_t w;
            std::memcpy(&w, b, sizeof w);
            mix(w);
        }
        for (; n > 0; n--, b++) mix(*b);
    }
    template <class T> void value(const T &v) { bytes(&v, sizeof v); }
    void floats(const std::vector<float> &v)
    {
        value(v.size());
        bytes(v.data(), v.size() * sizeof(float));
    }
    std::uint64_t result() const { return h_; }
private:
    void mix(std::uint64_t w) { h_ = (h_ ^ w) * 1099511628211ULL; /* the FNV 64-bit prime */ }
    std::uint64_t h_ = 14695981039346656037ULL; /* the FNV 64-bit offset basis */
};

} // namespace xpp
