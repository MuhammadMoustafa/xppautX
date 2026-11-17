#pragma once
/* The histogram and the spectrum's results a Session holds (histogram.cpp): plain data the analyses work on (W744). Their options, which
   the option table sets, are model_histogram_options.h's, its base (W757). */
#include "model_histogram_options.h"
#include "storage.h"

namespace xpp {

/* histogram.cpp's results, a Session's (session.h), beside the histogram and spectrum options: the last histogram and Fourier
   transform's columns (storage.h's LentColumns: read back by the data
   data table and the tests), whether one exists and its length */
struct HistogramState : HistogramSettings {
  LentColumns hist_columns, four_columns;
  ColumnTable hist() noexcept { return hist_columns.table(); }
  ColumnTable four() noexcept { return four_columns.table(); }
  int hist_here=0, four_here=0, hist_len=0, four_len=0;
};

} // namespace xpp
