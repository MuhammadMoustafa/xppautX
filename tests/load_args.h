#pragma once
/* a unit test's model start: argv as main has it, read once by
   parse_command_line, then loaded as every mode loads (xpp::load_model) */
#include "xpp_batch.h"
#include <string>
#include <vector>

inline xpp::Loaded load_args(int argc, char **argv, int batch, const xpp::SavedModel *saved = nullptr)
{
    xpp::Result<xpp::CommandLine> cl = xpp::parse_command_line(std::vector<std::string>(argv, argv + argc));
    if (!cl) return std::unexpected(cl.error());
    return xpp::load_model(std::move(*cl), batch, saved);
}
