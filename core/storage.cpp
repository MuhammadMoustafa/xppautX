
#include "storage.h"
#include "xpp_ui.h"
#include "xpp_mem.h"
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

void init_alloc_info()
{
  xpv.node=NODE+NMarkov;
  xpv.nvec=0; /* this is just for now */
  xpp_free(xpv.x); /* called again once the model's options are read */
  /* xpv.x is a shared raw block (storage.h XPPVEC) read across the
     numerics code by pointer; it stays xpp_malloc/xpp_free. */
  xpv.x=static_cast<double *>(xpp_malloc((xpv.nvec+xpv.node)*sizeof(double)));
  for(int i=xpv.node;i<(xpv.nvec+xpv.node);i++)
    xpv.x[i]=0.0;
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
  if(WORK)
    xpp_free(WORK);
  /* WORK is the shared scratch block the ODE solvers index directly;
     it stays xpp_malloc/xpp_free. */
  WORK=static_cast<double *>(xpp_malloc(sz*sizeof(double)));
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
