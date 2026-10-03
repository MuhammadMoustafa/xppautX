/* W178: build generators share all-or-nothing output and typed input errors. */
#include "xpptest.h"
#include "xpp_files.h"
#include "../tools/embed_data.h"
#include <sstream>

int main()
{
    xpp::TempDir scratch;
    CHECK(!scratch.path().empty());
    if (scratch.path().empty()) TEST_REPORT("test_embed");
    const auto target = scratch.file("asset.cpp");
    const auto missing = scratch.file("missing.js");
    {
        xpp::embed::Output output(target.c_str());
        output.stream() << "original";
        output.commit();
    }
    const auto original_size = std::filesystem::file_size(target);
    try {
        xpp::embed::Output output(target.c_str());
        xpp::embed::write_bytes(output.stream(), "asset", missing.c_str());
        CHECK(false);
    } catch (const xpp::embed::InputError &error) {
        CHECK(std::string(error.what()).starts_with(missing + ":1:"));
    }
    CHECK(std::filesystem::file_size(target) == original_size);
    std::filesystem::remove(target);
    try {
        xpp::embed::Output output(target.c_str());
        xpp::embed::write_bytes(output.stream(), "asset", missing.c_str());
        CHECK(false);
    } catch (const xpp::embed::InputError &) {}
    CHECK(!std::filesystem::exists(target));
    CHECK(std::filesystem::is_empty(scratch.path()));
    {
        xpp::embed::Output output(target.c_str());
        output.stream() << "replacement";
        output.commit();
    }
    {
        xpp::embed::Output output(target.c_str());
        output.stream() << "replacement again";
        output.commit();
    }
    CHECK(std::filesystem::file_size(target) != original_size);
    // Exercise the tools' actual error renderer with output failing mid-input.
    const auto input = scratch.file("input.js");
    std::filesystem::copy_file(target, input);
    const auto complete_size = std::filesystem::file_size(target);
    std::ostringstream diagnostic;
    auto *previous = std::cerr.rdbuf(diagnostic.rdbuf());
    const auto status = xpp::embed::generate(target.c_str(), "test_embed", [&](std::ostream &out) {
        try { out.setstate(std::ios::badbit); } catch (const std::ios_base::failure &) {}
        xpp::embed::write_bytes(out, "asset", input.c_str());
    });
    std::cerr.rdbuf(previous);
    CHECK(status == 1);
    CHECK(diagnostic.str().starts_with(target + ":1: test_embed:"));
    CHECK(diagnostic.str().find(input) == std::string::npos);
    CHECK(std::filesystem::file_size(target) == complete_size);
    // A failed output during byte conversion must stay an output exception.
    std::ostringstream broken;
    broken.setstate(std::ios::badbit);
    try {
        broken.exceptions(std::ios::failbit | std::ios::badbit);
    } catch (const std::ios_base::failure &) {}
    try {
        xpp::embed::write_bytes(broken, "asset", target.c_str());
        CHECK(false);
    } catch (const std::ios_base::failure &) {
        CHECK(true);
    }
    // Directory reads can fail after open on POSIX: those are input errors.
    try {
        std::ostringstream out;
        xpp::embed::write_bytes(out, "asset", scratch.path().c_str());
        CHECK(false);
    } catch (const xpp::embed::InputError &error) {
        CHECK(std::string(error.what()).starts_with(scratch.path() + ":1:"));
    }
    TEST_REPORT("test_embed");
}
