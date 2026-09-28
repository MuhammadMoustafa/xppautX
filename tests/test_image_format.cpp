/* xpp::image_formats (image_format.h, W53, issue #101): each format
   listed once, with the extension the export menus and file dialogs use
   (PostScript, SVG and the kinescope/array-plot GIF -- no PNG, decided
   on the card: the core has no rasteriser and SVG covers it). */
#include "xpptest.h"
#include "image_format.h"

#include <cstring>
#include <set>
#include <string>

int main(void)
{
    CHECK(xpp::n_image_formats == 3);

    std::set<std::string> names, extensions;
    for (int i = 0; i < xpp::n_image_formats; i++) {
        const xpp::ImageFormat &f = xpp::image_formats[i];
        CHECK(f.name != nullptr && f.name[0] != '\0');
        CHECK(f.extension != nullptr && f.extension[0] != '\0');
        /* no format is listed twice */
        CHECK(names.insert(f.name).second);
        CHECK(extensions.insert(f.extension).second);
    }

    CHECK(extensions.count("ps") == 1);
    CHECK(extensions.count("svg") == 1);
    CHECK(extensions.count("gif") == 1);

    /* PostScript and SVG are vector pictures written through begin/end
       (ps_init/ps_end, svg_init/svg_end); GIF is a raster snapshot with
       none of its own */
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_PS].begin != nullptr);
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_PS].end != nullptr);
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_SVG].begin != nullptr);
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_SVG].end != nullptr);
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_GIF].begin == nullptr);
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_GIF].end == nullptr);

    CHECK(std::strcmp(xpp::image_formats[xpp::IMAGE_FORMAT_PS].extension, "ps") == 0);
    CHECK(std::strcmp(xpp::image_formats[xpp::IMAGE_FORMAT_SVG].extension, "svg") == 0);
    CHECK(std::strcmp(xpp::image_formats[xpp::IMAGE_FORMAT_GIF].extension, "gif") == 0);

    /* the vector formats' primitive writers (graphics.cpp's former
       PSFMT/SVGFMT dispatch) are all filled in; GIF (a raster snapshot)
       has none */
    for (int i = 0; i < 2; i++) {
        const xpp::ImageFormat &f = xpp::image_formats[i];
        CHECK(f.restore != nullptr);
        CHECK(f.draw_point != nullptr);
        CHECK(f.draw_line != nullptr);
        CHECK(f.draw_bead != nullptr);
        CHECK(f.draw_frect != nullptr);
        CHECK(f.draw_text != nullptr);
        CHECK(f.draw_linetype != nullptr);
        CHECK(f.draw_special_text != nullptr);
    }
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_GIF].restore == nullptr);
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_GIF].draw_point == nullptr);

    /* only PostScript asks its own parameter dialog first */
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_PS].ask_params != nullptr);
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_SVG].ask_params == nullptr);
    CHECK(xpp::image_formats[xpp::IMAGE_FORMAT_GIF].ask_params == nullptr);

    /* find_image_format_by_extension (xpp_util.cpp's dump_ps) looks up
       by the same extension field */
    const xpp::ImageFormat *ps = xpp::find_image_format_by_extension("ps");
    CHECK(ps == &xpp::image_formats[xpp::IMAGE_FORMAT_PS]);
    const xpp::ImageFormat *svg = xpp::find_image_format_by_extension("svg");
    CHECK(svg == &xpp::image_formats[xpp::IMAGE_FORMAT_SVG]);
    CHECK(xpp::find_image_format_by_extension("png") == nullptr);

    TEST_REPORT("image_format");
}
