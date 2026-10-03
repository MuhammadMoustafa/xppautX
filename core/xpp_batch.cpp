/* Headless entry point: load an ODE file, integrate, write output.dat and
   whatever else the batch options ask for. This is the XPPBatch branch of
   the historical do_main(), with no front end setup, so it links against
   libxppcore alone. */
#include "solver.h"
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
#include "odex.h"
#include "snapx.h"
#include "xpp_ui.h"

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
	       if (strcmp(argv[i],"--quiet")==0)
	       {
	       	       set_option(s,"QUIET",argv[i+1],1,NULL);
		       quiet_specified_once=1;
     		       i++;
	       }
	       else if (strcmp(argv[i],"--logfile")==0)
	       {
		       set_option(s,"LOGFILE",argv[i+1],1,NULL);
		       logfile_specified_once = 1;
     		       i++;
	       }
	}
	/*If --quiet or --logfile were specified at least once on the command line
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
static void load_and_set_up(xpp::Session &s, int argc, char **argv, int batch,
                            const std::function<void(Session &)> &read)
{
    xpp::Model &m = s.model();
    program.interactive = 0;
    batch_options.out_file = "output.dat";
    s.plot_export.format = "ps";
    check_for_quiet(s, argc, argv);
    do_comline(s, argc, argv);
    if (batch) batch_options.enabled = 1; /* headless: always batch, even without --silent */

    s.options_applied = 0;
    for (int i = 0; i < MAXODE; i++) {
        s.itor[i] = 0;
        s.delay_string[i] = "0.0";
    }
    read(s);
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
    if (disc(m)) {
        const auto picked = pick_method(m, "Discrete", Load::place());
        if (!picked) model_failed(picked.error());
        s.numerics.method = *picked;
    }
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
Loaded in_model(const std::function<void(Session &)> &work, bool commit)
{
    /* the parser and the set-up fill a fresh Model and Session, kept only
       when the load gets to the end: a failed one puts back those before */
    xpp::Load load;
    /* the process-wide settings a load writes, put back when it fails */
    const XppProgram program_before = program;
    const XppBatchOptions batch_before = batch_options;
    xpp::Model &m = load.model();
    try {
        m.load_dir = xpp::files::working_dir();
        work(load.session());
    } catch (xpp::LoadFailed &failed) {
        program = program_before;
        batch_options = batch_before;
        return std::unexpected(std::move(failed.error));
    }
    if (!commit) {
        program = program_before;
        batch_options = batch_before;
        return nullptr; /* the inspected model is discarded */
    }
    m.saved_copies.reset(); /* the load's readers are done with them */
    load.commit();
    return &load.session();
}

Loaded load_in(int argc, char **argv, int batch, const SavedModel *saved,
               const std::function<std::optional<Error>(Session &)> &check,
               const std::function<void(Session &)> &read, bool commit)
{
    return in_model([&](Session &s) {
        Model &m = s.model();
        m.command_line.assign(argv, argv + argc);
        if (saved) {
            m.saved_in = saved->in;
            m.files = saved->files;
        }
        load_and_set_up(s, argc, argv, batch, read);
        if (check)
            if (std::optional<Error> wrong = check(s)) model_failed(std::move(*wrong));
    }, commit);
}

} // namespace

Loaded load_model(int argc, char **argv, int batch, const SavedModel *saved,
                  const std::function<std::optional<Error>(Session &)> &check)
{
    std::vector<std::string> args(argv, argv + argc);
    std::string file, converted;
    bool existing = false;
    int argument = -1;
    std::vector<std::string> included;
    std::vector<odex::Diagnostic> diagnostics;
    Loaded prepared = in_model([&](Session &s) {
        argument = do_comline(s, argc, argv);
        included = include_files;
        if (batch) batch_options.enabled = 1;
        choose_model_file(s);
        file = s.model().this_file;
        if (saved && !odex::is_odex(file))
            model_failed(Error{"open", "a saved model must be .odex", Place{saved->in}});
        if (argument < 0) {
            argument = static_cast<int>(args.size());
            args.push_back(file);
        }
    }, false);
    Loaded loaded = prepared;
    if (prepared) {
        if (snapx::has_extension(file, ".ode")) {
            Result<bool> opened = odex::open_ode(file, batch != 0, included, diagnostics);
            if (!opened) loaded = std::unexpected(opened.error());
            else {
                converted = odex::odex_name(file);
                existing = !*opened;
                args[static_cast<size_t>(argument)] = converted;
                /* The converter has folded these foreign includes into
                   the .odex; Reload uses that file from now on. */
                for (size_t i = 1; i + 1 < args.size();) {
                    if (args[i] == "--include") args.erase(args.begin() + static_cast<std::ptrdiff_t>(i), args.begin() + static_cast<std::ptrdiff_t>(i + 2));
                    else ++i;
                }
            }
        } else {
            /* extensions other than .ode are checked by the .odex loader */
            args[static_cast<size_t>(argument)] = file;
        }
        if (loaded) {
            std::vector<char *> ready;
            for (std::string &a : args) ready.push_back(a.data());
            ready.push_back(nullptr);
            loaded = load_in(static_cast<int>(args.size()), ready.data(), batch, saved, check, load_eqn, true);
        }
    }
    /* the one place a model that does not load is written to the log */
    if (!loaded) xpp::log(XPP_LOG_ERROR, "{}\n", loaded.error().text());
    else if (!converted.empty()) {
        const std::string message = existing ? xpp::format("Opened the existing {}. The model now open is {}.", converted, converted)
                                            : xpp::format("Converted {} and saved as {}. The model now open is {}.", file, converted, converted);
        if (batch) xpp::log(XPP_LOG_WARN, "{}\n", message);
        else xpp::bottom_msg(0, message, "02-ode-files", "odex");
    }
    odex::show_diagnostics(diagnostics);
    return loaded;
}

Result<> inspect_model(int argc, char **argv, const std::function<void(Session &)> &read,
                       const std::function<std::optional<Error>(Session &)> &check,
                       const SavedModel *saved)
{
    /* Inspection validates model options but must not create @ logfile's file. */
    const int file_option = log_settings.file_from_command_line;
    log_settings.file_from_command_line = 1;
    Loaded inspected = load_in(argc, argv, 1, saved, check, read, false);
    log_settings.file_from_command_line = file_option;
    if (!inspected) return std::unexpected(inspected.error());
    return {};
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

void model_failed(std::string_view what)
{
    /* no load is in progress: a model is built only by one, so there is
       no place to name */
    xpp::model_failed(xpp::Load::running() ? xpp::Load::error(what) : xpp::Error{"model", std::string(what), xpp::Place{}});
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
