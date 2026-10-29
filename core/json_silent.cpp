/* --silent's built-in script (W56): xppautX model.ode --silent is the
   protocol's own commands, the ones a page sends,
   played through this front end (ui_json.cpp json_ui_silent) with its
   events going nowhere. The command line (--outfile, --internset, --qsets,
   --equil, --noout, --mkplot, ...) and the model's @ options (output=,
   range=, stoch=, postprocess=, ncdraw=, dfdraw=, plotfmt=) say which.

   Each step makes its lines when its turn comes, after the lines before
   it have run: an internal set may change any option, so what a step
   does is read from the session then, not when the script starts. The
   questions the commands ask (a menu, a file name, a form) are answered
   by the internal list's next lines; a question the
   script does not expect stops it (exit 1, silent_fail).

   For one run (no internal sets, or each set in turn):
     select the set          applyset, scope all (File/Apply set, the set
                             by its name: no menu key limits their number)
     --qsets/--qpars/--qics     values query (-dryrun: nothing is run)
     run                     run.initial, or run.range (the settings
                             the @ range options made) for @ range=1 or a
                             stochastic run
     @ postprocess           the analysis it names (analysis.histogram, ...)
     then, unless each run of a range wrote its own file:
     @ stoch=1/2             analysis.load_stochastic_mean or _variance (the Mean
                             or Variance of the runs)
     output.dat              browser write, what output, replace
     --mkplot                 key g, answer p (pdf) or v (svg), the picture parameters
                             form answered with its values, the file
   The core computes requested nullclines and direction fields after a run.
   After every run: --equil 0/1 (equilibrium write equil.dat). */
#include "ui_json_internal.h"
#include "model.h"
#include "session.h"
#include "comline.h"
#include "graf_par.h"
#include "histogram.h"
#include "image_format.h"
#include "xpp_batch.h"
#include "load_eqn.h"
#include "xpp_log.h"
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
    bool ran = false;                            /* the current run integrates */

    void add_run(int set);
    void line(std::string_view cmd, std::string_view field, std::string_view value);
    void key(std::string_view k) { line("key", "key", k); }
    void registry(std::string_view cmd) { lines.push_back(xpp::format("{{\"cmd\":\"{}\"}}", cmd)); }
    void answer(std::string_view field, std::string_view value) { line("answer", field, value); }
    void answer_values(std::span<const std::string> values);
    void command(std::string_view cmd, std::string_view op, std::string_view name, std::string_view more = {});
    void apply_set(std::string_view name);
    void post_process();
};

/* {"cmd":cmd,"field":value} */
void SilentScript::line(std::string_view cmd, std::string_view field, std::string_view value)
{
    std::string b;
    buf_format(b, "{{\"cmd\":\"{}\",\"{}\":", cmd, field);
    buf_str(b, value);
    if(cmd=="key")BUF_LIT(b, ",\"replace\":1");
    BUF_LIT(b, "}");
    lines.push_back(std::move(b));
}

void SilentScript::answer_values(std::span<const std::string> values)
{
    std::string b;
    BUF_LIT(b, "{\"cmd\":\"answer\",\"values\":");
    buf_str_array(b, values);
    BUF_LIT(b, "}");
    lines.push_back(std::move(b));
}

/* {"cmd":cmd,"op":op,"name":name<more>}: more, the fields after it */
void SilentScript::command(std::string_view cmd, std::string_view op, std::string_view name, std::string_view more)
{
    std::string b;
    buf_format(b, "{{\"cmd\":\"{}\",\"op\":\"{}\",\"name\":", cmd, op);
    buf_str(b, name);
    BUF_LIT(b, ",\"replace\":1");
    buf_add(b, more.data(), more.size());
    BUF_LIT(b, "}");
    lines.push_back(std::move(b));
}

/* the model's @ postprocess of the run's data: the analysis the option names, with the settings the model gave, and only the
   time and its first column written after it (the postprocess kinds are the option's own numbers) */
void SilentScript::post_process()
{
    constexpr int HISTOGRAM = 1, FOURIER = 2, POWER = 3, FIRST_SPECTRUM = 4, LAST_SPECTRUM = 6, HISTOGRAM_2D = 7;
    const int kind = session.histogram.post_process;
    if (!ran || kind == 0) return;
    session.plot_list.assign({0, 1});
    if (kind == HISTOGRAM) registry("analysis.histogram");
    else if (kind == FOURIER) registry("analysis.fourier_series");
    else if (kind == POWER) registry("analysis.power_spectrum");
    else if (kind == HISTOGRAM_2D) registry("analysis.histogram_2d");
    else if (kind >= FIRST_SPECTRUM && kind <= LAST_SPECTRUM)
        lines.push_back(xpp::format("{{\"cmd\":\"analysis.spectral_density\",\"type\":\"{}\"}}", xpp::spectrum_type_names[static_cast<std::size_t>(kind - FIRST_SPECTRUM)]));
}

/* {"cmd":"applyset","set":name,"scope":"all"}: the whole set */
void SilentScript::apply_set(std::string_view name)
{
    std::string b;
    BUF_LIT(b, "{\"cmd\":\"applyset\",\"set\":");
    buf_str(b, name);
    buf_format(b, ",\"scope\":\"{}\"}}", xpp::set_scope::all);
    lines.push_back(std::move(b));
}

/* the steps of one run: of internal set `set`, or of none (-1) */
void SilentScript::add_run(int set)
{
    steps.push_back([this, set] {
        ran = !session.model().command_line.dry_run() && (set < 0 || batch_options.uses_intern_set(static_cast<std::size_t>(set)));
        if (set < 0) return;
        /* its data file is named after it without --outfile (its plots
           always are: batch_plot_name) */
        const std::string &name = session.model().intern_sets[static_cast<std::size_t>(set)].name;
        session.batch_out_file = batch_options.user_out_file.empty() ? name + ".dat" : batch_options.user_out_file;
        xpp::log(XPP_LOG_INFO, "out={}\n", session.batch_out_file);
        apply_set(name);
    });
    steps.push_back([this] {
        const CommandLine &cl = session.model().command_line;
        if (!cl.dry_run()) return;
        command("values", "query", session.batch_out_file,
                xpp::format(",\"sets\":{},\"pars\":{},\"ics\":{}", int{cl.query_sets}, int{cl.query_pars}, int{cl.query_ics}));
    });
    steps.push_back([this] {
        if (!ran) return;
        const bool range = session.integrator.range.write_files || session.stochastic.flag > 0;
        registry(range ? "run.range" : "run.initial");
    });
    steps.push_back([this] { post_process(); });
    steps.push_back([this] {
        if (!ran) return;
        const xpp::Session &s = session;
        /* a range that resets its storage wrote each run's file itself
           (integrate.cpp do_range, write_this_run) */
        if (!s.integrator.range.write_files || s.integrator.range.reset == 0) {
            if (s.stochastic.flag == 1 || s.stochastic.flag == 2)
                registry(s.stochastic.flag == 1 ? "analysis.load_stochastic_mean" : "analysis.load_stochastic_variance");
            if (!s.integrator.suppress_out)
                command("browser", "write", session.batch_out_file, ",\"what\":\"output\",\"format\":\"dat\"");
            if (s.integrator.make_plot_flag) {
                const xpp::ImageFormat *format=xpp::find_image_format_by_extension(s.plot_export.format);
                if (!format || !format->plot_key) {
                    xpp::command_error("mkplot",xpp::format("Unsupported batch picture format {}",s.plot_export.format));
                    return;
                }
                key("g");
                answer("key", format->plot_key);
                answer_values(xpp::picture_parameter_values(s));
                answer("file", batch_plot_name(s, -1));
            }
        }
        xpp::log(XPP_LOG_INFO, " Run complete ... \n");
    });
}

SilentScript::SilentScript(xpp::Session &s) : session(s)
{
    const std::size_t sets = s.model().intern_sets.size();
    if (sets == 0 || batch_options.intern_sets_used == 0) add_run(-1);
    else
        for (std::size_t i = 0; i < sets; i++) add_run(static_cast<int>(i));
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
