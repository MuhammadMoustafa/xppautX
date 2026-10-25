#include "browse.h"
/* Print arrayplot: the array plot as a picture of coloured cells with its colour scale, labels and
   ranges (W479, #542). It is a Picture like a plot window's, so colour, text and the file are
   picture.h's and the PDF writer's; the colours are the session's ramp, the one the page shows. */
#include <algorithm>
#include <cmath>
#include <string>

#include "array_print.h"
#include "arrayplot.h"
#include "image_format.h"
#include "my_pdf.h"
#include "picture.h"
#include "session.h"
#include "xpp_io.h"

namespace xpp {

namespace {

/* the plot's box in the picture's points, and the colour scale's bar beside it */
constexpr double box_left = 100, box_right = 460, box_top = 50, box_bottom = 340;
constexpr double scale_left = 500, scale_width = 20, scale_top = 80, scale_height = 200;
constexpr int scale_steps = 64; /* the bar's bands, each a colour of the ramp */
constexpr double seam = .5;     /* a cell or band overlaps the next by this much: no hairline of the page's colour between two */
constexpr double label_size = 12, number_gap = 6; /* points */

void add_rect(Picture &p, double x, double y, double w, double h, const Color &color)
{
    p.items.emplace_back(PictureRect{x, y, w, h, color});
}

void add_text(Picture &p, double x, double y, TextAnchor anchor, double angle, const std::string &text)
{
    if (!text.empty()) p.items.emplace_back(centred_text(x, y, anchor, angle, text, label_size));
}

} // namespace

Result<> array_print(const Session &s, const ArrayPicture &a)
{
    Picture p;
    p.font_size = label_size;
    const int across = a.nacross / a.ncskip;
    const double cell_w = (box_right - box_left) / across, cell_h = (box_bottom - box_top) / a.ndown;
    const bool scaled = a.zmax > a.zmin;
    for (int i = 0; i < across; i++) {
        const int column = a.col0 + i * a.ncskip;
        if (column > a.maxcol) break;
        for (int j = 0; j < a.ndown; j++) {
            const int row = a.row0 + j * a.nskip;
            if (row >= a.maxrow || row < 0 || !scaled) continue;
            const double fill = (a.data[column][row] - a.zmin) / (a.zmax - a.zmin);
            add_rect(p, box_left + i * cell_w, box_top + j * cell_h, cell_w + seam, cell_h + seam, ramp_color(s.ramp(), fill));
        }
    }
    Pen border;
    border.style.width = 1.5;
    p.items.emplace_back(PicturePath{{{box_left, box_top}, {box_right, box_top}, {box_right, box_bottom}, {box_left, box_bottom}, {box_left, box_top}}, border});
    for (int k = 0; k < scale_steps; k++) {
        const double band = scale_height / scale_steps;
        add_rect(p, scale_left, scale_top + k * band, scale_width, band + seam, ramp_color(s.ramp(), 1 - (k + .5) / scale_steps));
    }
    const double middle_x = .5 * (box_left + box_right), middle_y = .5 * (box_top + box_bottom);
    add_text(p, middle_x, box_top - 3 * number_gap, TextAnchor::middle, 0, a.xtitle);
    add_text(p, box_left - 5 * number_gap - 20, middle_y, TextAnchor::middle, 90, a.ytitle);
    add_text(p, middle_x, box_bottom + 4 * number_gap, TextAnchor::middle, 0, a.bottom);
    add_text(p, box_left - number_gap, box_top, TextAnchor::end, 0, xpp::format("{:g}", a.tlo));
    add_text(p, box_left - number_gap, box_bottom, TextAnchor::end, 0, xpp::format("{:g}", a.thi));
    add_text(p, scale_left + scale_width + number_gap, scale_top, TextAnchor::start, 0, xpp::format("{:g}", a.zmax));
    add_text(p, scale_left + scale_width + number_gap, scale_top + scale_height, TextAnchor::start, 0, xpp::format("{:g}", a.zmin));

    Result<> opened;
    Writer out = open_writer_asking(a.filename, true, &opened);
    if (!opened || !out) return opened;
    if (const Result<> written = write_pdf(p, out); !written) return written;
    return commit_save(out);
}

} // namespace xpp
