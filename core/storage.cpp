
#include "storage.h"
#include "xpp_ui.h"
#include "session.h"
#include "xpp_mem.h" /* xpp::out_of_memory */
#include <stdlib.h> 
#include <stdio.h>
#include <algorithm>
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
  added_.assign(ncol+ADDED_COLUMNS,{});
  table_.assign(ncol+ADDED_COLUMNS,nullptr);
  if(current.size()<static_cast<std::size_t>(ncol))current.resize(ncol);
  pool_=ZeroedFloats(xpp::size_product(nrow,ncol));
  pool_cols_=ncol;
  pool_rows_=nrow;
  for(int c=0;c<ncol;c++)table_[c]=pool_.data()+xpp::size_product(c,nrow);
  col=table_.data();
}

xpp::Result<> DataStore::grow(int ncol, int nrow)
{
  try {
    /* The general allocator hook was retired at W48 (xpp_mem.h).
       Exercise the recoverable storage-full path deterministically. */
    if(std::getenv("XPP_STORAGE_FAIL_GROW"))throw std::bad_alloc();
    ZeroedFloats grown(xpp::size_product(nrow,ncol));
    const std::size_t keep=static_cast<std::size_t>(std::min(nrow,pool_rows_));
    for(int c=0;c<std::min(ncol,pool_cols_);c++)
      std::copy_n(table_[c],keep,grown.data()+xpp::size_product(c,nrow));
    pool_=std::move(grown);
    pool_cols_=ncol;
    pool_rows_=nrow;
    for(int c=0;c<ncol;c++)table_[c]=pool_.data()+xpp::size_product(c,nrow);
  } catch (const std::bad_alloc &) {
    return xpp::fail("storage","Cannot allocate sufficient storage",xpp::command_place());
  }
  return {};
}

void DataStore::thin(int ncol)
{
  const int kept=(rows+1)/2;
  for(int c=0;c<ncol;c++)
    for(int r=1;r<kept;r++)table_[c][r]=table_[c][2*r];
  rows=kept;
}

void DataStore::add_column(int c)
{
  added_[c].assign(max_rows,0.0f);
  table_[c]=added_[c].data();
}

Result<> DataStore::remove_column(int c, int count)
{
  if(c<pool_cols_||c>=count||count>static_cast<int>(added_.size()))
    return xpp::fail("storage",xpp::format("Cannot remove column {} of {}",c,count),xpp::command_place());
  for(int j=c;j<count-1;j++){
    added_[j].swap(added_[j+1]);
    table_[j]=added_[j].data();
  }
  std::vector<float>().swap(added_[count-1]);
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
  table_.assign(std::max(n,last+1),nullptr);
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
  table_.clear();
  last_=-1;
}

} // namespace xpp
