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
  // A six-level RGB cube fits GIF's 256-color palette, as before W137.
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
  {.name = "PDF", .extension = "pdf", .write = write_pdf, .binary = true},
  {.name = "SVG", .extension = "svg", .write = write_svg},
  {.name = "GIF", .extension = "gif", .pixels = gif_pixels, .finish_movie = gif_finish},
  {.name = "PPM", .extension = "ppm", .pixels = ppm_pixels},
};

Result<> begin_picture(Session &s, const ImageFormat &format, std::string_view filename)
{
  Result<> opened;
  s.plot_file.writer = open_writer_asking(std::string(filename).c_str(), format.binary, &opened);
  if (!opened || !s.plot_file.writer) return opened;
  s.plot_file.format = &format;
  s.plot_file.picture = Picture{};
  s.plot_file.pen = Pen{};
  s.plot_file.text_color = foreground_color;
  init_picture(s);
  return {};
}

Result<> end_picture(Session &s)
{
  PlotFileState &f = s.plot_file;
  Result<> result;
  if (f.writer) {
    result = f.format->write(f.picture, f.writer);
    if (result) result = commit_save(f.writer);
  }
  f.format = nullptr;
  f.picture = Picture{};
  if (program.interactive) init_x11(s);
  return result;
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

const ImageFormat *picture_format_of_file(std::string_view path)
{
  for (const ImageFormat &f : image_formats)
    if (f.write && files::has_extension(path, xpp::format(".{}", f.extension))) return &f;
  return nullptr;
}

} // namespace xpp
