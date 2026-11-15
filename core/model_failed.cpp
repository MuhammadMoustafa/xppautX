#include "model_failed.h"
#include "xpp_log.h"

#include <cstdlib>
#include <utility>

namespace xpp {

namespace {
/* the load in progress: the one a load makes registers itself and goes at its end */
constinit LoadSite *current_site = nullptr;
}

LoadSite::LoadSite() noexcept { current_site = this; }
LoadSite::~LoadSite() { if (current_site == this) current_site = nullptr; }
void LoadSite::leave() noexcept { current_site = nullptr; }
LoadSite *LoadSite::current() noexcept { return current_site; }
bool LoadSite::running() noexcept { return current_site != nullptr; }

void LoadSite::add_source(Error &e)
{
  if (current_site) current_site->locate(e);
}

Error LoadSite::error(std::string_view also)
{
  return current_site->describe(also);
}

void model_failed(Error e)
{
  if (!LoadSite::running()) {
    if (!e.what.empty()) xpp::log(XPP_LOG_ERROR, "{}\n", e.text());
    exit(1);
  }
  LoadSite::add_source(e);
  throw LoadFailed{std::move(e)};
}

void model_failed()
{
  model_failed(LoadSite::running() ? LoadSite::error() : Error());
}

void model_failed(std::string_view what)
{
  /* only the parser and the readers a load runs call this: a model is
     built only by a load */
  model_failed(LoadSite::error(what));
}

} // namespace xpp
