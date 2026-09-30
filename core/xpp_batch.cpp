/* Headless entry point: load an ODE file, integrate, write output.dat and
   whatever else the batch options ask for. This is the XPPBatch branch of
   the historical do_main(), with no front end setup, so it links against
   libxppcore alone. */
#include "model.h"
#include "model_files.h"
#include "session.h"
#include "xpp_batch.h"
#include "load_eqn.h"
#include "odesol2.h"
#include "xpp_globals.h"
#include "colormap.h"
#include "comline.h"
#include "form_ode.h"
#include "integrate.h"
#include "nullcline.h"
#include "numerics.h"
#include "storage.h"
#include "dae_fun.h"
#include "simplenet.h"
#include "do_fit.h"
#include "graphics.h"
#include "browse.h"
#include "auto_nox.h"
#include "my_rhs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xpp_log.h"
#include "xpp_io.h"
#include <optional>
#include <string_view>
#include <utility>
#include "xpp_files.h"
#include "graf_par.h"

XppBatchOptions batch_options;

#define cstringmaj MYSTR1
#define cstringmin MYSTR2

/* nullcline.c / integrate.c batch helpers without a header prototype */

/* ---- moved from main.c (appended by tools/move_funcs.py) --------------- */

void check_for_quiet(xpp::Session &s, int argc, char **argv)
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
	       	       set_option(s,"QUIET",argv[i+1],1,NULL);
		       quiet_specified_once=1;
     		       i++;
	       }
	       else if (strcmp(argv[i],"-logfile")==0)
	       {
		       set_option(s,"LOGFILE",argv[i+1],1,NULL);
		       logfile_specified_once = 1;
     		       i++;
	       }
	}
	/*If -quiet or -logfile were specified at least once on the command line
	we lock those in now...
	*/
	if (quiet_specified_once == 1)
	{
		log_settings.quiet_from_command_line=1;
	}
	if (logfile_specified_once == 1)
	{
		log_settings.file_from_command_line=1;
	}
}

void do_vis_env(xpp::Session &s)
{
  check_for_xpprc();
  set_internopts_xpprc_and_comline(s);
  
}

void xpp_reset_options(xpp::Session &s)
{
  s.not_already_set.BIG_FONT_NAME=1;
  s.not_already_set.SMALL_FONT_NAME=1;
  s.not_already_set.IXPLT=1;
  s.not_already_set.IYPLT=1;
  s.not_already_set.IZPLT=1;
  s.not_already_set.AXES=1;
  s.not_already_set.NMESH=1;
  s.not_already_set.METHOD=1;
  s.not_already_set.TIMEPLOT=1;
  s.not_already_set.MAXSTOR=1;
  s.not_already_set.TEND=1;
  s.not_already_set.DT=1;
  s.not_already_set.T0=1;
  s.not_already_set.TRANS=1;
  s.not_already_set.BOUND=1;
  s.not_already_set.TOLER=1;
  s.not_already_set.DELAY=1;
  s.not_already_set.XLO=1;
  s.not_already_set.XHI=1;
  s.not_already_set.YLO=1;
  s.not_already_set.YHI=1;  
  s.not_already_set.UserBlack=1;
  s.not_already_set.UserWhite=1;
  s.not_already_set.UserMainWinColor=1;
  s.not_already_set.UserDrawWinColor=1;
  s.not_already_set.UserGradients=1;
  s.not_already_set.UserBGBitmap=1;
  s.not_already_set.UserMinWidth=1;
  s.not_already_set.UserMinHeight=1;
  s.not_already_set.YNullColor=1;
  s.not_already_set.XNullColor=1;
  s.not_already_set.StableManifoldColor=1;
  s.not_already_set.UnstableManifoldColor=1;
  s.not_already_set.START_LINE_TYPE=1;
  s.not_already_set.RandSeed=1;
  s.not_already_set.PaperWhite=1;
  s.not_already_set.COLORMAP=1;
  s.not_already_set.NPLOT=1;
  s.not_already_set.XP=1;
  s.not_already_set.YP=1;
  s.not_already_set.ZP=1;
  s.not_already_set.NOUT=1;
  s.not_already_set.VMAXPTS=1;
  s.not_already_set.TOR_PER=1;
  s.not_already_set.JAC_EPS=1;
  s.not_already_set.NEWT_TOL=1;
  s.not_already_set.NEWT_ITER=1;
  s.not_already_set.FOLD=1;
  s.not_already_set.DTMIN=1;
  s.not_already_set.DTMAX=1;
  s.not_already_set.BANDUP=1;
  s.not_already_set.BANDLO=1;
  s.not_already_set.PHI=1;
  s.not_already_set.THETA=1;
  s.not_already_set.XMIN=1;
  s.not_already_set.XMAX=1;
  s.not_already_set.YMIN=1;
  s.not_already_set.YMAX=1;
  s.not_already_set.ZMIN=1;
  s.not_already_set.ZMAX=1;
  s.not_already_set.POIVAR=1;
  s.not_already_set.OUTPUT=1;
  s.not_already_set.POISGN=1;
  s.not_already_set.POISTOP=1;
  s.not_already_set.STOCH=1;
  s.not_already_set.POIPLN=1;
  s.not_already_set.POIMAP=1;
  s.not_already_set.RANGEOVER=1;
  s.not_already_set.RANGESTEP=1;
  s.not_already_set.RANGELOW=1;
  s.not_already_set.RANGEHIGH=1;
  s.not_already_set.RANGERESET=1;
  s.not_already_set.RANGEOLDIC=1;
  s.not_already_set.RANGE=1;
  s.not_already_set.NTST=1;
  s.not_already_set.NMAX=1;
  s.not_already_set.NPR=1;
  s.not_already_set.NCOL=1;
  s.not_already_set.DSMIN=1;
  s.not_already_set.DSMAX=1;
  s.not_already_set.DS=1;
  s.not_already_set.PARMAX=1;
  s.not_already_set.NORMMIN=1;
  s.not_already_set.NORMMAX=1;
  s.not_already_set.EPSL=1;
  s.not_already_set.EPSU=1;
  s.not_already_set.EPSS=1;
  s.not_already_set.RUNNOW=1;
  s.not_already_set.SEC=1;
  s.not_already_set.UEC=1;
  s.not_already_set.SPC=1;
  s.not_already_set.UPC=1;
  s.not_already_set.AUTOEVAL=1;
  s.not_already_set.AUTOXMAX=1;
  s.not_already_set.AUTOYMAX=1;
  s.not_already_set.AUTOXMIN=1;
  s.not_already_set.AUTOYMIN=1;
  s.not_already_set.AUTOVAR=1;
  s.not_already_set.PS_FONT=1;
  s.not_already_set.PS_LW=1;   
  s.not_already_set.PS_FSIZE=1;
  s.not_already_set.PS_COLOR=1;
  s.not_already_set.FOREVER=1;
  s.not_already_set.BVP_TOL=1;
  s.not_already_set.BVP_EPS=1;
  s.not_already_set.BVP_MAXIT=1;
  s.not_already_set.BVP_FLAG=1;
  s.not_already_set.SOS=1;
  s.not_already_set.FFT=1;
  s.not_already_set.HIST=1;
  s.not_already_set.PltFmtFlag=1;
  s.not_already_set.ATOLER=1;
  s.not_already_set.MaxEulIter=1;
  s.not_already_set.EulTol=1;
  s.not_already_set.EVEC_ITER=1;
  s.not_already_set.EVEC_ERR=1;
  s.not_already_set.NEWT_ERR=1;
  s.not_already_set.NULL_HERE=1;
  s.not_already_set.TUTORIAL=1;
  s.not_already_set.SLIDER1=1;
  s.not_already_set.SLIDER2=1;
  s.not_already_set.SLIDER3=1;
  s.not_already_set.SLIDER1LO=1;
  s.not_already_set.SLIDER2LO=1;
  s.not_already_set.SLIDER3LO=1;
  s.not_already_set.SLIDER1HI=1;
  s.not_already_set.SLIDER2HI=1;
  s.not_already_set.SLIDER3HI=1;
  s.not_already_set.POSTPROCESS=1;
  s.not_already_set.HISTCOL=1;
  s.not_already_set.HISTLO=1;
  s.not_already_set.HISTHI=1;
  s.not_already_set.HISTBINS=1;
   s.not_already_set.HISTCOL2=1;
  s.not_already_set.HISTLO2=1;
  s.not_already_set.HISTHI2=1;
  s.not_already_set.HISTBINS2=1;
  s.not_already_set.SPECCOL=1;
  s.not_already_set.SPECCOL2=1;
  s.not_already_set.SPECWIDTH=1;
  s.not_already_set.SPECWIN=1;
  s.not_already_set.PLOTFORMAT=1;
  s.not_already_set.DFGRID=1;
  s.not_already_set.DFBATCH=1;
  s.not_already_set.NCBATCH=1;
  s.not_already_set.COLORVIA=1;
  s.not_already_set.COLORIZE=1;
    s.not_already_set.COLORHI=1;
  s.not_already_set.COLORLO=1;
}

/* everything both the batch run and an interactive front end do: options,
   the ODE file, numerics set-up. batch forces XPPBatch. */
static void load_and_set_up(xpp::Session &s, int argc, char **argv, int batch)
{
    xpp::Model &m = s.model();
    xpp_reset_options(s);
    program.interactive = 0;
    batch_options.out_file = "output.dat";
    s.plot_export.format = "ps";
    check_for_quiet(s, argc, argv);
    do_comline(s, argc, argv);
    if (batch) batch_options.enabled = 1; /* headless: always batch, even without -silent */

    load_eqn(s);
    /* the log settings of the model before (@ logfile, @ quiet) go, now
       that this one has parsed; its own options below set them again */
    xpp_log_new_model();
    /* the boundary conditions in use start as the model's */
    s.bcs = m.bcs;

    OptionsSet mask = s.not_already_set;
    set_internopts(s, &mask);

    init_alloc_info(s);
    do_vis_env(s);
    set_all_vals(s);
    init_alloc_info(s);
    set_init_guess(s);
    update_all_ffts(s);
#ifdef AUTO
    init_auto_win(s);
#endif
    if (disc(m.this_file)) s.numerics.method = 0;
    program.version_major = static_cast<float>(cstringmaj);
    program.version_minor = static_cast<float>(cstringmin);
    do_meth(s);
    set_delay(s);
    s.integrator.rhs = {my_rhs, &s};
    init_fit_info();
    strip_saveqn(m);
    create_plot_list(s);
}

xpp::Loaded xpp::load_model(int argc, char **argv, int batch, const SavedModel *saved)
{
    /* the parser and the set-up fill a fresh Model and Session, kept only
       when the load gets to the end: a failed one puts back those before */
    xpp::Load load;
    /* the process-wide settings a load writes, put back when it fails */
    const XppProgram program_before = program;
    const XppBatchOptions batch_before = batch_options;
    xpp::Model &m = load.model();
    try {
        m.command_line.assign(argv, argv + argc);
        m.load_dir = xpp_files_working_dir();
        if (saved) {
            m.saved_in = saved->in;
            m.files = saved->files;
        }
        load_and_set_up(load.session(), argc, argv, batch);
    } catch (xpp::LoadFailed &failed) {
        program = program_before;
        batch_options = batch_before;
        return std::unexpected(std::move(failed.diagnostic));
    }
    m.saved_copies.reset(); /* the load's readers are done with them */
    load.commit();
    return &load.session();
}

void xpp::model_failed(Diagnostic d)
{
    if (!xpp::Load::running()) exit(1);
    xpp::Load::add_source(d);
    throw xpp::LoadFailed{std::move(d)};
}

void xpp_model_failed(void)
{
    xpp::model_failed(xpp::Load::running() ? xpp::Load::diagnostic() : xpp::Diagnostic());
}

void xpp_batch_start(xpp::Session &s)
{
    xpp_build_colormap();
    init_browser(s);
    init_all_graph(s);
    if_needed_select_sets(s.model());
    load_command_line_values(s);
    set_extra_graphs(s);
    set_colorization_stuff(s);
}
