
#include "storage.h"
#include "xpp_ui.h"
#include "session.h"
#include "markov.h"
#include "adj2.h"
#include "histogram.h"
#include "xpp_mem.h" /* xpp::out_of_memory */
#include <stdlib.h> 
#include <stdio.h>
#include <algorithm>
#include <cstring>
#include <new>
#include <vector>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include "load_eqn.h"
#include "model.h"
#include "xpp_io.h"

namespace xpp {

void init_alloc_info(xpp::Session &s)
{
  SolverWork &w=*s.solver_work;
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

std::vector<double> Column::values(std::ptrdiff_t from, std::ptrdiff_t n) const
{
  std::vector<double> v(static_cast<std::size_t>(n));
  for(std::ptrdiff_t i=0;i<n;i++)v[static_cast<std::size_t>(i)]=get(from+i);
  return v;
}

std::vector<float> Column::as_floats(std::ptrdiff_t from, std::ptrdiff_t n) const
{
  std::vector<float> v(static_cast<std::size_t>(n));
  for(std::ptrdiff_t i=0;i<n;i++)v[static_cast<std::size_t>(i)]=static_cast<float>(get(from+i));
  return v;
}

void Column::store(std::ptrdiff_t from, std::span<const double> src) const noexcept
{
  for(std::size_t i=0;i<src.size();i++)put(from+static_cast<std::ptrdiff_t>(i),src[i]);
}

void OwnedColumn::assign(std::size_t n, StoragePrecision precision)
{
  clear();
  wide_=precision==StoragePrecision::double_;
  if(wide_)d_.assign(n,0.0);
  else f_.assign(n,0.0f);
}

void OwnedColumn::assign_copy(const Column &src, std::size_t from, std::size_t to)
{
  assign(0,src.is_double()?StoragePrecision::double_:StoragePrecision::single);
  append(src,from,to);
}

void OwnedColumn::append(const Column &src, std::size_t from, std::size_t to)
{
  for(std::size_t r=from;r<to;r++){
    const double v=src.get(static_cast<std::ptrdiff_t>(r));
    if(wide_)d_.push_back(v);
    else f_.push_back(static_cast<float>(v));
  }
}

void OwnedColumn::resize(std::size_t n)
{
  if(wide_)d_.resize(n,0.0);
  else f_.resize(n,0.0f);
}

void OwnedColumn::convert(StoragePrecision precision)
{
  const bool to_wide=precision==StoragePrecision::double_;
  if(empty()||to_wide==wide_){
    wide_=to_wide;
    return;
  }
  if(to_wide)d_.assign(f_.begin(),f_.end());
  else{
    f_.resize(d_.size());
    std::ranges::transform(d_,f_.begin(),[](double v){return static_cast<float>(v);});
  }
  if(to_wide)std::vector<float>().swap(f_);
  else std::vector<double>().swap(d_);
  wide_=to_wide;
}

void OwnedColumn::clear() noexcept
{
  f_.clear();
  d_.clear();
  f_.shrink_to_fit();
  d_.shrink_to_fit();
}

Column OwnedColumn::view() const noexcept
{
  return wide_?Column(const_cast<double *>(d_.data())):Column(const_cast<float *>(f_.data()));
}

Column DataStore::pool_column(int c) const noexcept
{
  const std::size_t at=xpp::size_product(c,pool_rows_);
  if(precision_==StoragePrecision::double_)return Column(static_cast<double *>(pool_.data())+at);
  return Column(static_cast<float *>(pool_.data())+at);
}

void DataStore::allocate(int nrow, int ncol, StoragePrecision precision)
{
  max_rows=nrow;
  rows=0;
  precision_=precision;
  added_.assign(ncol+ADDED_COLUMNS,{});
  table_.assign(ncol+ADDED_COLUMNS,Column());
  if(current.size()<static_cast<std::size_t>(ncol))current.resize(ncol);
  pool_=ZeroedBytes(xpp::size_product(nrow,ncol)*value_bytes());
  pool_cols_=ncol;
  pool_rows_=nrow;
  for(int c=0;c<ncol;c++)table_[c]=pool_column(c);
  col=ColumnTable(table_.data());
}

xpp::Result<> DataStore::set_precision(StoragePrecision precision)
{
  if(precision==precision_)return {};
  const std::size_t to_bytes=precision==StoragePrecision::double_?sizeof(double):sizeof(float);
  try {
    ZeroedBytes converted(xpp::size_product(pool_cols_,pool_rows_)*to_bytes);
    std::vector<OwnedColumn> added(added_);
    for(OwnedColumn &c:added)c.convert(precision);
    /* the new block and the new added columns are made; from here nothing can fail */
    std::vector<Column> views(table_.size());
    for(int c=0;c<pool_cols_;c++){
      const std::size_t at=xpp::size_product(c,pool_rows_);
      const Column from=table_[c];
      if(precision==StoragePrecision::double_){
        double *to=static_cast<double *>(converted.data())+at;
        for(int r=0;r<rows;r++)to[r]=from.get(r);
        views[c]=Column(to);
      }else{
        float *to=static_cast<float *>(converted.data())+at;
        for(int r=0;r<rows;r++)to[r]=static_cast<float>(from.get(r));
        views[c]=Column(to);
      }
    }
    for(std::size_t c=static_cast<std::size_t>(pool_cols_);c<added.size();c++)
      if(!added[c].empty())views[c]=added[c].view();
    pool_=std::move(converted);
    added_=std::move(added);
    std::ranges::copy(views,table_.begin());
    precision_=precision;
  } catch (const std::bad_alloc &) {
    return xpp::fail("storage","Cannot allocate sufficient storage",xpp::command_place());
  }
  return {};
}

xpp::Result<> DataStore::grow(int ncol, int nrow)
{
  try {
    /* The general allocator hook was retired at W48 (xpp_mem.h).
       Exercise the recoverable storage-full path deterministically. */
    if(std::getenv("XPP_STORAGE_FAIL_GROW"))throw std::bad_alloc();
    ZeroedBytes grown(xpp::size_product(nrow,ncol)*value_bytes());
    const std::size_t keep=static_cast<std::size_t>(std::min(nrow,pool_rows_));
    const std::size_t bytes=value_bytes();
    for(int c=0;c<std::min(ncol,pool_cols_);c++)
      std::memcpy(static_cast<std::byte *>(grown.data())+xpp::size_product(c,nrow)*bytes,
                  static_cast<std::byte *>(pool_.data())+xpp::size_product(c,pool_rows_)*bytes,keep*bytes);
    pool_=std::move(grown);
    pool_cols_=ncol;
    pool_rows_=nrow;
    for(int c=0;c<ncol;c++)table_[c]=pool_column(c);
  } catch (const std::bad_alloc &) {
    return xpp::fail("storage","Cannot allocate sufficient storage",xpp::command_place());
  }
  return {};
}

xpp::Result<> DataStore::fit(int ncol, int nrow)
{
  if(nrow<=max_rows)return {};
  if(const xpp::Result<> grown=grow(ncol,nrow);!grown)return grown;
  try {
    for(std::size_t c=0;c<added_.size();c++){
      if(added_[c].empty())continue;
      added_[c].resize(static_cast<std::size_t>(nrow));
      table_[c]=added_[c].view();
    }
  } catch (const std::bad_alloc &) {
    return xpp::fail("storage","Cannot allocate sufficient storage",xpp::command_place());
  }
  max_rows=nrow;
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
  added_[c].assign(max_rows,precision_);
  table_[c]=added_[c].view();
}

Result<> DataStore::remove_column(int c, int count)
{
  if(c<pool_cols_||c>=count||count>static_cast<int>(added_.size()))
    return xpp::fail("storage",xpp::format("Cannot remove column {} of {}",c,count),xpp::command_place());
  for(int j=c;j<count-1;j++){
    std::swap(added_[j],added_[j+1]);
    table_[j]=added_[j].view();
  }
  added_[count-1].clear();
  table_[count-1]=Column();
  return {};
}

void DataStore::lend_columns(Column *dst, int from, int to) const
{
  for(int c=from;c<=to;c++)dst[c]=table_[c];
}

ColumnTable LentColumns::make(const DataStore &store, int n, int len, int last)
{
  try {
    own_.assign(n,{});
    for(OwnedColumn &c:own_)c.assign(len,store.precision());
  } catch (const std::bad_alloc &) {
    xpp::out_of_memory("a derived data set's columns");
  }
  table_.assign(std::max(n,last+1),Column());
  for(int c=0;c<n;c++)table_[c]=own_[c].view();
  last_=last;
  return view(store);
}

ColumnTable LentColumns::view(const DataStore &store)
{
  store.lend_columns(table_.data(),static_cast<int>(own_.size()),last_);
  return ColumnTable(table_.data());
}

void LentColumns::retype(const DataStore &store)
{
  for(std::size_t c=0;c<own_.size();c++){
    own_[c].convert(store.precision());
    table_[c]=own_[c].view();
  }
  if(last_>=0)view(store);
}

void storage_precision_changed(Session &s)
{
  const StoragePrecision wanted=s.numerics->storage_precision;
  const Result<> converted=s.data_store->set_precision(wanted);
  if(!converted){
    s.numerics->storage_precision=s.data_store->precision();
    ok_or_show(converted);
    return;
  }
  s.histogram->hist_columns.retype(*s.data_store);
  s.histogram->four_columns.retype(*s.data_store);
  s.adjoint->adjoint.retype(*s.data_store);
  s.adjoint->h_function.retype(*s.data_store);
  s.adjoint->transposed.retype(*s.data_store);
  retype_stochastic(s);
}

void LentColumns::release()
{
  own_.clear();
  table_.clear();
  last_=-1;
}

} // namespace xpp
