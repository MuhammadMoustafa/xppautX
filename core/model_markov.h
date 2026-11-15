#pragma once

#include <functional>
#include <span>
#include <string>

/* The Markov variables and the Wiener parameters a load declares (model_markov.cpp), in the Session s the
   model is loaded into: what a load builds, below the integrators that run them (markov.h, W736). */
namespace xpp {

struct SessionParts; /* model_options.h */

void add_markov(const SessionParts &s, int nstate, const char *name);
void add_wiener(const SessionParts &s, int index);
int old_build_markov(const SessionParts &s, const std::function<bool(std::string &)> &next_line, const char *name);
void create_markov(const SessionParts &s, int nstates, double *st, int type, const char *name);
void add_markov_entry(const SessionParts &s, int index, int j, int k, const char *expr);
void compile_all_markov(const SessionParts &s);
int compile_markov(const SessionParts &s, int index, int j, int k);
/* an .ode transition row's next cell from *start on: the text between
   its { and }, *start past the } */
std::string markov_cell(const char *row, int *start);
/* the Markov variable name's transitions, cells its nstates x nstates
   formulas row by row (text, or numbers for a fixed chain): its index */
int build_markov(const SessionParts &s, std::span<const std::string> cells, const char *name);

} // namespace xpp
