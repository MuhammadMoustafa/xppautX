/* The PDF writer (W479, #542): a Picture as a one-page PDF, 640 x 400 points. Lines, polygons and dots
   only; the text in Helvetica and Symbol, two of the standard 14 fonts, which every reader has, so
   nothing is embedded; a colour's alpha through one ExtGState per alpha. The file has no date and no
   identifier: the same picture is the same bytes (tools/goldencheck.py). */
#include "my_pdf.h"

#include <cmath>
#include <map>
#include <numbers>
#include <string>

#include "xpp_io.h"
#include "xpp_math.h"

namespace xpp {

namespace {

/* the width of each of the characters 32 to 126 in 1/1000 of the size, from Adobe's AFM files of Helvetica and
   Symbol (Copyright (c) 1985, 1987, 1989, 1990 Adobe Systems Incorporated; the metrics the standard fonts have), with
   the quote as WinAnsi's straight one */
constexpr std::array<int, 95> helvetica_widths{
    278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278, 556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 278, 278, 584, 584, 584,
    556, 1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778, 667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278,
    469, 556, 333, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556, 556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334, 260,
    334, 584};
constexpr std::array<int, 95> symbol_widths{
    250, 333, 713, 500, 549, 833, 778, 439, 333, 333, 500, 549, 250, 549, 250, 278, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 278, 278, 549, 549, 549,
    444, 549, 722, 667, 722, 612, 611, 763, 603, 722, 333, 631, 722, 686, 889, 722, 722, 768, 741, 556, 592, 611, 690, 439, 768, 645, 795, 611, 333, 863, 333,
    658, 500, 500, 631, 549, 549, 494, 439, 521, 411, 603, 329, 603, 549, 549, 576, 521, 549, 549, 521, 549, 603, 439, 576, 713, 686, 493, 686, 494, 480, 200,
    480, 549};
constexpr int unknown_width = 556; /* a character outside 32 to 126: a digit's width, as the "?" drawn in its place */

/* the bytes of text in the font's own encoding: ASCII as it is, Latin-1's range (U+00A0 to U+00FF) as WinAnsi's same
   bytes, any other character a "?" (a symbol run's letters are the font's own, so only ASCII) */
std::string encode(std::string_view utf8, bool symbol)
{
    std::string out;
    for (std::size_t i = 0; i < utf8.size(); i++) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        if (c < 0x80) out += static_cast<char>(c);
        else if (!symbol && (c == 0xC2 || c == 0xC3) && i + 1 < utf8.size()) {
            out += static_cast<char>(((c & 0x03) << 6) | (static_cast<unsigned char>(utf8[i + 1]) & 0x3F));
            i++;
        } else {
            out += '?';
            while (i + 1 < utf8.size() && (static_cast<unsigned char>(utf8[i + 1]) & 0xC0) == 0x80) i++;
        }
    }
    return out;
}

double advance(const TextRun &run)
{
    double width = 0;
    for (const unsigned char c : encode(run.text, run.symbol)) {
        const auto &table = run.symbol ? symbol_widths : helvetica_widths;
        width += c >= 32 && c <= 126 ? table[c - 32] : unknown_width;
    }
    return width * run.size / 1000;
}

/* a number as short as it can be at 1/100 of a point (a colour's part at 1/1000: it reads back as the same 8 bits) */
std::string num(double v, int digits = 2)
{
    std::string s = xpp::format("{:.{}f}", v, digits);
    if (s.find('.') != std::string::npos) {
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    return s == "-0" ? "0" : s;
}

std::string string_literal(const std::string &bytes)
{
    std::string s = "(";
    for (const char c : bytes) {
        if (c == '(' || c == ')' || c == '\\') s += '\\';
        s += c;
    }
    return s + ')';
}

std::string rgb(const char *op, const Color &c)
{
    constexpr int part_digits = 3;
    return xpp::format("{} {} {} {}\n", num(c.r / 255.0, part_digits), num(c.g / 255.0, part_digits), num(c.b / 255.0, part_digits), op);
}

/* the colour as its three 8 bit parts (its alpha is the ExtGState's) */
using Parts = std::array<std::uint8_t, 3>;
Parts parts(const Color &c) { return {c.r, c.g, c.b}; }

/* the content stream: each item as the PDF operators that draw it, with its alpha set before */
class Content {
public:
    std::string text;
    std::map<std::uint8_t, int> alphas; /* the alphas used, each the number of its ExtGState */

    /* the graphics state is written when it changes only: the fill colour (`rg`) or the stroke colour (`RG`) */
    void set_color(bool fill, const Color &c)
    {
        Parts &now = fill ? fill_ : stroke_;
        if (now != parts(c)) text += rgb(fill ? "rg" : "RG", c);
        now = parts(c);
    }
    void set_fill(const Color &c) { set_color(true, c); }
    void set_stroke(const Color &c) { set_color(false, c); }

    void set_alpha(std::uint8_t alpha)
    {
        if (alpha == current_) return;
        const auto [it, inserted] = alphas.try_emplace(alpha, static_cast<int>(alphas.size()));
        text += xpp::format("/G{} gs\n", it->second);
        current_ = alpha;
    }

    void path(const PicturePath &p)
    {
        set_alpha(p.pen.color.a);
        stroke_style(p.pen);
        for (std::size_t i = 0; i < p.points.size(); i++)
            text += xpp::format("{} {} {}\n", num(p.points[i][0]), num(flip(p.points[i][1])), i ? "l" : "m");
        text += "S\n";
    }

    void marker(const PictureMarker &m)
    {
        const MarkerGlyph g = marker_glyph(m.pen.style.marker, m.pen.style.marker_size);
        set_alpha(m.pen.color.a);
        Pen thin = m.pen; /* an outline and a stroke marker's lines are a point wide */
        thin.style.dash = LineDash::solid;
        thin.style.width = 1;
        if (m.pen.hollow) stroke_style(thin);
        else set_fill(m.pen.color);
        const char *paint = m.pen.hollow ? "S" : "f";
        const double y = flip(m.y);
        if (g.circle_radius > 0) {
            circle(m.x, y, g.circle_radius);
            text += xpp::format("{}\n", paint);
        }
        if (!g.polygon.empty()) {
            for (std::size_t i = 0; i < g.polygon.size(); i++)
                text += xpp::format("{} {} {}\n", num(m.x + g.polygon[i][0]), num(y - g.polygon[i][1]), i ? "l" : "m");
            text += xpp::format("{}{}\n", m.pen.hollow ? "h " : "", paint);
        }
        if (!g.strokes.empty()) {
            stroke_style(thin);
            for (const auto &stroke : g.strokes)
                text += xpp::format("{} {} m {} {} l S\n", num(m.x + stroke[0][0]), num(y - stroke[0][1]), num(m.x + stroke[1][0]), num(y - stroke[1][1]));
        }
    }

    void rect(const PictureRect &r)
    {
        set_alpha(r.color.a);
        set_fill(r.color);
        text += xpp::format("{} {} {} {} re f\n", num(r.x), num(flip(r.y + r.h)), num(r.w), num(r.h));
    }

    void put_text(const PictureText &t)
    {
        set_alpha(t.color.a);
        set_fill(t.color);
        double total = 0;
        for (const TextRun &run : t.runs) total += advance(run);
        const double theta = t.angle * std::numbers::pi / 180, c = xpp::math::cos(theta), s = xpp::math::sin(theta);
        double along = t.anchor == TextAnchor::start ? 0 : t.anchor == TextAnchor::middle ? -total / 2 : -total;
        const double x = t.x, y = flip(t.y);
        text += "BT\n";
        for (const TextRun &run : t.runs) {
            /* the run's start: along the baseline from the anchor, then up by the rise */
            const double px = x + along * c - run.rise * s, py = y + along * s + run.rise * c;
            text += xpp::format("/{} {} Tf {} {} {} {} {} {} Tm {} Tj\n", run.symbol ? "F2" : "F1", num(run.size), num(c), num(s), num(-s), num(c), num(px),
                           num(py), string_literal(encode(run.text, run.symbol)));
            along += advance(run);
        }
        text += "ET\n";
    }

private:
    std::uint8_t current_ = 255;
    /* the colours the page starts with: black, so a black item writes none */
    Parts fill_{0, 0, 0}, stroke_{0, 0, 0};
    double width_ = 1; /* the PDF default line width */
    std::string dash_ = "[]"; /* and dash (a solid line) */

    static double flip(double y) { return picture_height - y; }

    void circle(double x, double y, double r)
    {
        constexpr double k = .5522847498; /* the Bezier control distance of a quarter circle, in radii */
        text += xpp::format("{} {} m\n", num(x + r), num(y));
        text += xpp::format("{} {} {} {} {} {} c\n", num(x + r), num(y + k * r), num(x + k * r), num(y + r), num(x), num(y + r));
        text += xpp::format("{} {} {} {} {} {} c\n", num(x - k * r), num(y + r), num(x - r), num(y + k * r), num(x - r), num(y));
        text += xpp::format("{} {} {} {} {} {} c\n", num(x - r), num(y - k * r), num(x - k * r), num(y - r), num(x), num(y - r));
        text += xpp::format("{} {} {} {} {} {} c\n", num(x + k * r), num(y - r), num(x + r), num(y - k * r), num(x + r), num(y));
    }

    void stroke_style(const Pen &pen)
    {
        set_stroke(pen.color);
        if (pen.style.width != width_) text += xpp::format("{} w\n", num(pen.style.width));
        width_ = pen.style.width;
        std::string pattern;
        for (const double d : dash_pattern(pen.style.dash, pen.style.width)) pattern += (pattern.empty() ? "" : " ") + num(d);
        pattern = "[" + pattern + "]";
        if (pattern != dash_) text += xpp::format("{} 0 d\n", pattern);
        dash_ = pattern;
    }
};

} // namespace

Result<> write_pdf(const Picture &picture, Writer &out)
{
    Content content;
    for (const PictureItem &item : picture.items) {
        if (const auto *l = std::get_if<PicturePath>(&item)) content.path(*l);
        else if (const auto *m = std::get_if<PictureMarker>(&item)) content.marker(*m);
        else if (const auto *r = std::get_if<PictureRect>(&item)) content.rect(*r);
        else if (const auto *t = std::get_if<PictureText>(&item)) content.put_text(*t);
    }
    std::string states;
    for (const auto &[alpha, number] : content.alphas)
        states += xpp::format(" /G{0} << /CA {1} /ca {1} >>", number, num(alpha / 255.0, 3));

    /* the objects, each as the file's bytes so far lets its offset be found */
    std::vector<std::string> objects;
    objects.push_back("<< /Type /Catalog /Pages 2 0 R >>");
    objects.push_back("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    objects.push_back(xpp::format("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {} {}] /Contents 4 0 R /Resources << /Font << /F1 5 0 R /F2 6 0 R >> "
                             "/ExtGState <<{} >> >> >>",
                             picture_width, picture_height, states));
    objects.push_back(xpp::format("<< /Length {} >>\nstream\n{}endstream", content.text.size(), content.text));
    objects.push_back("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
    objects.push_back("<< /Type /Font /Subtype /Type1 /BaseFont /Symbol >>");

    std::string file = "%PDF-1.4\n";
    std::vector<std::size_t> offsets;
    for (std::size_t i = 0; i < objects.size(); i++) {
        offsets.push_back(file.size());
        file += xpp::format("{} 0 obj\n{}\nendobj\n", i + 1, objects[i]);
    }
    const std::size_t xref = file.size();
    file += xpp::format("xref\n0 {}\n0000000000 65535 f \n", objects.size() + 1);
    for (const std::size_t offset : offsets) file += xpp::format("{:010} 00000 n \n", offset);
    file += xpp::format("trailer\n<< /Size {} /Root 1 0 R >>\nstartxref\n{}\n%%EOF\n", objects.size() + 1, xref);
    out.write(file);
    return {};
}

} // namespace xpp
