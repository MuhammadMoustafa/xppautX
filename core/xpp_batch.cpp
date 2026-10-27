/* Headless entry point: load an ODE file, integrate, write output.dat and
   whatever else the batch options ask for. This is the XPPBatch branch of
   the historical do_main(), with no front end setup, so it links against
   libxppcore alone. */
#include "model_options.h"
#include "xpp_session.h"
#include "solver.h"
#include "model.h"
#include "model_files.h"
#include "session.h"
#include "expr_native.h"
#include "jacobian.h"
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
#include "auto_settings.h"
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


void do_vis_env(xpp::Session &s)
{
  set_internopts_xpprc_and_comline(s, check_for_xpprc());
  
}

/* everything both the batch run and an interactive front end do: options,
   the ODE file, numerics set-up. batch forces XPPBatch. */
static void load_and_set_up(xpp::Session &s, const CommandLine &cl, int batch,
                            const std::function<void(Session &)> &read)
{
    xpp::Model &m = s.model();
    program.interactive = 0;
    s.plot_export.format = "pdf";
    apply_command_line(s, cl);
    if (batch) batch_options.enabled = 1; /* headless: always batch, even without --silent */

    s.options_applied = 0;
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
    /* a set is checked whole at the load (a bad one stops it) */
    if (const Result<> sets = check_intern_sets(s); !sets) model_failed(sets.error());
    xpp::init_alloc_info(s);
    xpp::set_init_guess(s);
    update_all_ffts(s);
    init_auto_win(s);
    auto_threads_load(s);
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
    xpp::Model &m = load.model();
    try {
        m.load_dir = xpp::files::working_dir();
        work(load.session());
    } catch (xpp::LoadFailed &failed) {
        program = program_before;
        return std::unexpected(std::move(failed.error));
    }
    if (!commit) {
        program = program_before;
        return nullptr; /* the inspected model is discarded */
    }
    const std::string missing = missing_initial_values_text(m);
    if (!missing.empty()) {
        Load::at(m.this_file);
        xpp::log(XPP_LOG_WARN, "states without init start at 0:{}\n", missing);
    }
    m.saved_copies.reset(); /* the load's readers are done with them */
    xpp_session_mark_clean(load.session()); /* what it loaded is not a change */
    auto compiled=compile_model(load.session());
    if (!compiled) compiled=std::vector<Error>{compiled.error()};
    for (const Error &warning:*compiled) {
        Load::at(warning.place.file,warning.place.line,warning.place.col);
        xpp::log(XPP_LOG_WARN,"{}\n",warning.what);
    }
    /* the exact Jacobian a model asks for is known to exist only now
       (W260): none is an error at the @ line, not a quiet difference */
    if (load.session().numerics.jacobian==JACOBIAN_EXACT && !m.jacobian.available()) {
        program = program_before;
        return std::unexpected(Error{"options",option_error_text("jacobian","exact","the model has no exact Jacobian: "+m.jacobian.unavailable),
                                     m.jacobian.requested});
    }
    load.commit();
    return &load.session();
}

Loaded load_in(const CommandLine &cl, int batch, const SavedModel *saved,
               const std::function<std::optional<Error>(Session &)> &check,
               const std::function<void(Session &)> &read, bool commit,
               const std::vector<std::string> &outside_paths = {})
{
    return in_model([&](Session &s) {
        Model &m = s.model();
        m.outside_paths = outside_paths;
        if (saved) {
            m.saved_in = saved->in;
            m.files = saved->files;
        }
        load_and_set_up(s, cl, batch, read);
        if (check)
            if (std::optional<Error> wrong = check(s)) model_failed(std::move(*wrong));
    }, commit);
}

} // namespace

Loaded load_model(CommandLine cl, int batch, const SavedModel *saved,
                  const std::function<std::optional<Error>(Session &)> &check)
{
    std::string file, converted;
    bool existing = false;
    std::vector<std::string> included;
    std::vector<odex::Diagnostic> diagnostics;
    std::vector<std::string> outside_paths;
    Loaded prepared = in_model([&](Session &s) {
        apply_command_line(s, cl);
        included = cl.include;
        if (batch) batch_options.enabled = 1;
        choose_model_file(s);
        file = s.model().this_file;
        if (saved && !odex::is_odex(file))
            model_failed(Error{"open", "a saved model must be .odex", Place{saved->in}});
        if (cl.model_argument < 0) {
            cl.model_argument = static_cast<int>(cl.arguments.size());
            cl.arguments.push_back(file);
            cl.model_file = file;
        }
    }, false);
    Loaded loaded = prepared;
    if (prepared) {
        if (xpp::files::has_extension(file, ".ode")) {
            Result<bool> opened = odex::open_ode(file, included, diagnostics, outside_paths);
            if (!opened) loaded = std::unexpected(opened.error());
            else {
                converted = odex::odex_name(file);
                existing = !*opened;
                /* The converter has folded these foreign includes into
                   the .odex; Reload uses that file from now on. */
                std::vector<std::string> args;
                for (size_t i = 0; i < cl.arguments.size(); ++i) {
                    if (cl.arguments[i] == "--include" && i + 1 < cl.arguments.size()) ++i;
                    else args.push_back(static_cast<int>(i) == cl.model_argument ? converted : cl.arguments[i]);
                }
                Result<CommandLine> converted_line = parse_command_line(std::move(args));
                if (converted_line) cl = std::move(*converted_line);
                else loaded = std::unexpected(converted_line.error());
            }
        }
        if (loaded) loaded = load_in(cl, batch, saved, check, load_eqn, true, outside_paths);
    }
    /* the one place a model that does not load is written to the log */
    if (!loaded) {
        if (batch || !is_no_model(loaded.error())) xpp::log(XPP_LOG_ERROR, "{}\n", loaded.error().text());
    } else if (!converted.empty()) {
        const std::string message = existing ? xpp::format("Opened the existing {}. The model now open is {}.", converted, converted)
                                            : xpp::format("Converted {} and saved as {}. The model now open is {}.", file, converted, converted);
        if (batch) xpp::log(XPP_LOG_WARN, "{}\n", message);
        else xpp::bottom_msg(0, message, "02-ode-files", "odex");
    }
    odex::show_diagnostics(diagnostics);
    return loaded;
}

Result<> inspect_model(const CommandLine &cl, const std::function<void(Session &)> &read,
                       const std::function<std::optional<Error>(Session &)> &check,
                       const SavedModel *saved)
{
    /* Inspection validates model options but must not create @ logfile's file. */
    const int file_option = log_settings.file_from_command_line;
    log_settings.file_from_command_line = 1;
    Loaded inspected = load_in(cl, 1, saved, check, read, false);
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
    /* only the parser and the readers a load runs call this: a model is
       built only by a load */
    xpp::model_failed(xpp::Load::error(what));
}

void batch_start(xpp::Session &s)
{
    init_browser(s);
    init_all_graph(s);
    if_needed_select_sets(s.model());
    load_command_line_values(s);
    set_extra_graphs(s);
    set_colorization_stuff(s);
}

} // namespace xpp
