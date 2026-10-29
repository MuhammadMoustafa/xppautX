/* W533: an invariant set's branch is drawn whole from the saddle, whatever store_from says:
   shoot_easy draws the unstable branch of x' = x, y' = -y (store_from 0.5, dt 0.05, length 1,
   20 steps) and the picture holds every step's segment, not only those past store_from. */
#include "xpptest.h"
#include "session.h"
#include "xpp_batch.h"
#include "comline.h"
#include "integrate.h"
#include "image_format.h"
#include "xpp_globals.h"
#include "picture.h"
#include <utility>
#include <variant>

int main()
{
    auto line = xpp::parse_command_line({"test_shoot_branch", "tools/models/saddle_manifolds_transient.odex", "--convert"});
    CHECK(line.has_value());
    if (!line) return 1;
    auto loaded = xpp::load_model(std::move(*line), 1);
    CHECK(loaded.has_value());
    if (!loaded) return 1;
    xpp::Session &s = **loaded;
    program.interactive = 1;
    s.numerics.storflag = false;
    s.numerics.delta_t = .05;
    s.numerics.total_time = 1;
    CHECK(s.numerics.store_from == .5);

    const std::string file = "test_shoot_branch.svg";
    CHECK(xpp::begin_picture(s, xpp::image_formats[xpp::IMAGE_FORMAT_SVG], file, true).has_value());
    double x[2] = {1e-4, 0};
    CHECK(xpp::shoot_easy(s, x).has_value());
    std::size_t segments = 0;
    for (const auto &item : s.plot_file.picture.items)
        if (const auto *path = std::get_if<xpp::PicturePath>(&item)) segments += path->points.size() - 1;
    CHECK(xpp::end_picture(s).has_value());
    std::remove(file.c_str());
    /* 20 steps of the branch: the whole branch is drawn, not only the 10 past store_from */
    CHECK(segments >= 20);
    TEST_REPORT("shoot branch");
}
