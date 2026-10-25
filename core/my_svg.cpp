/* The SVG writer (W479, #542): a Picture as an SVG, 640 x 400. Every item carries its own colour (#rrggbb with
   stroke-opacity or fill-opacity when it is not opaque), dash and width as attributes; nothing else is needed
   to draw it. The same picture is the same bytes (tools/goldencheck.py). */
#include "my_svg.h"

#include <string>

#include "xpp_io.h"

namespace xpp {

namespace {

std::string num(double v)
{
    std::string s = xpp::format("{:.2f}", v);
    while (s.back() == '0') s.pop_back();
    if (s.back() == '.') s.pop_back();
    return s == "-0" ? "0" : s;
}

std::string escaped(std::string_view text)
{
    std::string out;
    for (const char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += c;
        }
    }
    return out;
}

std::string hex(const Color &c) { return print_color(Color{c.r, c.g, c.b, 255, false}); }

/* the colour as a paint attribute (`stroke` or `fill`) and its opacity when not opaque */
std::string paint(const char *attribute, const Color &c)
{
    std::string s = xpp::format(" {}=\"{}\"", attribute, hex(c));
    if (c.a != 255) s += xpp::format(" {}-opacity=\"{}\"", attribute, num(c.a / 255.0));
    return s;
}

std::string stroke(const Pen &pen)
{
    std::string s = paint("stroke", pen.color) + xpp::format(" stroke-width=\"{}\"", num(pen.style.width));
    const std::vector<double> dashes = dash_pattern(pen.style.dash, pen.style.width);
    if (!dashes.empty()) {
        std::string list;
        for (const double d : dashes) list += (list.empty() ? "" : ",") + num(d);
        s += xpp::format(" stroke-dasharray=\"{}\"", list);
    }
    return s;
}

std::string points_of(const std::vector<std::array<double, 2>> &points, double dx = 0, double dy = 0)
{
    std::string list;
    for (const auto &p : points) list += xpp::format("{}{},{}", list.empty() ? "" : " ", num(dx + p[0]), num(dy + p[1]));
    return list;
}

void marker(Writer &out, const PictureMarker &m)
{
    const MarkerGlyph g = marker_glyph(m.pen.style.marker, m.pen.style.marker_size);
    Pen thin = m.pen; /* an outline and a stroke marker's lines are a point wide */
    thin.style.dash = LineDash::solid;
    thin.style.width = 1;
    const std::string paint_of = m.pen.hollow ? " fill=\"none\"" + stroke(thin) : paint("fill", m.pen.color);
    if (g.circle_radius > 0) out.print("<circle cx=\"{}\" cy=\"{}\" r=\"{}\"{}/>\n", num(m.x), num(m.y), num(g.circle_radius), paint_of);
    if (!g.polygon.empty()) out.print("<polygon points=\"{}\"{}/>\n", points_of(g.polygon, m.x, m.y), paint_of);
    for (const auto &line : g.strokes)
        out.print("<line x1=\"{}\" y1=\"{}\" x2=\"{}\" y2=\"{}\"{}/>\n", num(m.x + line[0][0]), num(m.y + line[0][1]), num(m.x + line[1][0]),
                  num(m.y + line[1][1]), stroke(thin));
}

void text(Writer &out, const PictureText &t)
{
    const char *anchor = t.anchor == TextAnchor::start ? "start" : t.anchor == TextAnchor::middle ? "middle" : "end";
    std::string turn;
    if (t.angle != 0) turn = xpp::format(" transform=\"rotate({} {} {})\"", num(-t.angle), num(t.x), num(t.y));
    out.print("<text x=\"{}\" y=\"{}\" text-anchor=\"{}\"{}{}>", num(t.x), num(t.y), anchor, turn, paint("fill", t.color));
    double rise = 0;
    for (const TextRun &run : t.runs) {
        /* dy moves from the last run's baseline to this one's (y grows down) */
        out.print("<tspan font-size=\"{}\" dy=\"{}\">{}</tspan>", num(run.size), num(rise - run.rise),
                  escaped(run.symbol ? symbol_to_unicode(run.text) : run.text));
        rise = run.rise;
    }
    out.print("</text>\n");
}

} // namespace

Result<> write_svg(const Picture &picture, Writer &out)
{
    out.print("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"{0}\" height=\"{1}\" viewBox=\"0 0 {0} {1}\" font-family=\"sans-serif\">\n", picture_width,
              picture_height);
    for (const PictureItem &item : picture.items) {
        if (const auto *p = std::get_if<PicturePath>(&item)) out.print("<polyline points=\"{}\" fill=\"none\"{}/>\n", points_of(p->points), stroke(p->pen));
        else if (const auto *m = std::get_if<PictureMarker>(&item)) marker(out, *m);
        else if (const auto *r = std::get_if<PictureRect>(&item))
            out.print("<rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\"{}/>\n", num(r->x), num(r->y), num(r->w), num(r->h), paint("fill", r->color));
        else if (const auto *t = std::get_if<PictureText>(&item)) text(out, *t);
        else out.write(std::get<PictureGroup>(item).begin ? "<g>\n" : "</g>\n");
    }
    out.print("</svg>\n");
    return {};
}

} // namespace xpp
