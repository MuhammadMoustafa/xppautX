#ifndef XPP_MODEL_SWITCH_H
#define XPP_MODEL_SWITCH_H
/* File > Open model and File > Reload (W61): another model, or the same
   file again, loaded in this process. The command asks what it needs and
   leaves a request in the Session; the front end carries it out once the
   command has returned, when nothing of the Session before is in use
   (ui_json.cpp handle_line, json_model.cpp): load_requested builds the new
   Model and Session through xpp_load_model, and a load that fails leaves
   the ones before current, untouched, and says so. */

#ifdef __cplusplus
extern "C" {
#endif

/* File > Open model (key m) and {"cmd":"open"}: path, or the file the
   user picks when it is NULL or empty; asks before this model goes,
   offering to save its session first (xpp_session_save). A file that
   carries a model (W103, xpp_session.h: an AUTO file, .autox, or a
   session file, .snapx) loads its saved model, from its saved files, then
   its diagram or session; when the model open is that one (the same
   files, byte for byte) nothing is asked: an AUTO file's diagram goes
   into it, keeping its data, and a session file loads it again with the
   session. */
void xpp_model_open(const char *path);
/* File > Reload (key e) and {"cmd":"reload"}: the model's file again, with
   the command line it was loaded with (a saved model's from its saved
   files); the parameters, initial data and numerics keep their values by
   name (restore_values) */
void xpp_model_reload(void);

#ifdef __cplusplus
}

#include "load_eqn.h"
#include "model_files.h"
#include "xpp_session.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace xpp {

/* what a command asked to load: file with the command line command_line
   (the program's name first), in the folder dir ("" the working one) */
struct ModelRequest {
  std::string dir, file;
  std::vector<std::string> command_line;
  /* Reload: the values of the Session before carry over by name */
  bool keep_values=false;
  /* a saved model: its files, read in place of the disk's (model_files.h) */
  std::optional<SavedModel> saved;
  /* Open of an AUTO or session file: what it adds, restored once its
     model is loaded (xpp_saved_restore) */
  std::optional<SavedFile> restore;
};

/* the request the last command made, taken: nullopt when it made none */
std::optional<ModelRequest> take_model_request();

/* what Reload carries over, by name: each parameter's value, each
   variable's initial data (and a delay equation's history text), and the
   numerics (the Poincare section's variable by its name) */
struct KeptValues {
  std::vector<std::pair<std::string,double>> pars, ics;
  std::vector<std::pair<std::string,std::string>> delays;
  NumericsSettings numerics;
  std::string poivar;
};
/* the current model's and session's */
KeptValues keep_values();
/* into the current model and session, a name at a time: a name the model
   no longer has is left out, and what kept does not name keeps the value
   the load gave it */
void restore_values(const KeptValues &kept);

/* loads what req asks for: true when the new Model and Session are
   current; false (an error message said why) when the file cannot be
   read or loaded, the Model and Session before still current and the
   working folder the one before */
bool load_requested(const ModelRequest &req);

}
#endif
#endif
