#pragma once
/* The data table a Session holds (browse_data.cpp): the data set it shows, the columns Add column made, the block a client shows. Plain
   data, which the option table reads (a column named in a setting is a variable's, the time or an added column) and the data table works
   on (W744). */
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include "storage.h"

namespace xpp {

struct Model; /* model.h */

typedef struct {
    int dataflag;
    int row0;
    int maxrow,maxcol;
    ColumnTable data;
    int istart,iend;
} DATA_VIEW;

/* an Add column (browse_data.cpp data_add_col): its name and formula, as
   typed, kept to recompute it after every fresh run (below) */
struct AddedColumn {
  std::string name;
  std::string formula;
};

/* the block of the table a client shows: its first row, rows (0: none, no block is sent), first column and
   columns after T, which is always the first value of a row (data.page sets it, the `data.updated` event sends it) */
struct DataBlock {
  int from = 0, count = 0, col = 1, ncol = 1;
};
/* the data table (browse_data.cpp), a Session's (session.h) */
struct DataState {
  /* the data set it shows: the stored data, or a derived set in its place */
  DATA_VIEW view{};
  /* Replace's column as it was, for Unreplace: whether there is one,
     its column and its values */
  int replaced=0,replaced_col=0;
  std::vector<double> old_column; /* the replaced column as it was, exactly at either precision */
  /* data_add_col's columns, in the order added, at data_store columns
     Model::neq+1, +2, ...: the Session's own data, not the
     Model's (docs/roadmap.md W77 -- the Model stays as the load left
     it). A fresh load clears this with the rest of the Session; a fresh
     run recomputes every one of them over its own rows
     (recompute_added_columns, refresh_data) instead of dropping
     them, since docs/manual/07-data-table.md promises an added
     column stays computed "as though ... another auxiliary variable". */
  std::vector<AddedColumn> added_columns;
  DataBlock block;
};

/* the data column of `name` (case ignored): 0 for T, i+1 for the model's variable i, then the added columns in order, -1 for none */
int data_column(const Model &m, std::span<const AddedColumn> added, std::string_view name);
/* column j's name: "T", a model variable's, an added column's; "" past the last one */
std::string data_column_name(const Model &m, std::span<const AddedColumn> added, int j);

} // namespace xpp
