#ifndef XPP_EXAMPLES_H
#define XPP_EXAMPLES_H
/* The bundled examples (W233): the .odex models a release ships in the
   examples/ folder beside the program (files::program_dir; the .app's
   Contents/Resources), which the start screen lists (hello's `start`,
   docs/protocol.md "Start screen"). One rule, no search: that folder and
   no other (a development checkout, whose executable has no examples/
   beside it, lists none and says where it looked).

   The folder is the program's own and may be read-only (an installed
   program), and a model writes its outputs and converted files beside
   itself, so an example is never opened where it is: install() copies it into
   the user's config folder (files::config_dir) under examples/, and the
   copy is what Open model loads. The page names an example by its base
   name only; the core finds it in the listing and reads it with
   files::open_read_within. C++ only. */
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "xpp_error.h"

namespace xpp::examples {

inline constexpr std::string_view FOLDER = "examples"; /* beside the program, and in the config folder for the copies */

/* how many examples are listed: the release ships a handful, and a folder
   with more than this is not ours; the listing stops there */
inline constexpr std::size_t MAX_EXAMPLES = 100;
/* the largest example copied: a bundled example is a few KB; a bigger
   file is not one of ours */
inline constexpr std::size_t MAX_EXAMPLE_BYTES = 1u << 20;

struct Listing {
    std::string folder;             /* where they are looked for ("" when the program's folder is unknown) */
    std::vector<std::string> names; /* base names, sorted */
    std::string reason;             /* why there are none ("" when there are some) */
};
Listing list();

/* hello.start's "examples": {"folder":..., "names":[...], "reason":...} (reason only when there are none) */
std::string json();

/* The copy of the example `name` in the config folder's examples/, made
   (the folder too) after asking before replacing one that is there (the
   Replace question of every save, browse.h open_writer_asking): its path,
   or "" when the user declined (nothing is changed). An error when `name` is
   not in list() (a path, "..", a link, a file that is not there are all
   that), when it cannot be read or when there is no config folder. */
Result<std::string> install(std::string_view name);

} // namespace xpp::examples

#endif
