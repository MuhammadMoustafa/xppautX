#ifndef JSON_NUMBER_H
#define JSON_NUMBER_H
/* The one JSON number writer (docs/protocol.md, CLAUDE.md "Single source"):
   JSON has no NaN or Infinity, so a non-finite double becomes the literal
   "null" instead of "-nan"/"inf", which is not valid JSON. Everything that
   writes a JSON number -- the protocol front end (json_io.cpp and the
   other json_*.cpp files, through ui_json_internal.h's buf_num()) and the
   plot-data/series modules that build their own text (plot_data.cpp,
   series_enc.cpp) -- goes through this. Pure: no core state, no I/O.
   C++ only (std::string in the API); every caller is a .cpp file. */
#ifndef __cplusplus
#error "json_number.h is C++ only"
#endif
#include <string>

namespace xpp::json {

/* Whether v is a JSON number: JSON has no NaN or Infinity. */
bool json_finite(double v);

/* Appends v to s: "null" when not finite, else a 'g' conversion with sig
   significant digits (sig e.g. 7, 16, 17). */
void json_append_number(std::string &s, double v, int sig);

/* Appends v to s: "null" when not finite, else the shortest of 15 or 17
   significant digits that reads back exactly as v (double round-trip),
   as plot_data.cpp's series/plot bounds use. */
void json_append_number_shortest(std::string &s, double v);

/* Appends ,"name": and v (json_append_number_shortest) to s: one more
   numeric field of an object being built (name is a plain key, not
   escaped). */
void json_append_field(std::string &s, const char *name, double v);

} // namespace xpp::json

#endif
