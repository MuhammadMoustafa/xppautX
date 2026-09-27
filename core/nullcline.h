#ifndef _nullcline_h_
#define _nullcline_h_

#include "xpplim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* nullcline.cpp: nullclines and direction fields */
extern int NCSuppress, DFSuppress, NCBatch, DFBatch, NullStyle;
extern int XNullColor, YNullColor;
extern int DF_GRID, DF_FLAG, DF_IX, DF_IY, DFIELD_TYPE;
/* set while the direction field is drawn, for the SVG classes */
extern int DOING_DFIELD;
extern double ColorViaLo, ColorViaHi;
extern int ColorizeFlag;

void create_new_cline(void);
void froz_cline_stuff_com(int i);
/* the nullclines (current, or frozen number who) as segments: 4 floats each */
int get_nullcline_floats(float **v, int *n, int who, int type);
void redraw_dfield(void);
void direct_field_com(int c);
void restore_nullclines(void);
void new_clines_com(int c);
void do_batch_nclines(void);
void do_batch_dfield(void);
/* the -silent run's nullclines.dat and dirfields.dat */
void silent_nullclines(void);
void silent_dfields(void);

#ifdef __cplusplus
}

#include <string>
/* what colours an orbit by (@ colorvia=) */
extern std::string ColorVia;

/* C++ linkage: xppautx_main.cpp declares it so itself */
void set_colorization_stuff(void);
#endif
#endif
