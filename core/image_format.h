#ifndef _image_format_h_
#define _image_format_h_
/* xpp::ImageFormat: the picture-export registry (W53, issue #101). Same
   idea as W51/W52's Solver/DataFormat tables: one row per format instead
   of a per-format branch at each place a picture is written or drawn
   into. C++ only (every caller is a .cpp file). */
#include <string_view>
#include <string>
#include <span>
#include <vector>

#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */
class Writer;
struct PlotFileState;
struct ArrayPicture;

/* PostScript and SVG are vector pictures: begin opens the file (renamed
   into place at end, my_ps.cpp/my_svg.cpp's own xpp::Writer); ask_params
   is an extra parameter dialog before the file selector (PostScript's
   BW/colour, portrait, font; nullptr where a format asks nothing); the
   draw_* primitives are what graphics.cpp's point/line/bead/frect/
   put_text/set_linestyle/fancy_text_abs call while the picture is drawn
   (its former PSFMT/SVGFMT if/else chain); restore finishes the picture
   (redraws the plot into it, closes it via end -- xpp_util.cpp's
   ps_restore/svg_restore). GIF (the kinescope's and the array plot's,
   json_windows.cpp) and PPM use the pixels hook; frame -1 is a still,
   zero starts a movie, and subsequent frame numbers append to it.
   Each draws into the picture of the Session it is given (W47d). */
struct ImageFormat {
  const char *name;                                 /* "Postscript", "SVG", "GIF" */
  const char *extension;                            /* no dot: "ps", "svg", "gif" */
  Result<> (*begin)(Session &s, const char *filename, int color); /* or why it cannot */
  void (*end)(Session &s);
  void (*restore)(Session &s);
  int (*ask_params)(Session &s);                    /* 0: the user cancelled */
  void (*draw_point)(Session &s, int x, int y);
  void (*draw_line)(Session &s, int x1, int y1, int x2, int y2);
  void (*draw_bead)(Session &s, int x1, int y1);
  void (*draw_frect)(Session &s, int x1, int y1, int w, int h);
  void (*draw_text)(Session &s, int x, int y, const char *str);
  void (*draw_linetype)(Session &s, int linetype);
  void (*draw_special_text)(Session &s, int x, int y, const char *str, int size);
  int plot_flag = 0; /* graf_par.h's vector drawing mode; zero for raster rows */
  void (*color)(PlotFileState &pf, int color) = nullptr;
  void (*group)(Session &s, bool begin, bool direction_field) = nullptr;
  void (*y_label)(Session &s, int x, int y, const char *str) = nullptr;
  int key_direction = 1; /* PostScript's coordinates grow upwards */
  void (*pixels)(Writer &w, std::span<const unsigned char> rgb, int width, int height, int frame) = nullptr;
  void (*finish_movie)(Writer &w) = nullptr;
  Result<> (*array)(const ArrayPicture &) = nullptr;
  const char *plot_title = nullptr;
  std::string (*plot_filename)(const Session &s) = nullptr;
  const char *plot_key = nullptr;
  std::vector<std::string> (*parameter_values)(const Session &s) = nullptr;
};

extern const ImageFormat image_formats[];
constexpr int n_image_formats = 4; /* one row for each supported picture format */
/* image_formats' own order, for a caller that picks by index (the AUTO
   diagram's export menu, graf_par.cpp's plot-window export) rather than
   by name */
constexpr int IMAGE_FORMAT_PS = 0;
constexpr int IMAGE_FORMAT_SVG = 1;
constexpr int IMAGE_FORMAT_GIF = 2;
constexpr int IMAGE_FORMAT_PPM = 3;
constexpr int IMAGE_STILL_FRAME = -1; /* a still has no index in a movie */

const ImageFormat *active_image_format(const Session &s);
void image_color(Session &s, int color);
void image_group(Session &s, bool begin, bool direction_field = false);
Result<bool> save_pixels(const ImageFormat &format, const char *filename, std::span<const unsigned char> rgb, int width, int height);

/* the format whose extension is `extension` (a Session's plot_export.format's
   own values, "ps"/"svg": xpp_util.cpp's dump_ps), or nullptr */
const ImageFormat *find_image_format_by_extension(std::string_view extension);

} // namespace xpp
#endif
