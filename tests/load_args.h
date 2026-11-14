#pragma once
/* a unit test's model start: argv as main has it, read once by
   parse_command_line, then loaded as every mode loads (xpp::load_model);
   the model is compiled as a computation's start would (W713), since a test
   reads the compiled code without one */
#include "expr_native.h"
#include "xpp_batch.h"
#include "comline.h"
#include "xpp_error.h"
#include <string>
#include <vector>

/* translate_model then build_native, for a test that reads the code at once (W715: a computation builds it when due) */
inline xpp::Result<std::vector<xpp::Error>> compile_model(xpp::Session &s)
{
    auto translated = xpp::translate_model(s);
    xpp::build_native(s, true);
    return translated;
}

inline xpp::Loaded load_args(int argc, char **argv, int batch, const xpp::SavedModel *saved = nullptr)
{
    xpp::Result<xpp::CommandLine> cl = xpp::parse_command_line(std::vector<std::string>(argv, argv + argc));
    if (!cl) return std::unexpected(cl.error());
    xpp::Loaded loaded = xpp::load_model(std::move(*cl), batch, saved);
    if (loaded && *loaded) xpp::ensure_compiled(**loaded);
    return loaded;
}
