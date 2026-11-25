#pragma once
/* What a command asked the program to load, and what a Reload carries over: data and the functions that make and read it, below
   the model switch (model_switch.h, the application) that carries a request out. The Session holds a request (session.h);
   the session file's saved model and the last saved session are its parts. Split from model_switch.h and xpp_session.h at W737
   so the Session, the value undo and the front end name them without including the application. */
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "autox_options.h"
#include "model_files.h"
#include "model_numerics.h"

namespace xpp {
struct Session; /* session.h */
struct Model;   /* model.h */

namespace snapx {
/* the manifest, the first keys of the state file's session statement: `model` and `data` */
struct Manifest {
    std::string model_name; /* the model's own file, as it names it: the member model/<model_name> */
    bool data = false;      /* data.npz holds the data table (a session file's alone) */
    bool operator==(const Manifest &) const = default;
};
} // namespace snapx

} // namespace xpp

/* the session file last saved or opened; a Session's (session.h) */
struct SavedSession {
    std::string file;           /* the .snapx */
    /* the session's fingerprint when it was saved, opened or loaded (xpp_session_changed); nothing when it could not be made,
       which counts as changed */
    std::optional<std::uint64_t> clean;
};

/* A session archive carries its saved model and all session members.
   The reader checks the whole file before the model is kept. */
struct SavedFile {
    std::string path;              /* absolute */
    std::string name;              /* what a message calls it: its file name, or a recording's snapshot */
    bool snapshot = false;         /* a recording's snapshot (recx.h): restored as a session, but no session file */
    xpp::snapx::Manifest manifest;
    std::map<std::string, std::string> members; /* every member, by its name */
    xpp::SavedModel model;         /* the model's files, saved in path */
};

namespace xpp {

/* the program's name a command line starts with: the one the model was loaded with */
std::string program_name(const Model &m);

/* what a command asked to load: file with the command line command_line
   (the program's name first), in the folder dir ("" the working one) */
struct ModelRequest {
  std::string dir, file;
  std::vector<std::string> command_line;
  /* Reload: the values of the Session before carry over by name */
  bool keep_values=false;
  /* a saved model: its files, read in place of the disk's (model_files.h) */
  std::optional<SavedModel> saved;
  /* Open of a session file: what it adds, restored once its
     model is loaded (xpp_saved_restore) */
  std::optional<SavedFile> restore;
  /* the folder file dialogs start in once it is loaded ("" the folder it
     loads in): a replay loads in a scratch folder and shows its
     recording's */
  std::string home;
};

/* a request for the model file `file`, loaded in the folder dir as a
   double-click starts it (the command line: the program's name, then the
   file), in place of the model of s */
ModelRequest open_request(const Session &s, std::string dir, std::string file);
/* a request for the model saved in f (a session file or a
   recording's snapshot), loaded in the folder dir with the command line
   it was saved with, in place of the model of s; then what f adds */
ModelRequest saved_request(const Session &s, std::string dir, SavedFile f);

/* the request the last command in s made, taken: nullopt when it made none */
std::optional<ModelRequest> take_model_request(Session &s);

/* what Reload carries over, by name: each parameter's value, each
   variable's initial data (and a delay equation's history text), and the
   numerics (the Poincare section's variable by its name), and the
   session file the session was last saved to or opened from */
struct KeptValues {
  std::vector<std::pair<std::string,double>> pars, ics;
  std::vector<std::pair<std::string,std::string>> delays;
  NumericsSettings numerics;
  AutoxSettingsSet autox_options;
  std::string poincare_variable;
  /* the session file and the values it was saved with (W218): the next
     Ctrl+S saves to it, and the changed flag still compares with that
     save, so the data and diagram Reload drops read as a change */
  SavedSession saved;
};
/* the session's and its model's */
KeptValues keep_values(const Session &s);
/* into the session s, just loaded, and its model, a name at a time: a
   name the model no longer has is left out, and what kept does not name
   keeps the value the load gave it */
void restore_values(Session &s, const KeptValues &kept);

} // namespace xpp
