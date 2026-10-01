#ifndef XPP_DIAGNOSTIC_H
#define XPP_DIAGNOSTIC_H
/* xpp::Diagnostic: a problem found in a model, as a value (W63c). C++
   only. A load that fails says why with one (xpp::load_model and
   xpp::model_failed, xpp_batch.h; xpp::LoadFailed, session.h), the .odex
   reader's problems are ones (odex::error_at, odex.h), and the JSON front
   end sends one as the `error` event (docs/protocol.md). */
#include <string>

namespace xpp {

struct Diagnostic {
  /* the file the problem is in: the model's, or a file it includes; ""
     when none */
  std::string file;
  /* where in it, both from 1; 0 when not known (a problem of the whole
     model, or a column the reader cannot tell) */
  int line = 0;
  int col = 0;
  /* what is wrong: one line, or several (a formula and a caret under the
     place it stops making sense) */
  std::string cause;
  /* line `line` of `file` as written, when it could be read */
  std::string source;

  /* "file:line:col: cause", leaving out what is not known */
  std::string text() const
  {
    std::string t = file;
    if (line > 0) t += ":" + std::to_string(line);
    if (line > 0 && col > 0) t += ":" + std::to_string(col);
    if (!t.empty()) t += ": ";
    return t + cause;
  }
};

}

#endif
