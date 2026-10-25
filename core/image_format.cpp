/* The picture-export registry (W53, issue #101): one row per format, so the call sites that used to
   branch on which format was chosen look it up here instead. A vector picture is recorded by
   picture.cpp and written by its row's writer (my_pdf.cpp, my_svg.cpp, W479); GIF and PPM take the
   page's pixels. */
#include "image_format.h"
#include "graphics.h"
#include "model.h"
#include "my_pdf.h"
#include "my_svg.h"
#include "picture.h"
#include "scrngif.h"
#include "session.h"
#include "xpp_files.h"
#include "xpp_globals.h"
#include "xpp_ui.h"
#include "xpp_util.h"

namespace xpp {

namespace {

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
  {.name = "PDF", .extension = "pdf", .write = write_pdf, .binary = true, .plot_title = "Print pdf", .plot_key = "p"},
  {.name = "SVG", .extension = "svg", .write = write_svg, .plot_title = "Print svg", .plot_key = "v"},
  {.name = "GIF", .extension = "gif", .pixels = gif_pixels, .finish_movie = gif_finish},
  {.name = "PPM", .extension = "ppm", .pixels = ppm_pixels},
};

int ask_picture_params(Session &s)
{
  std::array<FormField, 2> fields = {FormField::boolean("Colour", s.plot_export.color),
                                     FormField::real_number("Axes font size", s.plot_export.font_size, 1.0, 100.0)};
  if (!ask_form(s, "Picture parameters", fields)) return 0;
  s.plot_export.color = fields[0].flag;
  s.plot_export.font_size = fields[1].real;
  return 1;
}

std::vector<std::string> picture_parameter_values(const Session &s)
{
  return {s.plot_export.color ? "true" : "false", number(s.plot_export.font_size)};
}

std::string picture_filename(const Session &s, const ImageFormat &picture_format)
{
  return files::output_name(s.model().this_file, xpp::format(".{}", picture_format.extension));
}

Result<> begin_picture(Session &s, const ImageFormat &format, std::string_view filename, bool color)
{
  Result<> opened;
  s.plot_file.writer = open_writer_asking(std::string(filename).c_str(), format.binary, &opened);
  if (!opened || !s.plot_file.writer) return opened;
  s.plot_file.format = &format;
  s.plot_file.picture = Picture{};
  s.plot_file.picture.color = color;
  s.plot_file.picture.font_size = s.plot_export.font_size;
  s.plot_file.pen = Pen{};
  s.plot_file.text_color = foreground_color;
  init_picture(s);
  return {};
}

void end_picture(Session &s)
{
  PlotFileState &f = s.plot_file;
  if (f.writer) {
    if (const Result<> written = f.format->write(f.picture, f.writer); !written) ok_or_show(written);
    else ok_or_show(commit_save(f.writer));
  }
  f.format = nullptr;
  f.picture = Picture{};
  if (program.interactive) init_x11(s);
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
