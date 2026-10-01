#ifndef XPP_ERROR_H
#define XPP_ERROR_H
/* xpp::Error: an error as a value, and where it is (W63b, W63c; one type
   since W140). C++ only. Every way of reporting one takes it: show_error
   and err_msg (xpp_ui.h), a load that fails (xpp::load_model and
   xpp::model_failed, xpp_batch.h; xpp::LoadFailed, session.h), the .odex
   reader's problems (odex::error_at, odex.h). Error::text() is the one
   rendering of it as text (the console, the log, -silent), and the JSON
   front end sends its fields in the `error` and `message` events
   (docs/protocol.md "Errors").
   A computation (an integration and its solvers, an equilibrium, a
   delay or DAE solve) returns one -- in an xpp::Result, or beside a
   result of its own -- instead of showing a message itself; the command
   that ran it shows it with xpp::show_error (xpp_ui.h), once the
   computation has ended and cleaned up. AUTO's numerics throw
   xpp::AutoFailed (auto_state.h). */
#include <expected>
#include <optional>
#include <string>
#include <utility>

namespace xpp {

/* where a problem is */
struct Place {
  /* the file it is in (a model, a file it includes, a file a command
     read); "" when none */
  std::string file;
  /* where in it, both from 1; 0 when not known (the file as a whole, or
     a column the reader cannot tell) */
  int line = 0;
  int col = 0;
  /* line `line` of `file` as written, when it could be read */
  std::string source;
};

struct Error {
  /* the computation or reader that failed ("integration", "delay",
     "set file", ...), for the log */
  std::string where;
  /* what failed, as the user reads it: one line, or several (a formula
     and a caret under the place it stops making sense) */
  std::string what;
  /* where it is: a file read with no line is one that could not be read,
     which the page offers to add to the model's folder (docs/protocol.md
     "Errors") */
  Place place{};

  /* "file:line:col: what", leaving out what is not known ("line N: what"
     with no file) */
  std::string text() const
  {
    std::string t = place.file;
    if (place.line > 0) {
      t += t.empty() ? "line " : ":";
      t += std::to_string(place.line);
      if (place.col > 0) t += ":" + std::to_string(place.col);
    }
    if (!t.empty()) t += ": ";
    return t + what;
  }
};

/* a computation's value, or why it has none */
template <class T = void>
using Result = std::expected<T, Error>;

inline std::unexpected<Error> fail(std::string where, std::string what, Place place = {})
{
  return std::unexpected<Error>(Error{std::move(where), std::move(what), std::move(place)});
}

/* a failure to read `file` */
inline std::unexpected<Error> fail_reading(std::string where, std::string what, std::string file)
{
  return fail(std::move(where), std::move(what), Place{std::move(file)});
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
