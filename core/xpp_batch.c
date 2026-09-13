/* Headless entry point: load an ODE file, integrate, write output.dat and
   whatever else the batch options ask for. This is the XPPBatch branch of
   do_main() in main.c without any X11 setup, so it links against
   libxppcore alone. */
#include "xpp_batch.h"
#include "xpp_globals.h"
#include "xpp_ui.h"
#include "colormap.h"
#include "load_eqn.h"
#include "comline.h"
#include "form_ode.h"
#include "integrate.h"
#include "nullcline.h"
#include "numerics.h"
#include "storage.h"
#include "dae_fun.h"
#include "simplenet.h"
#include "do_fit.h"
#include "extra.h"
#include "graphics.h"
#include "browse.h"
#include "auto_nox.h"
#include "my_rhs.h"
#include "read_dir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define cstringmaj MYSTR1
#define cstringmin MYSTR2

extern int NCBatch, DFBatch;
extern char this_file[XPP_MAX_NAME];
extern int METHOD;
extern int (*rhs)();

/* nullcline.c / integrate.c batch helpers without a header prototype */
void set_colorization_stuff(void);
void silent_nullclines(void);
void silent_dfields(void);
void silent_equilibria(void);

/* ---- moved from main.c (appended by tools/move_funcs.py) --------------- */

void check_for_quiet(argc,argv)
char **argv;
int argc;
{
	/*First scan, check for any QUIET option set...*/
	int i = 0;
	/*Allow for multiple calls to the QUIET and LOGFILE options 
	on the command line. The last setting is the one that will stick.
	Settings of logfile and quiet in the xpprc file will be ignored
	if they are set on the command line.
	*/ 
	int quiet_specified_once=0;
	int logfile_specified_once=0;
	
	for(i=1;i<argc;i++)
	{
 	       if (strcmp(argv[i],"-quiet")==0)
	       {
	       	       set_option("QUIET",argv[i+1],1,NULL);
		       quiet_specified_once=1;
     		       i++;
	       }
	       else if (strcmp(argv[i],"-logfile")==0)
	       {
		       set_option("LOGFILE",argv[i+1],1,NULL);
		       logfile_specified_once = 1;
     		       i++;
	       }
	}
	/*If -quiet or -logfile were specified at least once on the command line
	we lock those in now...
	*/
	if (quiet_specified_once == 1)
	{
		OVERRIDE_QUIET=1;
	}
	if (logfile_specified_once == 1)
	{
		OVERRIDE_LOGFILE=1;
	}
}

void do_vis_env()
{
  set_X_vals();
  check_for_xpprc();
  set_internopts_xpprc_and_comline();
  
}

void xpp_reset_options(void)
{
  notAlreadySet.BIG_FONT_NAME=1;
  notAlreadySet.SMALL_FONT_NAME=1;
  notAlreadySet.BACKGROUND=1;
  notAlreadySet.IXPLT=1;
  notAlreadySet.IYPLT=1;
  notAlreadySet.IZPLT=1;
  notAlreadySet.AXES=1;
  notAlreadySet.NMESH=1;
  notAlreadySet.METHOD=1;
  notAlreadySet.TIMEPLOT=1;
  notAlreadySet.MAXSTOR=1;
  notAlreadySet.TEND=1;
  notAlreadySet.DT=1;
  notAlreadySet.T0=1;
  notAlreadySet.TRANS=1;
  notAlreadySet.BOUND=1;
  notAlreadySet.TOLER=1;
  notAlreadySet.DELAY=1;
  notAlreadySet.XLO=1;
  notAlreadySet.XHI=1;
  notAlreadySet.YLO=1;
  notAlreadySet.YHI=1;  
  notAlreadySet.UserBlack=1;
  notAlreadySet.UserWhite=1;
  notAlreadySet.UserMainWinColor=1;
  notAlreadySet.UserDrawWinColor=1;
  notAlreadySet.UserGradients=1;
  notAlreadySet.UserBGBitmap=1;
  notAlreadySet.UserMinWidth=1;
  notAlreadySet.UserMinHeight=1;
  notAlreadySet.YNullColor=1;
  notAlreadySet.XNullColor=1;
  notAlreadySet.StableManifoldColor=1;
  notAlreadySet.UnstableManifoldColor=1;
  notAlreadySet.START_LINE_TYPE=1;
  notAlreadySet.RandSeed=1;
  notAlreadySet.PaperWhite=1;
  notAlreadySet.COLORMAP=1;
  notAlreadySet.NPLOT=1;
  notAlreadySet.DLL_LIB=1;
  notAlreadySet.DLL_FUN=1;
  notAlreadySet.XP=1;
  notAlreadySet.YP=1;
  notAlreadySet.ZP=1;
  notAlreadySet.NOUT=1;
  notAlreadySet.VMAXPTS=1;
  notAlreadySet.TOR_PER=1;
  notAlreadySet.JAC_EPS=1;
  notAlreadySet.NEWT_TOL=1;
  notAlreadySet.NEWT_ITER=1;
  notAlreadySet.FOLD=1;
  notAlreadySet.DTMIN=1;
  notAlreadySet.DTMAX=1;
  notAlreadySet.ATOL=1;
  notAlreadySet.TOL=1;
  notAlreadySet.BANDUP=1;
  notAlreadySet.BANDLO=1;
  notAlreadySet.PHI=1;
  notAlreadySet.THETA=1;
  notAlreadySet.XMIN=1;
  notAlreadySet.XMAX=1;
  notAlreadySet.YMIN=1;
  notAlreadySet.YMAX=1;
  notAlreadySet.ZMIN=1;
  notAlreadySet.ZMAX=1;
  notAlreadySet.POIVAR=1;
  notAlreadySet.OUTPUT=1;
  notAlreadySet.POISGN=1;
  notAlreadySet.POISTOP=1;
  notAlreadySet.STOCH=1;
  notAlreadySet.POIPLN=1;
  notAlreadySet.POIMAP=1;
  notAlreadySet.RANGEOVER=1;
  notAlreadySet.RANGESTEP=1;
  notAlreadySet.RANGELOW=1;
  notAlreadySet.RANGEHIGH=1;
  notAlreadySet.RANGERESET=1;
  notAlreadySet.RANGEOLDIC=1;
  notAlreadySet.RANGE=1;
  notAlreadySet.NTST=1;
  notAlreadySet.NMAX=1;
  notAlreadySet.NPR=1;
  notAlreadySet.NCOL=1;
  notAlreadySet.DSMIN=1;
  notAlreadySet.DSMAX=1;
  notAlreadySet.DS=1;
  notAlreadySet.PARMAX=1;
  notAlreadySet.NORMMIN=1;
  notAlreadySet.NORMMAX=1;
  notAlreadySet.EPSL=1;
  notAlreadySet.EPSU=1;
  notAlreadySet.EPSS=1;
  notAlreadySet.RUNNOW=1;
  notAlreadySet.SEC=1;
  notAlreadySet.UEC=1;
  notAlreadySet.SPC=1;
  notAlreadySet.UPC=1;
  notAlreadySet.AUTOEVAL=1;
  notAlreadySet.AUTOXMAX=1;
  notAlreadySet.AUTOYMAX=1;
  notAlreadySet.AUTOXMIN=1;
  notAlreadySet.AUTOYMIN=1;
  notAlreadySet.AUTOVAR=1;
  notAlreadySet.PS_FONT=1;
  notAlreadySet.PS_LW=1;   
  notAlreadySet.PS_FSIZE=1;
  notAlreadySet.PS_COLOR=1;
  notAlreadySet.FOREVER=1;
  notAlreadySet.BVP_TOL=1;
  notAlreadySet.BVP_EPS=1;
  notAlreadySet.BVP_MAXIT=1;
  notAlreadySet.BVP_FLAG=1;
  notAlreadySet.SOS=1;
  notAlreadySet.FFT=1;
  notAlreadySet.HIST=1;
  notAlreadySet.PltFmtFlag=1;
  notAlreadySet.ATOLER=1;
  notAlreadySet.MaxEulIter=1;
  notAlreadySet.EulTol=1;
  notAlreadySet.EVEC_ITER=1;
  notAlreadySet.EVEC_ERR=1;
  notAlreadySet.NULL_ERR=1;
  notAlreadySet.NEWT_ERR=1;
  notAlreadySet.NULL_HERE=1;
  notAlreadySet.TUTORIAL=1;
  notAlreadySet.SLIDER1=1;
  notAlreadySet.SLIDER2=1;
  notAlreadySet.SLIDER3=1;
  notAlreadySet.SLIDER1LO=1;
  notAlreadySet.SLIDER2LO=1;
  notAlreadySet.SLIDER3LO=1;
  notAlreadySet.SLIDER1HI=1;
  notAlreadySet.SLIDER2HI=1;
  notAlreadySet.SLIDER3HI=1;
  notAlreadySet.POSTPROCESS=1;
  notAlreadySet.HISTCOL=1;
  notAlreadySet.HISTLO=1;
  notAlreadySet.HISTHI=1;
  notAlreadySet.HISTBINS=1;
   notAlreadySet.HISTCOL2=1;
  notAlreadySet.HISTLO2=1;
  notAlreadySet.HISTHI2=1;
  notAlreadySet.HISTBINS2=1;
  notAlreadySet.SPECCOL=1;
  notAlreadySet.SPECCOL2=1;
  notAlreadySet.SPECWIDTH=1;
  notAlreadySet.SPECWIN=1;
  notAlreadySet.PLOTFORMAT=1;
  notAlreadySet.DFGRID=1;
  notAlreadySet.DFBATCH=1;
  notAlreadySet.NCBATCH=1;
  notAlreadySet.COLORVIA=1;
  notAlreadySet.COLORIZE=1;
    notAlreadySet.COLORHI=1;
  notAlreadySet.COLORLO=1;
}

int xpp_batch_main(int argc, char **argv)
{
    char myfile[XPP_MAX_NAME];
    OptionsSet *tempNS;

    xpp_reset_options();
    get_directory(myfile);
    Xup = 0;
    sprintf(batchout, "output.dat");
    sprintf(PlotFormat, "ps");
    logfile = stdout;
    check_for_quiet(argc, argv);
    do_comline(argc, argv);
    XPPBatch = 1; /* headless: always batch, even without -silent */

    load_eqn();

    tempNS = (OptionsSet *)malloc(sizeof(OptionsSet));
    *tempNS = notAlreadySet;
    set_internopts(tempNS);
    free(tempNS);

    init_alloc_info();
    do_vis_env();
    set_all_vals();
    init_alloc_info();
    set_init_guess();
    update_all_ffts();
#ifdef AUTO
    init_auto_win();
#endif
    if (disc(this_file)) METHOD = 0;
    xppvermaj = (float)cstringmaj;
    xppvermin = (float)cstringmin;
    do_meth();
    set_delay();
    rhs = my_rhs;
    init_fit_info();
    strip_saveqn();
    create_plot_list();
    auto_load_dll();

    xpp_build_colormap();
    init_browser();
    init_all_graph();
    if_needed_load_set();
    if_needed_load_par();
    if_needed_load_ic();
    if_needed_select_sets();
    if_needed_load_ext_options();
    set_extra_graphs();
    set_colorization_stuff();
    batch_integrate();
    if (NCBatch > 0) silent_nullclines();
    if (DFBatch > 0) silent_dfields();
    silent_equilibria();
    return 0;
}
