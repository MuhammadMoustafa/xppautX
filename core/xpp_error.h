#ifndef XPP_ERROR_H
#define XPP_ERROR_H
/* xpp::Error: a computation's failure as a value (W63b). C++ only.
   A computation (an integration and its solvers, an equilibrium, a
   delay or DAE solve) returns one -- in an xpp::Result, or beside a
   result of its own -- instead of showing a message itself; the command
   that ran it shows it with xpp::show_error (xpp_ui.h), once the
   computation has ended and cleaned up. A model's problems are
   xpp::Diagnostic (diagnostic.h); AUTO's numerics throw xpp::AutoFailed
   (auto_state.h). */
#include <expected>
#include <optional>
#include <string>
#include <utility>

namespace xpp {

struct Error {
  /* the computation that failed ("integration", "delay", "DAE", ...),
     for the log */
  std::string where;
  /* what failed, as the user reads it */
  std::string what;
  /* the file it could not read ("" when none), which the page offers to
     add to the model's folder (docs/protocol.md `message`) */
  std::string file{};
};

/* a computation's value, or why it has none */
template <class T = void>
using Result = std::expected<T, Error>;

inline std::unexpected<Error> fail(std::string where, std::string what)
{
  return std::unexpected<Error>(Error{std::move(where), std::move(what)});
}

/* a failure to read `file` */
inline std::unexpected<Error> fail_reading(std::string where, std::string what, std::string file)
{
  return std::unexpected<Error>(Error{std::move(where), std::move(what), std::move(file)});
}

/* A computation that goes on past a failure (a sweep's steps, a saddle's
   manifolds) keeps the first one it met and returns it at its end. */
class FirstError {
public:
  void keep(Error e)
  {
    if (!first_) first_ = std::move(e);
  }
  template <class T> void keep(const Result<T> &r)
  {
    if (!r) keep(r.error());
  }
  Result<> result() const
  {
    if (first_) return std::unexpected<Error>(*first_);
    return {};
  }

private:
  std::optional<Error> first_;
};

} // namespace xpp

#endif
