#pragma once

#include <array>
#include <string>
#include <string_view>

/* The words and settings of the analyses (shooting, spectra) that a model's options name and the analysis
   modules read: pure data, below both (W736), so the option table needs no analysis header. */
namespace xpp {

/* which end of the interval Range shoot stores the solution's values at (ShootRange::side; a session keeps the number) */
inline constexpr int SHOOT_SIDE_START = 0, SHOOT_SIDE_END = 1;
inline constexpr std::array<const char *, 2> SHOOT_SIDE_NAMES = {"start", "end"};
/* BVP's Range settings (pp_shoot.cpp) */
struct ShootRange {
  std::string item;
  int steps = 10, side = SHOOT_SIDE_START, cycle = 0, movie = 0;
  double plow = 0, phigh = 1;
};

/* the spectra's words, by spec_type's number: the power spectral density of one column, the cross spectrum of two, their coherence */
inline constexpr std::array<std::string_view, 3> spectrum_type_names{"spectral_density", "cross_spectral_density", "coherence"};

} // namespace xpp
