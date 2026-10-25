/* xpp::image_formats (image_format.h, W53, issue #101; PDF in place of PostScript at W479, #542): each format
   listed once, with the extension the export menus and file dialogs use (PDF, SVG and the kinescope/array-plot
   GIF -- no PNG, decided on the card: the core has no rasteriser and SVG covers it). */
#include "xpptest.h"
#include "image_format.h"
#include "my_pdf.h"
#include "my_svg.h"
#include "picture.h"
#include "browse.h"
#include "xpp_io.h"

#include <cstring>
#include <set>
#include <string>

namespace {

/* a picture's file as text: the writer's bytes, read back */
std::string written(xpp::Result<> (*write)(const xpp::Picture &, xpp::Writer &), const xpp::Picture &p, const char *path)
{
    {
        xpp::Writer w = xpp::Writer::binary(path);
        CHECK(w && write(p, w) && xpp::commit_save(w));
    }
    std::string text;
    xpp::LineReader r(path);
    while (const auto line = r.next()) text += std::string(*line) + '\n';
    return text;
}

xpp::Color rgba(int r, int g, int b, int a = 255)
{
    return xpp::Color{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b), static_cast<std::uint8_t>(a), false};
}

bool has(const std::string &text, const std::string &part) { return text.find(part) != std::string::npos; }

} // namespace

int main(void)
{
    CHECK(xpp::n_image_formats == 4);

    std::set<std::string> names, extensions;
    for (int i = 0; i < xpp::n_image_formats; i++) {
        const xpp::ImageFormat &f = xpp::image_formats[i];
        CHECK(f.name != nullptr && f.name[0] != '\0');
        CHECK(f.extension != nullptr && f.extension[0] != '\0');
        /* no format is listed twice */
        CHECK(names.insert(f.name).second);
        CHECK(extensions.insert(f.extension).second);
    }

    CHECK(extensions.count("pdf") == 1);
    CHECK(extensions.count("svg") == 1);
    CHECK(extensions.count("gif") == 1);
    CHECK(extensions.count("ppm") == 1);
    CHECK(extensions.count("ps") == 0); /* PostScript went with W479 */
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_GIF].pixels != nullptr);
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_GIF].finish_movie != nullptr);
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_PPM].pixels != nullptr);

    /* PDF and SVG are vector pictures: each has the one writer of a Picture, PDF's file is bytes; GIF is a raster snapshot with none */
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_PDF].write == xpp::write_pdf && xpp::image_formats[xpp::IMAGE_FORMAT_PDF].binary);
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_SVG].write == xpp::write_svg && !xpp::image_formats[xpp::IMAGE_FORMAT_SVG].binary);
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_GIF].write == nullptr && xpp::image_formats[xpp::IMAGE_FORMAT_PPM].write == nullptr);
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_PDF].plot_key != nullptr && xpp::image_formats[xpp::IMAGE_FORMAT_SVG].plot_key != nullptr);

    CHECK(std::strcmp(xpp::image_formats[xpp::IMAGE_FORMAT_PDF].extension, "pdf") == 0);
    CHECK(std::strcmp(xpp::image_formats[xpp::IMAGE_FORMAT_SVG].extension, "svg") == 0);
    CHECK(std::strcmp(xpp::image_formats[xpp::IMAGE_FORMAT_GIF].extension, "gif") == 0);

    /* find_image_format_by_extension (xpp_util.cpp's dump_ps) looks up by the same extension field */
    CHECK(xpp::find_image_format_by_extension("pdf") == &xpp::image_formats[xpp::IMAGE_FORMAT_PDF]);
    CHECK(xpp::find_image_format_by_extension("svg") == &xpp::image_formats[xpp::IMAGE_FORMAT_SVG]);
    CHECK(xpp::find_image_format_by_extension("ps") == nullptr);
    CHECK(xpp::find_image_format_by_extension("png") == nullptr);
    CHECK(xpp::picture_format_names.size() == 2 && xpp::picture_format_names[0] == "pdf" && xpp::picture_format_names[1] == "svg");

    /* one picture, both writers: the colour with its alpha, the dash, a marker and a text each as the format says it */
    xpp::Picture p;
    xpp::Pen pen;
    pen.color = rgba(255, 0, 0, 128);
    pen.style.dash = xpp::LineDash::dashed;
    pen.style.width = 2;
    p.items.emplace_back(xpp::PicturePath{{{10, 20}, {30, 40}, {50, 20}}, pen});
    xpp::Pen square = pen;
    square.style.marker = xpp::LineMarker::square;
    p.items.emplace_back(xpp::PictureMarker{60, 70, square});
    p.items.emplace_back(xpp::centred_text(100, 100, xpp::TextAnchor::middle, 0, "a\\1b\\s2", 12));
    const std::string svg = written(xpp::write_svg, p, "build/test_picture.svg");
    CHECK(has(svg, "<polyline points=\"10,20 30,40 50,20\"") && has(svg, "stroke=\"#ff0000\"") && has(svg, "stroke-opacity=\"0.5\""));
    CHECK(has(svg, "stroke-dasharray=\"12,6\"") && has(svg, "<polygon") && has(svg, "<text") && has(svg, "β")); /* \1b is beta */
    const std::string pdf = written(xpp::write_pdf, p, "build/test_picture.pdf");
    CHECK(pdf.starts_with("%PDF-1.4") && has(pdf, "/BaseFont /Helvetica") && has(pdf, "/CA 0.502") && has(pdf, "[12 6] 0 d") && has(pdf, "1 0 0 RG"));
    CHECK(pdf == written(xpp::write_pdf, p, "build/test_picture.pdf")); /* the same picture, the same bytes */

    /* black and white: strokes and text black, fills gray, the alpha kept */
    xpp::Picture bw;
    bw.color = false;
    CHECK(xpp::ink(bw, rgba(255, 0, 0, 128)) == rgba(0, 0, 0, 128));
    CHECK(xpp::ink_fill(bw, rgba(255, 255, 255)) == rgba(255, 255, 255));
    CHECK(xpp::ink(bw, xpp::foreground_color) == rgba(0, 0, 0));
    CHECK(xpp::ink(p, xpp::foreground_color) == rgba(0, 0, 0));

    /* the text's markup: runs of one font, size and rise each */
    const std::vector<xpp::TextRun> runs = xpp::text_runs("x\\s2\\nz\\1a", 10);
    CHECK(runs.size() == 4 && runs[0].text == "x" && runs[1].text == "2" && runs[1].size == 6 && runs[1].rise < 0 && runs[2].rise == 0
          && runs[3].symbol && xpp::symbol_to_unicode(runs[3].text) == "α");

    TEST_REPORT("image_format");
}
