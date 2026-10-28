/* W520: retained .ani imports compile to native grammar and reproduce their
   frame data at several states, including trail history and random colours. */
#include "xpptest.h"
#include "aniparse.h"
#include "ani_data.h"
#include "session.h"
#include "comline.h"
#include "xpp_batch.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "my_rhs.h"
#include <utility>

namespace {
xpp::Loaded open_model(const std::string &model)
{
    auto line = xpp::parse_command_line({"test_anix", model, "--convert"});
    CHECK(line.has_value());
    if (!line) return std::unexpected(line.error());
    return xpp::load_model(std::move(*line), 1);
}
}

int main()
{
    xpp::ani_data_init([](std::string_view) {});
    xpp::LineReader cases("tests/animations/imports.tsv");
    CHECK(cases);
    int models = 0;
    while (auto row = cases.next()) {
        if (row->empty() || row->front() == '#') continue;
        const auto tab = row->find('\t');
        CHECK(tab != std::string_view::npos);
        if (tab == std::string_view::npos) continue;
        const std::string input(row->substr(0, tab)), model(row->substr(tab + 1));
        auto loaded = open_model(model);
        CHECK(loaded.has_value());
        if (!loaded) continue;
        auto &s = **loaded;
        std::string source;
        CHECK(xpp::read_bytes(input, source));
        auto original = xpp::read_animation(s, source, input);
        auto converted = xpp::convert_animation(s, source, input);
        if (!converted) xpp::log(XPP_LOG_WARN, "{}\n", converted.error().text());
        CHECK(original.has_value() && converted.has_value());
        if (!original || !converted) continue;
        auto native = xpp::read_animation(s, *converted, "converted.anix");
        CHECK(native.has_value());
        if (!native) continue;
        CHECK(original->grabs.size() == native->grabs.size());
        /* Permanent coordinates evaluate once at the same initial state. */
        const auto random = s.random;
        s.animation = std::move(*original);
        xpp::set_ani_perm(s);
        auto legacy_state = std::move(s.animation);
        s.random = random;
        s.animation = std::move(*native);
        xpp::set_ani_perm(s);
        auto native_state = std::move(s.animation);
        auto y = s.last_ic;
        for (int frame = 0; frame < 5; ++frame) {
            for (std::size_t i = 0; i < y.size(); ++i) y[i] = s.last_ic[i] + frame * .01;
            set_fix_rhs(s, frame * .1, y.data());
            const auto before = s.random;
            s.animation = std::move(legacy_state);
            xpp::render_ani(s);
            const auto expected = s.ani_shown.frame;
            legacy_state = std::move(s.animation);
            s.random = before;
            s.animation = std::move(native_state);
            xpp::render_ani(s);
            CHECK(s.ani_shown.frame == expected);
            if (s.ani_shown.frame != expected) xpp::log(XPP_LOG_WARN, "{}: frame {} differs\n", input, frame);
            native_state = std::move(s.animation);
        }
        ++models;
    }
    CHECK(models == 7);
    xpp::LineReader examples("tests/animations/examples.tsv");
    CHECK(examples);
    int native_examples = 0;
    while (auto row = examples.next()) {
        if (row->empty() || row->front() == '#') continue;
        const auto tab = row->find('\t');
        CHECK(tab != std::string_view::npos);
        if (tab == std::string_view::npos) continue;
        const std::string animation(row->substr(0, tab)), model(row->substr(tab + 1));
        auto loaded = open_model(model);
        CHECK(loaded.has_value());
        if (!loaded) continue;
        std::string text;
        CHECK(xpp::read_bytes(animation, text));
        const auto native = xpp::read_animation(**loaded, text, animation);
        CHECK(native.has_value());
        if (!native) xpp::log(XPP_LOG_WARN, "{}\n", native.error().text());
        ++native_examples;
    }
    CHECK(native_examples == 32);
    TEST_REPORT("anix conversion");
}
