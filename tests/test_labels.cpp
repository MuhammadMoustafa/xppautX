/* The text labels of a plot (grobs.h, W227): what a label may hold, and
   adding, changing and deleting one in a Session's slots. */
#include "xpptest.h"
#include "colormap.h"
#include "grobs.h"
#include "model.h"
#include "session.h"

#include <memory>
#include <string>

namespace {

void check_problems()
{
    using namespace xpp;
    CHECK(label_problem(LabelLook{"a", 0, 0, 0}).empty());
    CHECK(label_problem(LabelLook{"a", LABEL_SIZE_MAX, LABEL_STYLE_COUNT - 1, LAST_PLOT_COLOR}).empty());
    CHECK(label_problem(LabelLook{std::string(LABEL_TEXT_MAX, 'x'), 2, 0, 0}).empty());
    CHECK(!label_problem(LabelLook{std::string(LABEL_TEXT_MAX + 1, 'x'), 2, 0, 0}).empty());
    CHECK(!label_problem(LabelLook{"", 2, 0, 0}).empty());
    CHECK(!label_problem(LabelLook{"a", -1, 0, 0}).empty());
    CHECK(!label_problem(LabelLook{"a", LABEL_SIZE_MAX + 1, 0, 0}).empty());
    CHECK(!label_problem(LabelLook{"a", 2, -1, 0}).empty());
    CHECK(!label_problem(LabelLook{"a", 2, LABEL_STYLE_COUNT, 0}).empty());
    CHECK(!label_problem(LabelLook{"a", 2, 0, -1}).empty());
    CHECK(!label_problem(LabelLook{"a", 2, 0, LAST_PLOT_COLOR + 1}).empty());
    CHECK_STR(label_size_problem(9).c_str(), "9 is not a text size (0 to 4)");
}

void check_slots()
{
    const auto model = std::make_unique<xpp::Model>(); /* both are big: not on the stack */
    const auto s = std::make_unique<xpp::Session>(*model);
    const int a = xpp::add_label(*s, xpp::LabelLook{"alpha", 3, 1, 5}, 77, 0.5f, -0.5f);
    CHECK(a == 0 && s->labels[0].use == 1 && s->labels[0].w == 77 && s->labels[0].s == "alpha");
    CHECK(s->labels[0].size == 3 && s->labels[0].style == 1 && s->labels[0].color == 5 && s->labels[0].x == 0.5f);
    const int b = xpp::add_label(*s, xpp::LabelLook{"beta", 2, 0, 0}, 77, 1, 1);
    CHECK(b == 1);
    xpp::change_label(*s, a, xpp::LabelLook{"gamma", 0, 3, 9}, false, 9, 9);
    CHECK(s->labels[0].s == "gamma" && s->labels[0].size == 0 && s->labels[0].style == 3 && s->labels[0].color == 9);
    CHECK(s->labels[0].x == 0.5f && s->labels[0].y == -0.5f); /* not moved */
    xpp::change_label(*s, a, xpp::LabelLook{"gamma", 0, 3, 9}, true, 9, 8);
    CHECK(s->labels[0].x == 9 && s->labels[0].y == 8);
    xpp::delete_label(*s, a);
    CHECK(s->labels[0].use == 0 && s->labels[0].w == 0 && s->labels[1].use == 1);
    CHECK(xpp::add_label(*s, xpp::LabelLook{"again", 2, 0, 0}, 77, 0, 0) == 0); /* the freed slot */
    for (int i = 2; i < MAXLAB; i++) CHECK(xpp::add_label(*s, xpp::LabelLook{"x", 2, 0, 0}, 77, 0, 0) == i);
    CHECK(xpp::add_label(*s, xpp::LabelLook{"full", 2, 0, 0}, 77, 0, 0) == -1);
}

} // namespace

int main(void)
{
    check_problems();
    check_slots();
    TEST_REPORT("test_labels");
}
