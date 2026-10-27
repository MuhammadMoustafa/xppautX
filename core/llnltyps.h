#pragma once

// The retained CVODE solver uses double precision and int indices on every platform.
namespace xpp::cvode_solver {
typedef double real;
typedef int integer;
} // namespace xpp::cvode_solver
