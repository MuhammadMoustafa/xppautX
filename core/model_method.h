#pragma once

#include <string_view>

#include "model_numerics.h"
#include "xpp_error.h"

namespace xpp {

struct Model; /* model.h */

/* The one place a method is chosen (W132): pick_method takes a name (a
   method's name or set label, XPPAUT's one-letter @ meth key, a spelling
   shipped models use), check_method a method's number (a menu, a .set);
   both refuse what the model cannot run (Symplectic an odd dimension,
   Volterra no kernels, kernels anything but Volterra) at place. */
Result<method::Id> pick_method(const Model &model, std::string_view text, Place place);
Result<method::Id> check_method(const Model &model, int id, Place place);
/* the method whose key (the word our files and the protocol use: runge_kutta,
   cvode, ...) is exactly key, refused as check_method refuses */
Result<method::Id> method_by_key(const Model &model, std::string_view key, Place place);

} // namespace xpp
