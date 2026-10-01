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

namespace xpp {

XppBatchOptions batch_options;


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
		xpp::log_settings.quiet_from_command_line=1;
	}
	if (logfile_specified_once == 1)
	{
		xpp::log_settings.file_from_command_line=1;
	}
}

void do_vis_env(xpp::Session &s)
{
  set_internopts_xpprc_and_comline(s, check_for_xpprc());
  
}

/* everything both the batch run and an interactive front end do: options,
   the ODE file, numerics set-up. batch forces XPPBatch. */
static void load_and_set_up(xpp::Session &s, int argc, char **argv, int batch)
{
    xpp::Model &m = s.model();
    program.interactive = 0;
    batch_options.out_file = "output.dat";
    s.plot_export.format = "ps";
    check_for_quiet(s, argc, argv);
    do_comline(s, argc, argv);
    if (batch) batch_options.enabled = 1; /* headless: always batch, even without -silent */

    load_eqn(s);
    /* the log settings of the model before (@ logfile, @ quiet) go, now
       that this one has parsed; its own options below set them again */
    xpp::log_new_model();
    /* the boundary conditions in use start as the model's */
    s.bcs = m.bcs;

    const OptionsSet mask = s.options_set;
    set_internopts(s, &mask);

    xpp::init_alloc_info(s);
    do_vis_env(s);
    set_all_vals(s);
    xpp::init_alloc_info(s);
    xpp::set_init_guess(s);
    update_all_ffts(s);
#ifdef AUTO
    init_auto_win(s);
#endif
    if (disc(m)) s.numerics.method = 0;
    program.version_major = static_cast<float>(MYSTR1);
    program.version_minor = static_cast<float>(MYSTR2);
    xpp::do_meth(s);
    xpp::set_delay(s);
    s.integrator.rhs = {my_rhs, &s};
    strip_saveqn(m);
    create_plot_list(s);
}

namespace {

/* load_model's work: the Session loaded, else what is wrong and where.
   The Load ends here, before its error is written: the messages it kept
   are written first */
Loaded load_in(int argc, char **argv, int batch, const SavedModel *saved,
               const std::function<std::optional<Error>(Session &)> &check)
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
        m.load_dir = xpp::files::working_dir();
        if (saved) {
            m.saved_in = saved->in;
            m.files = saved->files;
        }
        load_and_set_up(load.session(), argc, argv, batch);
        if (check)
            if (std::optional<Error> wrong = check(load.session())) throw xpp::LoadFailed{std::move(*wrong)};
    } catch (xpp::LoadFailed &failed) {
        program = program_before;
        batch_options = batch_before;
        return std::unexpected(std::move(failed.error));
    }
    m.saved_copies.reset(); /* the load's readers are done with them */
    load.commit();
    return &load.session();
}

} // namespace

Loaded load_model(int argc, char **argv, int batch, const SavedModel *saved,
                  const std::function<std::optional<Error>(Session &)> &check)
{
    Loaded loaded = load_in(argc, argv, batch, saved, check);
    /* the one place a model that does not load is written to the log */
    if (!loaded) xpp::log(XPP_LOG_ERROR, "{}\n", loaded.error().text());
    return loaded;
}

void model_failed(Error e)
{
    if (!xpp::Load::running()) {
        if (!e.what.empty()) xpp::log(XPP_LOG_ERROR, "{}\n", e.text());
        exit(1);
    }
    xpp::Load::add_source(e);
    throw xpp::LoadFailed{std::move(e)};
}

void model_failed()
{
    xpp::model_failed(xpp::Load::running() ? xpp::Load::error() : xpp::Error());
}

void batch_start(xpp::Session &s)
{
    xpp_build_colormap(s.colormap);
    init_browser(s);
    init_all_graph(s);
    if_needed_select_sets(s.model());
    load_command_line_values(s);
    set_extra_graphs(s);
    set_colorization_stuff(s);
}

} // namespace xpp
