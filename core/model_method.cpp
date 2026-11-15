/* Choosing a method (model_method.h, W132): by its name, key or number, refused when the model cannot run it. The methods' words and traits
   are model_numerics.h's table; the solvers (solver.h) are the integrators'. */
#include "model_method.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "model.h"
#include "model_numerics.h"
#include "xpp_text.h"

namespace xpp {

Result<method::Id> check_method(const Model &model, int id, Place place)
{
  if (id < 0 || id >= method::COUNT)
    return fail("method", xpp::format("{} is not a method's number (0 to {})", id, method::COUNT - 1), std::move(place));
  const SolverTraits &traits = methods[static_cast<std::size_t>(id)].traits;
  if (traits.integral_history && model.compiler.nkernel == 0)
    return fail("method", "Volterra only for integral eqns", std::move(place));
  if (traits.paired_dimension && model.node % 2 != 0)
    return fail("method", "Symplectic is only for even dimensions", std::move(place));
  if (model.compiler.nkernel > 0 && !traits.integral_history)
    return fail("method", "a model with integral equations is integrated by Volterra", std::move(place));
  const char *noise = model.nwiener > 0 ? "Wiener" : !model.ou.empty() ? "Ornstein-Uhlenbeck" : nullptr;
  if (noise && !traits.wiener)
    return fail("method", xpp::format("a model with {} noise requires a method that draws it each step", noise), std::move(place));
  return static_cast<method::Id>(id);
}

Result<method::Id> method_by_key(const Model &model, std::string_view key, Place place)
{
  for (const MethodInfo &info : methods)
    if (info.key == key) return check_method(model, info.id, std::move(place));
  std::string keys;
  for (const MethodInfo &info : methods) keys += (keys.empty() ? "" : ", ") + std::string(info.key);
  return fail("method", xpp::format("`{}` is not a method ({})", key, keys), std::move(place));
}

Result<method::Id> pick_method(const Model &model, std::string_view text, Place place)
{
  /* XPPAUT's @ meth keys in methods order (load_eqn.c:1116). */
  constexpr std::string_view keys = "demragvbqsc582y";
  static_assert(keys.size() == methods.size());
  const MethodInfo *chosen = nullptr;
  const std::string name = lower_case(std::string(trim_blanks(text)));
  for (const MethodInfo &info : methods)
    if (equal_ignoring_case(name, info.name) || equal_ignoring_case(name, info.set_label) || name == info.key) chosen = &info;
  if (!chosen && name.size() == 1) {
    const auto i = keys.find(name[0]);
    if (i != std::string_view::npos) chosen = &methods[i];
  }
  /* Spellings used by shipped models and the long descriptive names. */
  if (name == "rk4") chosen = &methods[method::RK4];
  if (name == "disc") chosen = &methods[method::DISCRETE];
  if (name == "qualrk4") chosen = &methods[method::RKQS];
  if (name == "modified euler") chosen = &methods[method::MOD_EULER];
  if (name == "modeuler") chosen = &methods[method::MOD_EULER];
  if (!chosen) return fail("method", xpp::format("Unknown method `{}`", text), std::move(place));
  return check_method(model, chosen->id, std::move(place));
}

} // namespace xpp
