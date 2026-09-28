#ifndef _image_format_h_
#define _image_format_h_
/* xpp::ImageFormat: the picture-export registry (W53, issue #101). Same
   idea as W51/W52's Solver/DataFormat tables: one row per format instead
   of a per-format branch at each place a picture is written or drawn
   into. C++ only (every caller is a .cpp file). */
#include <string_view>

namespace xpp {

/* PostScript and SVG are vector pictures: begin opens the file (renamed
   into place at end, my_ps.cpp/my_svg.cpp's own xpp::Writer); ask_params
   is an extra parameter dialog before the file selector (PostScript's
   BW/colour, portrait, font; nullptr where a format asks nothing); the
   draw_* primitives are what graphics.cpp's point/line/bead/frect/
   put_text/set_linestyle/fancy_text_abs call while the picture is drawn
   (its former PSFMT/SVGFMT if/else chain); restore finishes the picture
   (redraws the plot into it, closes it via end -- xpp_util.cpp's
   ps_restore/svg_restore). GIF (the kinescope's and the array plot's,
   json_windows.cpp) is a raster snapshot written straight from pixels:
   every field but name/extension is null for it. */
struct ImageFormat {
  const char *name;                                 /* "Postscript", "SVG", "GIF" */
  const char *extension;                            /* no dot: "ps", "svg", "gif" */
  int (*begin)(const char *filename, int color);
  void (*end)();
  void (*restore)();
  int (*ask_params)();                              /* 0: the user cancelled */
  void (*draw_point)(int x, int y);
  void (*draw_line)(int x1, int y1, int x2, int y2);
  void (*draw_bead)(int x1, int y1);
  void (*draw_frect)(int x1, int y1, int w, int h);
  void (*draw_text)(int x, int y, const char *str);
  void (*draw_linetype)(int linetype);
  void (*draw_special_text)(int x, int y, const char *str, int size);
};

extern const ImageFormat image_formats[];
constexpr int n_image_formats = 3;
/* image_formats' own order, for a caller that picks by index (the AUTO
   diagram's export menu, graf_par.cpp's plot-window export) rather than
   by name */
constexpr int IMAGE_FORMAT_PS = 0;
constexpr int IMAGE_FORMAT_SVG = 1;
constexpr int IMAGE_FORMAT_GIF = 2;

/* the format whose extension is `extension` (xpp::session().plot_export.format's
   own values, "ps"/"svg": xpp_util.cpp's dump_ps), or nullptr */
const ImageFormat *find_image_format_by_extension(std::string_view extension);

} // namespace xpp
#endif
