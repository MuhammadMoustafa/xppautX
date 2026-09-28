/* The picture-export registry (W53, issue #101): one row per format, so
   the call sites that used to branch on which format was chosen
   (graphics.cpp's PSFMT/SVGFMT primitive dispatch, AUTO's export_diagram
   picking ps_init/ps_end vs. svg_init/svg_end, graf_par.cpp's
   create_ps/create_svg pair, xpp_util.cpp's dump_ps, json_windows.cpp's
   duplicated still-GIF writer) look it up here instead. Output is
   unchanged: tools/goldencheck.py's .ps/.svg files and the
   kinescope/array-plot GIFs still come from exactly the same
   ps_.../svg_.../gif_stuff_ppm calls as before. */
#include "image_format.h"
#include "my_ps.h"
#include "my_svg.h"
#include "xpp_util.h"

namespace xpp {

const ImageFormat image_formats[n_image_formats] = {
  {"Postscript", "ps",  ps_init,  ps_end,  ps_restore,  ps_ask_params,
   ps_point,  ps_line,  ps_bead,  ps_frect,  ps_text,  ps_linetype,  special_put_text_ps},
  {"SVG",        "svg", svg_init, svg_end, svg_restore, nullptr,
   svg_point, svg_line, svg_bead, svg_frect, svg_text, svg_linetype, special_put_text_svg},
  {"GIF",        "gif", nullptr,  nullptr, nullptr,     nullptr,
   nullptr,   nullptr,  nullptr,  nullptr,   nullptr,  nullptr,      nullptr},
};

const ImageFormat *find_image_format_by_extension(std::string_view extension)
{
  for (const ImageFormat &f : image_formats)
    if (extension == f.extension) return &f;
  return nullptr;
}

} // namespace xpp
