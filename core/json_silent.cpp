/* --silent's built-in script (W56): xppautX model.ode --silent is the
   protocol's own commands, the ones a page sends,
   played through this front end (ui_json.cpp json_ui_silent) with its
   events going nowhere. The command line (--outfile, --equil, --noout, ...), the model's run_set options (which sets run) and the model's @ options (output=,
   range=, stoch=, postprocess=, ncdraw=, dfdraw=, picture_file=) say which.

   Each step makes its lines when its turn comes, after the lines before
   it have run: an internal set may change any option, so what a step
   does is read from the session then, not when the script starts. The
   questions the commands ask (a menu, a file name, a form) are answered
   by the internal list's next lines; a question the
   script does not expect stops it (exit 1, silent_fail).

   For one run (no internal sets, or each set in turn):
     select the set          set.apply, scope all (the set by its name)
     run                     run.initial, run.range (the settings
                             the sweep options made) for a sweep that writes files, or
                             stochastic.compute (the sweep's settings) for
                             an after_run of mean_of_runs or variance_of_runs
     after_run               the analysis it names (histogram.compute, ...), or
                             stochastic.show_mean or _variance
     then, unless each run of a range wrote its own file:
     output.dat              the output columns' data file, replacing it (save_data_file)
     picture_file            plot.export_pdf or plot.export_svg, by the extension of output.picture_file
   The core computes requested nullclines and direction fields after a run.
   After every run: --equil 0/1 (equilibrium write equil.dat). */
#include "ui_json_internal.h"
#include "model.h"
#include "session.h"
#include "comline.h"
#include "browse.h"
#include "graf_par.h"
#include "histogram.h"
#include "image_format.h"
#include "lunch-new.h"
#include "xpp_batch.h"
#include "load_eqn.h"
#include "xpp_log.h"
#include <algorithm>
#include <deque>
#include <memory>
#include <span>
#include <vector>

namespace xpp::json {

namespace {

class SilentScript {
public:
    explicit SilentScript(xpp::Session &s);
    std::optional<std::string> next();

private:
    xpp::Session &session; /* the session --silent runs (no other is ever loaded) */
    std::deque<std::string> lines;               /* the step's lines not yet given */
    std::deque<std::function<void()>> steps;     /* the steps still to make */

    void add_run(int set);
    void registry(std::string_view cmd) { lines.push_back(xpp::format("{{\"cmd\":\"{}\"}}", cmd)); }
    void apply_set(std::string_view name);
    void post_process();
    /* the run's data into batch_out_file (the stochastic mean or variance first); once, until the next run */
    void save_output();
    /* a range that resets its storage wrote each run's file itself (integrate.cpp do_range, write_this_run) */
    bool writes_output() const { return !session.integrator.range.write_files || session.integrator.range.reset == 0; }
    bool trajectory_saved = false;
    /* the run's data were replaced by an after_run (the analysis, or the mean or the variance of the runs) */
    bool after_run_analysed() const { return session.histogram.post_process != xpp::AfterRun::none; }
    /* the files --silent writes itself replace what is there, unasked, as its key lines' "replace":1 said */
    void replace_files() { xpp::json::session.save_replace = SAVE_REPLACE; }
};

/* the model's after_run of the run's data: the analysis the option names, with the settings the model gave, and only the
   time and its first column written after it; the mean or the variance of the runs of a stochastic sweep replaces the data */
void SilentScript::post_process()
{
    using xpp::AfterRun;
    const AfterRun kind = session.histogram.post_process;
    if (kind == AfterRun::none) return;
    if (kind == AfterRun::mean_of_runs) return registry("stochastic.show_mean");
    if (kind == AfterRun::variance_of_runs) return registry("stochastic.show_variance");
    session.plot_list.assign({0, 1});
    switch (kind) {
    case AfterRun::histogram: registry("histogram.compute"); break;
    case AfterRun::histogram_2d: registry("histogram.compute_2d"); break;
    case AfterRun::fourier_series: registry("spectrum.fourier_series"); break;
    case AfterRun::power_spectrum: registry("spectrum.power"); break;
    default:
        lines.push_back(xpp::format("{{\"cmd\":\"spectrum.density\",\"type\":\"{}\"}}", xpp::after_run_names[static_cast<std::size_t>(kind)]));
    }
}

void SilentScript::save_output()
{
    const xpp::Session &s = session;
    trajectory_saved = true;
    if (s.integrator.suppress_out) return;
    replace_files();
    xpp::ok_or_show(xpp::save_data_file(s, xpp::DataTarget::output, "dat", s.batch_out_file));
}

/* {"cmd":"set.apply","name":name,"scope":"all"}: the whole set */
void SilentScript::apply_set(std::string_view name)
{
    std::string b;
    BUF_LIT(b, "{\"cmd\":\"set.apply\",\"name\":");
    buf_str(b, name);
    buf_format(b, ",\"scope\":\"{}\"}}", xpp::set_scope::all);
    lines.push_back(std::move(b));
}

/* the file a set's run writes: the set's name and the extension of the kind of file (data_file's and picture_file's one rule) */
static std::string set_file(std::string_view set, std::string_view extension) { return xpp::format("{}.{}", set, extension); }

/* the steps of one run: of internal set `set`, or of none (-1) */
void SilentScript::add_run(int set)
{
    steps.push_back([this, set] {
        if (set < 0) return;
        /* its data file is named after it without --outfile, its picture always is (set_file) */
        const std::string &name = session.model().intern_sets[static_cast<std::size_t>(set)].name;
        session.batch_out_file = batch_options.user_out_file.empty() ? set_file(name, "dat") : batch_options.user_out_file;
        if (const xpp::ImageFormat *format = xpp::picture_format_of_file(session.picture_file)) session.picture_file = set_file(name, format->extension);
        xpp::log(XPP_LOG_INFO, "out={}\n", session.batch_out_file);
        apply_set(name);
    });
    steps.push_back([this] {
        const xpp::AfterRun after = session.histogram.post_process;
        if (after == xpp::AfterRun::mean_of_runs || after == xpp::AfterRun::variance_of_runs) registry("stochastic.compute");
        else registry(session.integrator.range.write_files ? "run.range" : "run.initial");
    });
    /* with after_run_file the trajectory is written first, so the analysis does not replace it */
    steps.push_back([this] {
        if (writes_output() && !session.after_run_file.empty() && after_run_analysed()) save_output();
    });
    steps.push_back([this] { post_process(); });
    steps.push_back([this] {
        if (!writes_output() || session.after_run_file.empty() || !after_run_analysed()) return;
        replace_files();
        xpp::ok_or_show(xpp::save_data_file(session, xpp::DataTarget::output, "", session.after_run_file));
    });
    steps.push_back([this] {
        const xpp::Session &s = session;
        if (writes_output()) {
            if (!trajectory_saved) save_output();
            if (const xpp::ImageFormat *format = xpp::picture_format_of_file(s.picture_file)) {
                std::string line = xpp::format("{{\"cmd\":\"plot.export_{}\",\"file\":", format->extension);
                buf_str(line, s.picture_file);
                line += '}';
                lines.push_back(std::move(line));
            }
        }
        trajectory_saved = false;
        xpp::log(XPP_LOG_INFO, " Run complete ... \n");
    });
}

/* run_set names the sets a batch run runs */
static bool runs_set(const xpp::Session &s, const std::string &name)
{
    return std::find(s.run_sets.begin(), s.run_sets.end(), name) != s.run_sets.end();
}

SilentScript::SilentScript(xpp::Session &s) : session(s)
{
    const std::size_t sets = s.model().intern_sets.size();
    /* no run_set: one run with no set, as a batch run always was; run_set: each named set in turn */
    if (sets == 0 || s.run_sets.empty()) add_run(-1);
    else
        for (std::size_t i = 0; i < sets; i++)
            if (runs_set(s, s.model().intern_sets[i].name)) add_run(static_cast<int>(i));
    steps.push_back([this] {
        if (batch_options.equilibria < 0) return;
        lines.push_back(xpp::format("{{\"cmd\":\"equilibrium.export\",\"file\":\"equil.dat\",\"confirm\":true,\"shoot\":{}}}",batch_options.equilibria==1));
    });
}

std::optional<std::string> SilentScript::next()
{
    while (lines.empty() && !steps.empty()) {
        std::function<void()> step = std::move(steps.front());
        steps.pop_front();
        step();
    }
    if (lines.empty()) return std::nullopt;
    std::string line = std::move(lines.front());
    lines.pop_front();
    return line;
}

} // namespace

std::function<std::optional<std::string>()> silent_script(xpp::Session &s)
{
    auto script = std::make_shared<SilentScript>(s);
    return [script] { return script->next(); };
}

} // namespace xpp::json
