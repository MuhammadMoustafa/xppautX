
#include "storage.h"
#include "xpp_ui.h"
#include "session.h"
#include "xpp_mem.h" /* xpp::out_of_memory */
#include <stdlib.h> 
#include <stdio.h>
#include <array>
#include <new>
#include <vector>
#include "form_ode.h"
#include "load_eqn.h"
#include "model.h"

namespace xpp {

void init_alloc_info(xpp::Session &s)
{
  SolverWork &w=s.solver_work;
  w.xpv.node=s.model().node+s.model().nmarkov;
  w.xpv.nvec=0; /* this is just for now */
  /* called again once the model's options are read: a fresh zeroed block */
  try {
    w.state.assign(w.xpv.nvec+w.xpv.node,0.0);
  } catch (const std::bad_alloc &) {
    xpp::out_of_memory("the state vector");
  }
  w.xpv.x=w.state.data();
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

xpp::Result<> DataStore::grow(int ncol, int nrow)
{
  try {
    for(int c=0;c<ncol;c++){
      columns_[c].resize(nrow,0.0f);
      table_[c]=columns_[c].data();
    }
  } catch (const std::bad_alloc &) {
    return xpp::fail("storage","Cannot allocate sufficient storage",xpp::command_place());
  }
  return {};
}

void DataStore::add_column(int c)
{
  columns_[c].assign(max_rows,0.0f);
  table_[c]=columns_[c].data();
}

Result<> DataStore::remove_column(int c, int count)
{
  if(c<0||c>=count||count>static_cast<int>(columns_.size()))
    return xpp::fail("storage",xpp::format("Cannot remove column {} of {}",c,count),xpp::command_place());
  for(int j=c;j<count-1;j++){
    columns_[j].swap(columns_[j+1]);
    table_[j]=columns_[j].data();
  }
  std::vector<float>().swap(columns_[count-1]);
  table_[count-1]=nullptr;
  return {};
}

void DataStore::lend_columns(float **dst, int from, int to) const
{
  for(int c=from;c<=to;c++)dst[c]=table_[c];
}

float **LentColumns::make(const DataStore &store, int n, int len, int last)
{
  try {
    own_.assign(n,std::vector<float>(len,0.0f));
  } catch (const std::bad_alloc &) {
    xpp::out_of_memory("a derived data set's columns");
  }
  table_.fill(nullptr);
  for(int c=0;c<n;c++)table_[c]=own_[c].data();
  last_=last;
  return view(store);
}

float **LentColumns::view(const DataStore &store)
{
  store.lend_columns(table_.data(),static_cast<int>(own_.size()),last_);
  return table_.data();
}

void LentColumns::release()
{
  own_.clear();
  table_.fill(nullptr);
  last_=-1;
}

} // namespace xpp
