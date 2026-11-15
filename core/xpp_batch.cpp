/* Headless entry point: load an ODE file, integrate, write output.dat and
   whatever else the batch options ask for. This is the XPPBatch branch of
   the historical do_main(), with no front end setup, so it links against
   libxppcore alone. */
#include "model_options.h"
#include "model_method.h"
#include "xpp_session.h"
#include "load_eqn.h"
#include "set_apply.h"
#include "solver.h"
#include "model.h"
#include "model_record.h"
#include "model_place.h"
#include "plot_settings.h"
#include "session.h"
#include "autox_state.h"
#include "model_parts.h"
#include "model_native.h"
#include "jacobian.h"
#include "xpp_batch.h"
#include "model_failed.h"
#include "command_registry.h"
#include "command_def.h"
#include "data_formats.h"
#include "image_format.h"
#include "model_load.h"
#include "xpp_globals.h"
#include "comline.h"
#include "form_ode.h"
#include "integrate.h"
#include "nullcline.h"
#include "numerics.h"
#include "storage.h"
#include "dae_fun.h"
#include "delay_handle.h"
#include "adj2.h"
#include "volterra2.h"
#include "simplenet.h"
#include "graphics.h"
#include "browse.h"
#include "autox_nox.h"
#include "autox_settings.h"
#include "my_rhs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xpp_log.h"
#include "xpp_io.h"
#include <optional>
#include <string_view>
#include <utility>
#include <cstddef>
#include <memory>
#include "xpp_files.h"
#include "odex.h"
#include "autox_script.h"
#include "aniparse.h"
#include "xpp_ui.h"
#include "expr_program.h"
#include "xpp_prefs.h"

namespace xpp {

void install_application_commands()
{
    command::install_application(command::model_commands(), command::preferences_commands(), command::update_commands());
}

namespace {

/* the registry's shape check, which the script reader is given */
Result<> script_shape(const std::string &line, const Place &place) { return command::check_shape(line.c_str(), place); }

/* every run's lines checked against the loaded model (its parameters' names, the settings' values): the first fault, at its run's line */
Result<> check_autox_script(Session &s, const AutoxScript &script)
{
    for (const AutoxScriptRun &run : script.runs) {
        const std::string axes = autox_script_axes_command(run);
        for (const std::string &each : {axes, autox_script_command(run)})
            if (!each.empty())
                if (const Result<> shaped = command::check_shape(each.c_str(), run.place, &s); !shaped) return shaped;
    }
    return {};
}

} // namespace

namespace {

/* the load's steps that read or fill the Session (the readers of model and loading give it only the Model) */
void choose_model_file(xpp::Session &s)
{
 std::string &file=s.model().this_file;
 xpp::FileSystem &fs=*s.model().fs;
 if(s.got_file&&!fs.is_dir(file))return;
 /* no file, or a folder: the start screen's Open model asks, in that folder
    (xppautx_main.cpp); the dialog is File > Open model's, not a second one */
 if(s.got_file)fs.change_dir(file);
 const char *start=getenv("XPPSTART");
 if(start&&fs.is_dir(start))fs.change_dir(start);
 model_failed(Error{std::string(xpp::NO_MODEL_WHERE),"no model file was named",Place{fs.working_dir()}});
}

void load_eqn(xpp::Session &s)
{
 xpp::Model &m=s.model();
 const std::string &file=m.this_file;
 xpp::LoadSite::at(file);
 if(!xpp::odex::is_odex(file))
   model_failed(Error{"model","the loader reads .odex only; open a .ode to convert it",Place{file}});
 std::string bytes;
 if(!xpp::read_model_file(m,file,bytes))
   model_failed(Error{"model",m.saved_in.empty()?"cannot be read":xpp::format("is not saved in {}",m.saved_in),Place{file}});
 if(!xpp::is_model_text(bytes))
   model_failed(Error{"model",xpp::zip::is_zip(bytes)?"is not a model: it is a zip file (a session file is a .snapx)":"is not a model: it is a binary file",Place{file}});
 try {
   /* the parsed statements go before the model is built: the built list keeps only the expanded ones */
   odex::Parsed readied=odex::ready(m,odex::parse(m,bytes,file));
   build_model(s,std::move(readied));
 } catch (const Error &e) {
   model_failed(e);
 }
 /* the model's source: the .odex file's own lines */
 set_model_source(m,bytes);
}

void set_internopts(xpp::Session &s, const OptionsSet *mask)
{
  const std::vector<xpp::Model::OptionLine> &options=s.model().options;
  if(s.options_applied>=options.size())return;
  for(;s.options_applied<options.size();s.options_applied++){
    /* a value an option refuses is reported at its line */
    const xpp::Place &at=options[s.options_applied].where;
    const std::string &file=at.file.empty()?s.model().this_file:at.file;
    xpp::LoadSite::at(file,at.line,at.col);
    for(const auto &[name,value] : options[s.options_applied].items)
      set_option(s,name,value,0,mask);
  }
  std::array<double, xpp::AUTOX_NUM_N> values{};
  std::array<int, xpp::AUTOX_NUM_N> supplied{};
  for (const xpp::OptionRow &row : xpp::option_rows()) {
    if (row.autox_numeric < 0) continue;
    values[row.autox_numeric] = row.real ? row.real(s) : row.whole ? row.whole(s) : row.boolean ? row.boolean(s) : row.index.read(s);
    supplied[row.autox_numeric] = s.options_set.has(row);
  }
  if (const auto valid = xpp::autox_settings_numbers_check(values, supplied); !valid) {
    const auto row = std::ranges::find_if(xpp::option_rows(), [&](const xpp::OptionRow &r) {
      return r.autox_numeric >= 0 && valid.error().field == xpp::autox_settings_num_key(r.autox_numeric);
    });
    xpp::Error error = valid.error();
    error.place = xpp::model_place(s.model(), option_name(*row));
    error.place.source = xpp::model_source_line(s.model(), error.place.file, error.place.line);
    xpp::model_failed(error);
  }
  xpp::LoadSite::at(s.model().this_file);
}

void set_all_vals(xpp::Session &s)
{
 s.plot_settings->plot_3d=0;
 xpp::set_option_defaults(s);
 /* internal options go here  */
 set_internopts(s,NULL);
 /* every option has its value now (a source's or its default): the orders they keep hold */
 if (const auto bad = xpp::option_pairs_problem(s)) {
   xpp::Place place = xpp::model_place(s.model(), bad->hi);
   place.source = xpp::model_source_line(s.model(), place.file, place.line);
   xpp::model_failed(xpp::Error{"options", bad->text, place});
 }

 init_txtview();

 PlotCurve &axes_curve=s.plot_settings->curves[0];
 if(axes_curve.z>s.model().neq)axes_curve.z=s.model().neq;
 if(axes_curve.y>s.model().neq)axes_curve.y=s.model().neq;
 if(axes_curve.x==0||axes_curve.y==0)
   s.plot_settings->timplot=1;
 else 
   s.plot_settings->timplot=0;
 if(s.plot_settings->axes>=5)s.plot_settings->plot_3d=1;
 set_initial_values(s); /* the initial values given as formulas */
}

} // namespace

/* the command line's switches into the loading Session s and the model's
   file into its Model's this_file; the Model keeps cl. */
void apply_command_line(Session &s, CommandLine cl)
{
  s.got_file = 0;
  if (!cl.model_file.empty()) {
    s.model().this_file = cl.model_file;
    s.got_file = 1;
  }
  if (cl.new_seed) xpp::log(XPP_LOG_INFO, "Random number seed changed\n");
  if (cl.silent) batch_options.enabled = 1;
  if (cl.no_output) s.integrator->suppress_out = 1;
  if (cl.out_file) {
    s.batch_out_file = *cl.out_file;
    batch_options.user_out_file = *cl.out_file;
    s.output_place = Place{"", 0, 0, cl.out_place};
  }
  if (cl.ani_file) {
    s.animation->options.file = *cl.ani_file;
    s.animation->options.use_file = 1;
  }
  if (cl.equilibria) {
    batch_options.equilibria = *cl.equilibria;
    xpp::log(XPP_LOG_INFO, " Batch equilibria {:d} \n", batch_options.equilibria);
  }
  /* --dfdraw and --ncdraw win over the model's @ lines */
  for (const auto &[name, value] : cl.options) set_option(s, name, value, 1, NULL);
  s.model().record->command_line = std::move(cl);
}


/* the thread count and the colors the preferences file holds into s (a recording's replay
   keeps the user's preferences out of it, and reads none); a bad file leaves
   the automatic count and the default colors (the file's error is shown by the recent models' note,
   once). Called by the load (load_and_set_up), with the preferences the program's start read
   (read), or null to read the file now: a reload reads it again. */
void preferences_load(Session &s, const Result<prefs::Preferences> *read)
{
    if (files::serving_reads()) return;
    const Result<prefs::Preferences> p = read ? *read : prefs::load_prefs(*s.model().fs);
    if (p) {
        s.threads = p->threads;
        s.colors = p->colors;
    }
    /* the count stays automatic and the colors the defaults until the file is fixed (a `set` of the count refuses until then); said in the
       log, as --silent shows no recent models' note to carry the file's error */
    else xpp::log(XPP_LOG_WARN, "{}: the thread count stays automatic and the colors the defaults\n", p.error().text());
}

/* what a run needs once the model is loaded and its options hold their values (a load starts none): the stored rows' room (at most
   start_rows: an untouched row costs a page in every column of a large model), the delays', the adjoint's and the integrals' memory,
   the DAE solver's, the method's solver, and the array initial conditions by formula */
static void set_up_run(Session &s)
{
    s.delay->stab_flag = DELAY_NORMAL;
    s.data_store->rows = 0;
    s.numerics->storflag = false;
    s.numerics->inflag = false;
    xpp::init_range(s);
    const NumericsSettings &n = *s.numerics;
    const std::int64_t run_steps = xpp::step_count(s, xpp::solver_info(n.method), n.delta_t, n.total_time).value_or(DataStore::start_rows);
    const std::int64_t run_rows = run_steps / std::max(n.store_every, 1) + 2;
    s.data_store->allocate(static_cast<int>(std::min<std::int64_t>(DataStore::start_rows, run_rows)), s.model().neq + 1, n.storage_precision);
    xpp::chk_delay(s); /* check for delay allocation */
    xpp::alloc_h_stuff(s);
    xpp::alloc_v_memory(s); /* allocate stuff for volterra equations */
    xpp::init_dae_work(s);
    xpp::start_solver(s);
    xpp::arr_ic_start(s); /* take care of all predefined array ics */
}

/* everything both the batch run and an interactive front end do: options,
   the ODE file, numerics set-up. batch forces XPPBatch. */
static void load_and_set_up(xpp::Session &s, const CommandLine &cl, std::optional<AutoxScript> script, int batch,
                            const std::function<void(Session &)> &read, const Result<prefs::Preferences> *read_prefs)
{
    xpp::Model &m = s.model();
    program.interactive = 0;
    apply_command_line(s, cl);
    if (script) s.autox_state->script.script = std::move(script);
    if (batch) batch_options.enabled = 1; /* headless: always batch, even without --silent */

    s.options_applied = 0;
    read(s);
    /* the boundary conditions in use start as the model's */
    s.bcs = m.bcs;

    const OptionsSet mask = s.options_set;
    set_internopts(s, &mask);

    xpp::init_alloc_info(s);
    set_all_vals(s);
    set_up_run(s);
    /* a set is checked whole at the load (a bad one stops it) */
    if (const Result<> sets = check_intern_sets(s); !sets) model_failed(sets.error());
    /* run_set names one of the model's sets (W666): checked here, where the sets are read */
    for (const std::string &name : s.run_sets)
        if (!model_has_set(s.model(), name))
            model_failed(Error{"options", xpp::format("run_set \"{}\" is not one of the model's sets", name),
                               model_statement_place(s.model(), odex::Statement::Kind::Options, 0)});
    xpp::init_alloc_info(s);
    xpp::set_init_guess(s);
    update_all_ffts(s);
    if (s.autox_state->script.script)
        if (const Result<> checked = check_autox_script(s, *s.autox_state->script.script); !checked) model_failed(checked.error());
    init_autox_win(s);
    preferences_load(s, read_prefs);
    if (disc(m)) {
        const auto picked = pick_method(m, "Discrete", LoadSite::place());
        if (!picked) model_failed(picked.error());
        s.numerics->method = *picked;
    }
    program.version_major = static_cast<float>(MYSTR1);
    program.version_minor = static_cast<float>(MYSTR2);
    xpp::do_meth(s);
    xpp::set_delay(s);
    s.integrator->rhs = {my_rhs, &s};
    strip_saveqn(m);
    create_plot_list(s);
    init_all_graph(s);
    if (!cl.convert) load_option_animation(s);
}

namespace {

/* an .autox script opens with the model it names, its runs listed in the session (nothing computes) */
Result<CommandLine> script_command_line(FileSystem &fs, CommandLine cl, std::optional<AutoxScript> &script)
{
    if (!files::has_extension(cl.model_file, autox_script_extension)) return cl;
    auto loaded = autox_script_load(fs, cl.model_file, script_shape);
    if (!loaded) return std::unexpected(loaded.error());
    cl.model_file = include_path(cl.model_file, loaded->model);
    script = std::move(*loaded);
    return cl;
}

Result<CommandLine> animation_command_line(FileSystem &fs, CommandLine cl)
{
    if (files::has_extension(cl.model_file, ".anix")) {
        /* a .anix opens its animation with the model it names: the animation is the load's own (ani_file) */
        const std::string animation = cl.model_file;
        auto model = animation_model(fs, animation);
        if (!model) return std::unexpected(model.error());
        cl.model_file = *model;
        cl.ani_file = animation;
    }
    return cl;
}

/* load_model's work: the Session loaded, else what is wrong and where.
   The Load ends here, before its error is written: the messages it kept
   are written first */
Loaded in_model(FileSystem &fs, const std::function<void(Session &)> &work, bool commit)
{
    /* the parser and the set-up fill a fresh Model and Session, kept only
       when the load gets to the end: a failed one puts back those before */
    xpp::Load load(fs);
    /* the process-wide settings a load writes, put back when it fails */
    const XppProgram program_before = program;
    xpp::Model &m = load.model();
    try {
        m.load_dir = fs.working_dir();
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
        LoadSite::at(m.this_file);
        xpp::log(XPP_LOG_WARN, "states without init start at 0:{}\n", missing);
    }
    xpp_session_mark_clean(load.session()); /* what it loaded is not a change */
    /* the compile waits for the first computation (W713), except for a model that asks for the exact Jacobian: its absence is the load's error */
    if (load.session().numerics->jacobian!=JACOBIAN_EXACT) m.compile_pending=true;
    const auto compiled=m.compile_pending?Result<std::vector<Error>>(std::vector<Error>()):compile_loaded(load.session());
    if (!compiled) {
        program = program_before;
        return std::unexpected(compiled.error());
    }
    for (const Error &warning:*compiled) {
        LoadSite::at(warning.place.file,warning.place.line,warning.place.col);
        xpp::log(XPP_LOG_WARN,"{}\n",warning.what);
    }
    load.commit();
    return &load.session();
}

Loaded load_in(FileSystem &fs, const CommandLine &cl, int batch, const SavedModel *saved,
               const std::function<std::optional<Error>(Session &)> &check,
               const std::function<void(Session &)> &read, bool commit,
               const std::vector<std::string> &outside_paths = {}, const Result<prefs::Preferences> *read_prefs = nullptr,
               std::optional<AutoxScript> script = {}, const LoadServices *services = nullptr)
{
    return in_model(fs, [&](Session &s) {
        Model &m = s.model();
        m.outside_paths = outside_paths;
        if (services) m.services = *services;
        if (!m.services.command_exists) m.services.command_exists = command::command_known;
        if (!m.services.exports.begin_picture)
            m.services.exports = {data_formats(), std::span<const ImageFormat>(image_formats, n_image_formats), begin_picture, end_picture, write_picture};
        if (saved) {
            m.saved_in = saved->in;
            m.record->files = saved->files;
        }
        load_and_set_up(s, cl, std::move(script), batch, read, read_prefs);
        if (check)
            if (std::optional<Error> wrong = check(s)) model_failed(std::move(*wrong));
    }, commit);
}

} // namespace

Loaded load_model(FileSystem &fs, CommandLine cl, int batch, const SavedModel *saved,
                  const std::function<std::optional<Error>(Session &)> &check, const Result<prefs::Preferences> *read_prefs)
{
    auto resolved = animation_command_line(fs, std::move(cl));
    std::optional<AutoxScript> script;
    if (resolved) resolved = script_command_line(fs, std::move(*resolved), script);
    if (!resolved) {
        /* a file that names the model is the load's own: its fault is written once, as the model's own are (below) */
        if (batch) xpp::log(XPP_LOG_ERROR, "{}\n", resolved.error().text());
        return std::unexpected(resolved.error());
    }
    cl = std::move(*resolved);
    std::string file, converted;
    bool existing = false;
    std::vector<odex::Diagnostic> diagnostics;
    std::vector<std::string> outside_paths;
    /* the "Allow always" answers the load works with (xpp_prefs.h) */
    const LoadServices services = prefs::load_services(fs, read_prefs);
    Loaded prepared = in_model(fs, [&](Session &s) {
        apply_command_line(s, cl);
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
            Result<bool> opened = odex::open_ode(fs, program_loader, file, diagnostics, outside_paths, services);
            if (!opened) loaded = std::unexpected(opened.error());
            else {
                converted = odex::odex_name(file);
                existing = !*opened;
                /* Reload uses the .odex the converter wrote from now on */
                cl.arguments[static_cast<std::size_t>(cl.model_argument)] = converted;
                cl.model_file = converted;
            }
        }
        if (loaded) loaded = load_in(fs, cl, batch, saved, check, load_eqn, true, outside_paths, read_prefs, std::move(script), &services);
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

Result<> inspect_model(FileSystem &fs, const CommandLine &cl, const std::function<void(Session &)> &read,
                       const std::function<std::optional<Error>(Session &)> &check,
                       const SavedModel *saved, const LoadServices *services)
{
    const auto resolved = animation_command_line(fs, cl);
    Loaded inspected = resolved ? load_in(fs, *resolved, 1, saved, check, read, false, {}, nullptr, {}, services) : Loaded{std::unexpected(resolved.error())};
    if (!inspected) return std::unexpected(inspected.error());
    return {};
}

const ModelLoader program_loader{load_model, inspect_model, batch_start, load_eqn};

void batch_start(xpp::Session &s)
{
    init_data(s);
    init_all_graph(s);
    set_extra_graphs(s);
    set_colorization_stuff(s);
}

} // namespace xpp
