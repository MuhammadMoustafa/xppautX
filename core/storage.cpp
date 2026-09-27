
#include "storage.h"
#include "session.h"
#include "xpp_ui.h"
#include "xpp_mem.h" /* xpp_out_of_memory */
#include <stdlib.h> 
#include <stdio.h>
#include <array>
#include <new>
#include <vector>
#include "form_ode.h"
#include "load_eqn.h"
#include "model.h"

#define BACKEUL 7
#define VOLTERRA 6
#define STIFF 9
#define GEAR 5
#define RB23 13

void init_alloc_info()
{
  SolverWork &w=xpp::session().solver_work;
  w.xpv.node=xpp::model().node+xpp::model().nmarkov;
  w.xpv.nvec=0; /* this is just for now */
  /* called again once the model's options are read: a fresh zeroed block */
  try {
    w.state.assign(w.xpv.nvec+w.xpv.node,0.0);
  } catch (const std::bad_alloc &) {
    xpp_out_of_memory("the state vector");
  }
  w.xpv.x=w.state.data();
}

void alloc_meth()
{
  SolverWork &w=xpp::session().solver_work;
  int nn=w.xpv.node+w.xpv.nvec;
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
    w.work.assign(sz,0.0);
  } catch (const std::bad_alloc &) {
    xpp_out_of_memory("the solver's work space");
  }
}

void DataStore::allocate(int nrow, int ncol)
{
  max_rows=nrow;
  rows=0;
  columns_.assign(MAXODE+1,{});
  table_.fill(nullptr);
  for(int c=0;c<ncol;c++){
    columns_[c].assign(nrow,0.0f);
    table_[c]=columns_[c].data();
  }
  col=table_.data();
}

bool DataStore::grow(int ncol, int nrow)
{
  try {
    for(int c=0;c<ncol;c++){
      columns_[c].resize(nrow,0.0f);
      table_[c]=columns_[c].data();
    }
  } catch (const std::bad_alloc &) {
    err_msg("Cannot allocate sufficient storage");
    return false;
  }
  return true;
}

void DataStore::add_column(int c)
{
  columns_[c].assign(max_rows,0.0f);
  table_[c]=columns_[c].data();
}

void DataStore::lend_columns(float **dst, int from, int to) const
{
  for(int c=from;c<=to;c++)dst[c]=table_[c];
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
  xpp::session().data_store.lend_columns(table_.data(),n,last);
  return table_.data();
}

void LentColumns::release()
{
  own_.clear();
  table_.fill(nullptr);
}
