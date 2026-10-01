#ifndef _array_print_h_
#define _array_print_h_

namespace xpp {

/* array_print.cpp: Print arrayplot's PostScript file */
int array_print(const char *filename, const char *xtitle, const char *ytitle, const char *bottom, int nacross, int ndown, int col0, int row0, int nskip, int ncskip, int maxrow, int maxcol, float **data, double zmin, double zmax, double tlo, double thi, int type);

} // namespace xpp
#endif
