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
#include "session.h"
#include "model.h"
#include "graf_par.h"
#include "scrngif.h"
#include "browse.h"
#include "array_print.h"

namespace xpp {

namespace {

// Keep the existing default-name limit, leaving room for its extension.
constexpr int plot_filename_limit = 250;
// SVG's default retains the existing removal of a four-character .ode suffix.
constexpr std::size_t plot_model_suffix = 4;
std::string ps_filename(const Session &s) { return xpp::format("{:.{}}.ps",s.model().this_file,plot_filename_limit); }
std::string svg_filename(const Session &s)
{
  std::string filename=s.model().this_file;
  filename.resize(filename.size()>=plot_model_suffix?filename.size()-plot_model_suffix:0);
  return filename+".svg";
}

void svg_group(Session &s, bool begin, bool direction_field)
{
  if (direction_field) s.nullclines.doing_dfield = begin;
  svg_write(s.plot_file, begin ? "<g>" : "</g>");
}

void svg_label(Session &s, int x, int y, const char *str)
{
  svg_y_axis_label(s.plot_file, x, y, str);
}

void gif_pixels(Writer &w, std::span<const unsigned char> rgb, int width, int height, int frame)
{
  // A six-level RGB cube fits GIF's 256-colour palette, as before W137.
  constexpr int color_step = 51;
  std::vector<unsigned char> palette_rgb(rgb.begin(), rgb.end());
  for (unsigned char &c : palette_rgb) c = static_cast<unsigned char>(((c + color_step / 2) / color_step) * color_step);
  gif_stuff_ppm(palette_rgb.data(), width, height, w.file(), frame == IMAGE_STILL_FRAME ? MAKE_ONE_GIF : frame == 0 ? FIRST_ANI_GIF : NEXT_ANI_GIF);
}

void gif_finish(Writer &w) { end_ani_gif(w.file()); }

void ppm_pixels(Writer &w, std::span<const unsigned char> rgb, int width, int height, int)
{
  w.print("P6\n{} {}\n255\n", width, height);
  w.write(std::string_view(reinterpret_cast<const char *>(rgb.data()), rgb.size()));
}

} // namespace

const ImageFormat image_formats[n_image_formats] = {
  {"Postscript", "ps",  ps_init,  ps_end,  ps_restore,  ps_ask_params,
   ps_point,  ps_line,  ps_bead,  ps_frect,  ps_text,  ps_linetype,  special_put_text_ps,
   PSFMT, ps_do_color, nullptr, nullptr, -1, nullptr, nullptr, array_print, "Print postscript", ps_filename, "p", ps_parameter_values},
  {"SVG",        "svg", svg_init, svg_end, svg_restore, nullptr,
   svg_point, svg_line, svg_bead, svg_frect, svg_text, svg_linetype, special_put_text_svg,
   SVGFMT, svg_do_color, svg_group, svg_label, 1, nullptr, nullptr, nullptr, "Print svg", svg_filename, "v"},
  {"GIF",        "gif", nullptr,  nullptr, nullptr,     nullptr,
   nullptr,   nullptr,  nullptr,  nullptr,   nullptr,  nullptr,      nullptr,
   SCRNFMT, nullptr, nullptr, nullptr, 1, gif_pixels, gif_finish},
  {"PPM", "ppm", nullptr, nullptr, nullptr, nullptr,
   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
   SCRNFMT, nullptr, nullptr, nullptr, 1, ppm_pixels},
};

const ImageFormat *active_image_format(const Session &s)
{
  for (const ImageFormat &f : image_formats)
    if (f.begin && f.plot_flag == s.plot_file.plt_fmt_flag) return &f;
  return nullptr;
}

void image_color(Session &s, int color)
{
  if (const ImageFormat *f = active_image_format(s); f && f->color) f->color(s.plot_file, color);
}

void image_group(Session &s, bool begin, bool direction_field)
{
  if (const ImageFormat *f = active_image_format(s); f && f->group) f->group(s, begin, direction_field);
}

Result<bool> save_pixels(const ImageFormat &format, const char *filename, std::span<const unsigned char> rgb, int width, int height)
{
  Result<> opened;
  Writer w = open_writer_asking(filename, true, &opened);
  if (!opened) return std::unexpected(opened.error());
  if (!w) return false;
  format.pixels(w, rgb, width, height, IMAGE_STILL_FRAME);
  if (Result<> saved = commit_save(w); !saved) return std::unexpected(saved.error());
  return true;
}

const ImageFormat *find_image_format_by_extension(std::string_view extension)
{
  for (const ImageFormat &f : image_formats)
    if (extension == f.extension) return &f;
  return nullptr;
}

} // namespace xpp
