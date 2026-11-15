#pragma once
/* The columns of the data table a name can mean: the time, a variable of the model, a column Add column made (W757: the option table's
   rows name columns, so this is the model's, and the data table (data_state.h, the analyses') keeps the list). */
#include <span>
#include <string>
#include <string_view>

namespace xpp {

struct Model; /* model.h */

/* an Add column (browse_data.cpp data_add_col): its name and formula, as
   typed, kept to recompute it after every fresh run (below) */
struct AddedColumn {
  std::string name;
  std::string formula;
};

/* the data column of `name` (case ignored): 0 for T, i+1 for the model's variable i, then the added columns in order, -1 for none */
int data_column(const Model &m, std::span<const AddedColumn> added, std::string_view name);
/* column j's name: "T", a model variable's, an added column's; "" past the last one */
std::string data_column_name(const Model &m, std::span<const AddedColumn> added, int j);

} // namespace xpp
