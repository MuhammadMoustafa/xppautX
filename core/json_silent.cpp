/* -silent's built-in script (W56): xppautX model.ode -silent is the
   protocol's own commands, the ones a page or a --script file sends,
   played through this front end (ui_json.cpp json_ui_silent) with its
   events going nowhere. The command line (-outfile, -internset, -qsets,
   -equil, -noout, -mkplot, ...) and the model's @ options (output=,
   range=, stoch=, postprocess=, ncdraw=, dfdraw=, plotfmt=) say which.

   Each step makes its lines when its turn comes, after the lines before
   it have run: an internal set may change any option, so what a step
   does is read from the session then, not when the script starts. The
   questions the commands ask (a menu, a file name, a form) are answered
   by the script's next lines, as in a --script file; a question the
   script does not expect stops it (exit 1, script_fail).

   For one run (no internal sets, or each set in turn):
     select the set          values internset (File/Get par set, the set
                             by its index: no menu key limits their number)
     -qsets/-qpars/-qics     values query (-dryrun: nothing is run)
     run                     key i, answer g (Initialconds/Go), or
                             answer r (Range) for @ range=1 or a
                             stochastic run
     @ postprocess           browser postprocess
     then, unless each run of a range wrote its own file:
     @ stoch=1/2             key u, key h, answer m or v (the Mean or
                             Variance of the runs), key Escape
     output.dat              browser write, what output, replace
     -mkplot                 key g, answer p (the PostScript form
                             answered with its values) or v, the file
   After every run: @ ncdraw=2 (key n, answer n; key n, answer s, the
   file nullclines.dat), @ dfdraw=4/5 (key d, answer d or s, the grid;
   dfield write dirfields.dat), -equil 0/1 (equilibrium write equil.dat). */
#include "ui_json_internal.h"
#include "model.h"
#include "session.h"
#include "comline.h"
#include "graf_par.h"
#include "xpp_batch.h"
#include "xpp_log.h"
#include <deque>
#include <memory>
#include <span>
#include <vector>

namespace xpp::json {

namespace {

class SilentScript {
public:
    SilentScript();
    std::optional<std::string> next();

private:
    std::deque<std::string> lines;               /* the step's lines not yet given */
    std::deque<std::function<void()>> steps;     /* the steps still to make */
    bool ran = false;                            /* the current run integrates */

    void add_run(int set);
    void line(std::string_view cmd, std::string_view field, std::string_view value);
    void key(std::string_view k) { line("key", "key", k); }
    void answer(std::string_view field, std::string_view value) { line("answer", field, value); }
    void answer_values(std::span<const std::string> values);
    void command(std::string_view cmd, std::string_view op, std::string_view name, std::string_view more = {});
};

/* the current window is a phase plane: its nullclines and direction
   field can be computed */
bool phase_plane()
{
    const xpp::Session &s = xpp::session();
    const auto &w = *s.plot_windows.current;
    return !(w.ThreeDFlag || w.TimeFlag || w.xv[0] == w.yv[0]);
}

/* {"cmd":cmd,"field":value} */
void SilentScript::line(std::string_view cmd, std::string_view field, std::string_view value)
{
    Buf b;
    buf_format(&b, "{{\"cmd\":\"{}\",\"{}\":", cmd, field);
    buf_str(&b, value);
    BUF_LIT(&b, "}");
    lines.push_back(std::move(b.s));
}

void SilentScript::answer_values(std::span<const std::string> values)
{
    Buf b;
    BUF_LIT(&b, "{\"cmd\":\"answer\",\"values\":");
    buf_str_array(&b, values);
    BUF_LIT(&b, "}");
    lines.push_back(std::move(b.s));
}

/* {"cmd":cmd,"op":op,"name":name<more>}: more, the fields after it */
void SilentScript::command(std::string_view cmd, std::string_view op, std::string_view name, std::string_view more)
{
    Buf b;
    buf_format(&b, "{{\"cmd\":\"{}\",\"op\":\"{}\",\"name\":", cmd, op);
    buf_str(&b, name);
    buf_add(&b, more.data(), more.size());
    BUF_LIT(&b, "}");
    lines.push_back(std::move(b.s));
}

/* the steps of one run: of internal set `set`, or of none (-1) */
void SilentScript::add_run(int set)
{
    steps.push_back([this, set] {
        ran = !dryrun && (set < 0 || batch_options.uses_intern_set(static_cast<std::size_t>(set)));
        if (set < 0) return;
        /* its data file is named after it without -outfile (its plots
           always are: batch_plot_name) */
        const std::string &name = xpp::model().intern_sets[static_cast<std::size_t>(set)].name;
        batch_options.out_file = batch_options.user_out_file.empty() ? name + ".dat" : batch_options.user_out_file;
        xpp::log(XPP_LOG_INFO, "out={}\n", batch_options.out_file);
        command("values", "internset", name, xpp::format(",\"index\":{:d}", set));
    });
    steps.push_back([this] {
        if (!dryrun) return;
        command("values", "query", batch_options.out_file,
                xpp::format(",\"sets\":{},\"pars\":{},\"ics\":{}", querysets, querypars, queryics));
    });
    steps.push_back([this] {
        if (!ran) return;
        const bool range = batch_options.range == 1 || xpp::session().stochastic.flag > 0;
        key("i");
        answer("key", range ? "r" : "g");
    });
    steps.push_back([this] {
        if (ran && xpp::session().histogram.post_process != 0) command("browser", "postprocess", "");
    });
    steps.push_back([this] {
        if (!ran) return;
        xpp::Session &s = xpp::session();
        /* a range that resets its storage wrote each run's file itself
           (integrate.cpp do_range, write_this_run) */
        if (!batch_options.range || s.integrator.range.reset == 0) {
            if (s.stochastic.flag == 1 || s.stochastic.flag == 2) {
                key("u");
                key("h");
                answer("key", s.stochastic.flag == 1 ? "m" : "v");
                key("Escape");
            }
            if (!s.integrator.suppress_out)
                command("browser", "write", batch_options.out_file, ",\"what\":\"output\",\"format\":\"dat\",\"replace\":1");
            if (s.integrator.make_plot_flag) {
                const std::string &format = s.plot_export.format;
                if (format == "ps") {
                    key("g");
                    answer("key", "p");
                    const std::vector<std::string> ps = {
                        xpp::format("{:d}", s.plot_export.color), xpp::format("{:d}", s.drawing.ps_port),
                        xpp::format("{:d}", s.plot_file.ps_font_size), s.plot_file.ps_font,
                        xpp::format("{:.17g}", s.plot_file.ps_lw)};
                    answer_values(ps);
                    answer("file", batch_plot_name(-1));
                } else if (format == "svg") {
                    key("g");
                    answer("key", "v");
                    answer("file", batch_plot_name(-1));
                }
            }
        }
        xpp::log(XPP_LOG_INFO, " Run complete ... \n");
    });
}

SilentScript::SilentScript()
{
    const std::size_t sets = xpp::model().intern_sets.size();
    if (sets == 0 || batch_options.intern_sets_used == 0) add_run(-1);
    else
        for (std::size_t i = 0; i < sets; i++) add_run(static_cast<int>(i));
    steps.push_back([this] {
        if (xpp::session().nullclines.nc_batch != 2 || !phase_plane()) return;
        key("n");
        answer("key", "n");
        key("n");
        answer("key", "s");
        answer("file", "nullclines.dat");
    });
    steps.push_back([this] {
        const xpp::Session &s = xpp::session();
        const int df = s.nullclines.df_batch;
        if ((df != 4 && df != 5) || !phase_plane() || s.nullclines.df_grid <= 1) return;
        key("d");
        answer("key", df == 4 ? "d" : "s"); /* arrows of one length, or scaled */
        answer("value", xpp::format("{:d}", s.nullclines.df_grid));
        command("dfield", "write", "dirfields.dat");
    });
    steps.push_back([this] {
        if (batch_options.equilibria < 0) return;
        command("equilibrium", "write", "equil.dat", batch_options.equilibria == 1 ? ",\"shoot\":1" : "");
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

std::function<std::optional<std::string>()> silent_script(void)
{
    auto script = std::make_shared<SilentScript>();
    return [script] { return script->next(); };
}

} // namespace xpp::json
