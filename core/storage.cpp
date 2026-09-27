
#include "storage.h"
#include "xpp_ui.h"
#include "xpp_mem.h" /* xpp_out_of_memory */
#include <stdlib.h> 
#include <stdio.h>
#include <array>
#include <new>
#include <vector>
#include "xpplim.h"
double *WORK;
int IWORK[10000];
extern int NODE,NMarkov;
extern int METHOD;

#define BACKEUL 7
#define VOLTERRA 6
#define STIFF 9
#define GEAR 5
#define RB23 13
#define SYMPLECT 14
XPPVEC xpv;

namespace {
/* the memory behind xpv.x and WORK: the solvers read both through the
   plain pointers storage.h exports, which point into these */
std::vector<double> state_vector;
std::vector<double> work_space;
}

void init_alloc_info()
{
  xpv.node=NODE+NMarkov;
  xpv.nvec=0; /* this is just for now */
  /* called again once the model's options are read: a fresh zeroed block */
  try {
    state_vector.assign(xpv.nvec+xpv.node,0.0);
  } catch (const std::bad_alloc &) {
    xpp_out_of_memory("the state vector");
  }
  xpv.x=state_vector.data();
}

void alloc_meth()
{
  int nn=xpv.node+xpv.nvec;
  int sz=30*nn;
  switch(METHOD){
  case STIFF:
     sz=2*nn*nn+13*nn+100;

     break;
  case GEAR:
    sz=30*nn+nn*nn+100;
    break;
  case BACKEUL:
  case VOLTERRA:
    sz=10*nn+nn*nn+100;
    break;
  case RB23:
    sz=12*nn+100+nn*nn;
    break;
  }
  try {
    work_space.assign(sz,0.0);
  } catch (const std::bad_alloc &) {
    xpp_out_of_memory("the solver's work space");
  }
  WORK=work_space.data();
}

DataStore data_store;

namespace {
/* the store's memory: the columns, and the table of their addresses that
   data_store.col points at (fixed, so a pointer to it stays valid) */
std::vector<std::vector<float>> columns;
std::array<float *, MAXODE + 1> column_table{};
}

void DataStore::allocate(int nrow, int ncol)
{
  max_rows=nrow;
  rows=0;
  columns.assign(MAXODE+1,{});
  column_table.fill(nullptr);
  for(int c=0;c<ncol;c++){
    columns[c].assign(nrow,0.0f);
    column_table[c]=columns[c].data();
  }
  col=column_table.data();
}

bool DataStore::grow(int ncol, int nrow)
{
  try {
    for(int c=0;c<ncol;c++){
      columns[c].resize(nrow,0.0f);
      column_table[c]=columns[c].data();
    }
  } catch (const std::bad_alloc &) {
    err_msg("Cannot allocate sufficient storage");
    return false;
  }
  return true;
}

void DataStore::add_column(int c)
{
  columns[c].assign(max_rows,0.0f);
  column_table[c]=columns[c].data();
}

void DataStore::lend_columns(float **dst, int from, int to) const
{
  for(int c=from;c<=to;c++)dst[c]=column_table[c];
}

float **LentColumns::make(int n, int len, int last)
{
  try {
    own_.assign(n,std::vector<float>(len,0.0f));
  } catch (const std::bad_alloc &) {
    xpp_out_of_memory("a derived data set's columns");
  }
  table_.fill(nullptr);
  for(int c=0;c<n;c++)table_[c]=own_[c].data();
  data_store.lend_columns(table_.data(),n,last);
  return table_.data();
}

void LentColumns::release()
{
  own_.clear();
  table_.fill(nullptr);
}
