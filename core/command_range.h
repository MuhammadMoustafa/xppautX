#pragma once

/* What the registry commands that run the Range sweep share (commands_run.cpp's run.range,
   commands_analysis.cpp's analysis.lyapunov_exponent): the sweep's arguments, and the Range settings they set.
   An argument left out keeps the setting the session has. */
#include "command_def.h"
#include "integrate.h"
#include <array>

namespace xpp::command {

inline constexpr std::array RUN_RANGE_ARGUMENTS = {
    Argument{"variable", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::either},
    Argument{"start", Type::real, false},
    Argument{"end", Type::real, false},
    Argument{"steps", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)},
    Argument{"reset_storage", Type::boolean, false},
    Argument{"use_old_ic", Type::boolean, false},
    Argument{"cycle_color", Type::boolean, false},
    Argument{"movie", Type::boolean, false},
    Argument{"write_each_run_to_file", Type::boolean, false}};

/* the single-parameter Range settings the arguments given set (and the sweep's type: one value) */
void set_range_settings(RangeVars &range, const Arguments &args);

} // namespace xpp::command
