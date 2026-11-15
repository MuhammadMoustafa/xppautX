#pragma once

#include <vector>

#include "model_declarations.h"

/* The global flags a load declares (model_flags.cpp), in the Session s the model is loaded into: what a load
   builds, below the integrators that fire them (flags.h, W736). */
namespace xpp {
struct OptionParts; /* model_options.h */
}

/* a flag: when cond crosses 0 in the direction sign, each event sets its
   name to its formula; 0, or 1 (said why) */
int add_global(const xpp::OptionParts &s, const char *cond, int sign, const std::vector<xpp::FlagEvent> &events);
/* every flag's condition and event formulas compiled: 0, or 1 (said why) when one does not parse */
int compile_flags(const xpp::OptionParts &s);
