#pragma once

/* What the registry commands that run the Range sweep share (commands_run.cpp's run.range,
   commands_analysis.cpp's lyapunov_exponent.compute): the sweep's arguments, and the Range settings they set.
   An argument left out keeps the setting the session has. */
#include "command_def.h"
#include "integrate.h"
#include <array>

namespace xpp::command {

inline constexpr std::array RUN_RANGE_ARGUMENTS = {
    set_by(Argument{"variable", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::either}, "variable"),
    set_by(Argument{"start", Type::real, false}, "low"),
    set_by(Argument{"end", Type::real, false}, "high"),
    set_by(Argument{"steps", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)}, "steps"),
    set_by(Argument{"reset_storage", Type::boolean, false}, "reset"),
    set_by(Argument{"use_old_ic", Type::boolean, false}, "old_ic"),
    Argument{"cycle_color", Type::boolean, false},
    Argument{"movie", Type::boolean, false},
    set_by(Argument{"write_each_run_to_file", Type::boolean, false}, "write_files")};

/* the single-parameter Range settings the arguments given set (and the sweep's type: one value) */
void set_range_settings(RangeVars &range, const Arguments &args);

} // namespace xpp::command
