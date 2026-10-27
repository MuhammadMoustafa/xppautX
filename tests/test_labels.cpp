/* The text labels of a plot (grobs.h, W227): what a label may hold, and
   adding, changing and deleting annotations in a Session by id. */
#include "xpptest.h"
#include "colormap.h"
#include "grobs.h"
#include "model.h"
#include "session.h"

#include <algorithm>
#include <memory>
#include <string>

namespace {

void check_problems()
{
    using namespace xpp;
    CHECK(label_problem(LabelLook{"a", 0}).empty());
    CHECK(label_problem(LabelLook{"a", LABEL_SIZE_MAX, LABEL_STYLE_COUNT - 1}).empty());
    CHECK(label_problem(LabelLook{std::string(LABEL_TEXT_MAX, 'x'), 2, 0}).empty());
    CHECK(!label_problem(LabelLook{std::string(LABEL_TEXT_MAX + 1, 'x'), 2, 0}).empty());
    CHECK(!label_problem(LabelLook{"", 2, 0}).empty());
    CHECK(!label_problem(LabelLook{"a", -1, 0}).empty());
    CHECK(!label_problem(LabelLook{"a", LABEL_SIZE_MAX + 1, 0}).empty());
    CHECK(!label_problem(LabelLook{"a", 2, -1}).empty());
    CHECK(!label_problem(LabelLook{"a", 2, LABEL_STYLE_COUNT}).empty());
    CHECK_STR(label_size_problem(9).c_str(), "9 is not a text size (0 to 4)");
}

void check_ids()
{
    const auto model = std::make_unique<xpp::Model>(); /* both are big: not on the stack */
    const auto s = std::make_unique<xpp::Session>(*model);
    const int a = xpp::add_label(*s, xpp::LabelLook{"alpha", 3, 1, xpp::xppaut_color(5)}, 77, 0.5f, -0.5f);
    const LABEL *l = xpp::find_annotation(s->labels, a);
    CHECK(a == 0 && l && l->w == 77 && l->s == "alpha");
    CHECK(l->size == 3 && l->style == 1 && l->color == xpp::xppaut_color(5) && l->x == 0.5f);
    const int b = xpp::add_label(*s, xpp::LabelLook{"beta", 2, 0}, 77, 1, 1);
    const int c = xpp::add_grob(*s, 77, 1, 2, 3, 4, .1, xpp::ARROW, xpp::foreground_color);
    CHECK(b == 1 && c == 2); /* one counter for texts and objects */
    CHECK(xpp::edit_label(*s, a, xpp::LabelLook{"gamma", 0, 3, xpp::xppaut_color(9)}));
    l = xpp::find_annotation(s->labels, a);
    CHECK(l->s == "gamma" && l->size == 0 && l->style == 3 && l->color == xpp::xppaut_color(9));
    CHECK(l->x == 0.5f && l->y == -0.5f); /* not moved */
    CHECK(xpp::move_annotation(*s, a, 9, 8));
    CHECK(l->x == 9 && l->y == 8);
    CHECK(xpp::move_annotation(*s, c, 10, 10));
    const xpp::GROB *g = xpp::find_annotation(s->grobs, c);
    CHECK(g->xs == 10 && g->xe == 12 && g->ye == 12); /* the end follows the start */
    CHECK(xpp::delete_annotation(*s, a) && !xpp::delete_annotation(*s, a));
    CHECK(!xpp::find_annotation(s->labels, a) && xpp::find_annotation(s->labels, b));
    CHECK(xpp::add_label(*s, xpp::LabelLook{"again", 2, 0}, 77, 0, 0) == 3); /* an id is not reused */
    for (std::size_t i = 0; i < 600; i++) CHECK(xpp::add_label(*s, xpp::LabelLook{"x", 2, 0}, 77, 0, 0) >= 0); /* no 50 limit */
    xpp::destroy_labels_and_grobs(*s, 77);
    CHECK(s->labels.empty() && s->grobs.empty());
}

/* every marker's glyph is centred on its position, the one the pictures and the page share */
void check_glyphs()
{
    for (int shape = 0; shape < xpp::MARKER_SHAPE_COUNT; shape++) {
        double lo[2] = {1e9, 1e9}, hi[2] = {-1e9, -1e9};
        for (const auto &stroke : xpp::marker_strokes(shape))
            for (const auto &p : stroke)
                for (int axis = 0; axis < 2; axis++) {
                    lo[axis] = std::min(lo[axis], p[static_cast<std::size_t>(axis)]);
                    hi[axis] = std::max(hi[axis], p[static_cast<std::size_t>(axis)]);
                }
        CHECK(lo[0] == -hi[0] && hi[0] <= 2 * xpp::MARKER_HALF_UNITS);
        CHECK(lo[1] == -hi[1]);
    }
    CHECK(xpp::marker_strokes(1)[0][0][0] == 8 && xpp::marker_strokes(1)[0].size() == 5); /* the diamond */
}

} // namespace

int main(void)
{
    check_problems();
    check_ids();
    check_glyphs();
    TEST_REPORT("test_labels");
}
