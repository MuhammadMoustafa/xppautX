#ifndef XPP_AUTO_UNKNOWNS_H
#define XPP_AUTO_UNKNOWNS_H

namespace xpp::auto_unknown {
// PAR(10) holds the objective in algebraic and periodic optimization.
constexpr int objective = 9;
// The wave formulation's PAR(10) is its propagation speed.
constexpr int wave_speed = 9;
// HomCont's PAR(10) unfolds the adjoint nondegeneracy condition.
constexpr int homoclinic_adjoint = 9;
// PAR(11) scales periodic orbits and homoclinic truncation intervals.
constexpr int period = 10;
// Fold continuation's PAR(12) is beta in the variational equation.
constexpr int fold_beta = 11;
// Fold continuation's PAR(13) is the squared null-vector norm.
constexpr int fold_norm = 12;
// Torus continuation's PAR(12) is the Floquet angle theta.
constexpr int torus_angle = 11;
// Torus continuation's PAR(13) is the squared eigenvector norm.
constexpr int torus_norm = 12;
// Period-doubling continuation's PAR(13) is the squared null-vector norm.
constexpr int doubling_norm = 12;
// Periodic optimization's PAR(12) normalizes its adjoint and multipliers.
constexpr int optimization_norm = 11;
// Periodic optimization's PAR(13) is the phase multiplier kappa.
constexpr int optimization_kappa = 12;
// Periodic optimization's PAR(14) is the objective multiplier gamma.
constexpr int optimization_gamma = 13;
// Time integration and parabolic evolution continue time in PAR(14).
constexpr int evolution_time = 13;
// Parabolic formulations keep diffusion coefficients from PAR(15).
constexpr int diffusion_start = 14;
// General BVP folds keep their additional free parameters from PAR(11).
constexpr int bvp_fold_start = 10;
// Rotating periodic orbits use PAR(19) as their rotation period.
constexpr int rotation_period = 18;
// Periodic optimization and HomCont store test functions from slot 20.
constexpr int test_function_start = 20;
// HomCont's equilibrium coordinates start at PAR(12), zero-based slot 11.
constexpr int homoclinic_equilibrium_start = 11;
// Homotopy's one-based pointer starts at 12 before its equilibrium blocks.
constexpr int homoclinic_start = 12;
// Homotopy reserves two equilibria and two unstable-direction blocks.
constexpr int homoclinic_blocks = 4;
// Numerical test functions occupy slots through 35 (HomCont PSI plus 19).
constexpr int block_size = 36;
}

#endif
