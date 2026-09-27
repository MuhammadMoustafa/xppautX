#ifndef XPP_MODEL_H
#define XPP_MODEL_H
/* xpp::Model: what loading a model produces (CLAUDE.md "No global state";
   docs/roadmap.md W46c, the start of W47b). C++ only.

   For now there is one Model, reached through xpp::model(); W47b moves
   the rest of what a load produces here, and W47d passes it explicitly.

   The name tables hold a model's names as the parser keeps them (blanks
   removed, upper case), each at most XPP_NAME_MAX long: the parser refuses
   a longer one (name_too_long), so nothing here cuts a name. A display of
   fixed width shortens one with short_name (xpp_util.h). The tables have
   the parser's fixed limits as their sizes (xpplim.h): the parser writes
   an entry by its index (a variable, a Markov variable, an aux quantity
   each at its own offset), and an index past what the model uses reads
   an empty name. */
#include "xpplim.h"

#include <array>
#include <string>
#include <vector>

namespace xpp {

struct Model {
  /* the variables' names by index: the ODEs (NODE), the Markov variables
     (NMarkov), then the aux quantities, NEQ in all; browse_data's added
     column takes the next one */
  std::array<std::string, MAXODE> uvar_names;
  /* the parameters' names (NUPAR of them) */
  std::array<std::string, MAXPAR> upar_names;
  /* the user functions' names (NFUN of them) */
  std::array<std::string, MAXUFUN> ufun_names;
  /* each user function's argument names (narg_fun[i] of them) */
  std::array<std::vector<std::string>, MAXUFUN> ufun_args;
  /* the loaded file's path, as given ("console" for standard input) */
  std::string this_file;
  /* "_<name>" of the internal set last applied, "" when none */
  std::string this_internset;
};

/* the one Model, for the program's life */
Model &model();

}

#endif
