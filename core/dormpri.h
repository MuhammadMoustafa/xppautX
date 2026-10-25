#pragma once
#include "solver.h"
namespace xpp {
/* Accepted and rejected attempts together, over a run (including event restarts).
   Exhaustion is an error, never a successful shortened trajectory. */
inline constexpr long DOPRI_RUN_ATTEMPTS = 100000;
std::unique_ptr<Solver> make_dormand_prince(const SolverInfo &info, Session &s, int n);
}
