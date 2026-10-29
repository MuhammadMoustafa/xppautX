/* The analysis resource's registry rows (W525): the stochastic runs and what is made of the stored data
   (histograms, correlations, spectra, Fourier series, statistics, curve fits, Liapunov exponents), the adjoint and
   H function, the Poincare map, the phase space's torus and the coloring of an orbit. Each handler sets the
   settings it is given over the ones the session has (an argument left out keeps them) and calls the one core
   function the Stochastic, Adjoint, Poincare map, Phase space and Color code menus' items call after their
   questions (histogram.h, markov.h, adj2.h, do_fit.h, numerics.h, torus.h): one behaviour, whichever front
   end asks. */
#include "browse.h"
#include "command_def.h"
#include "command_graph.h"
#include "command_range.h"
#include "do_fit.h"
#include "adj2.h"
#include "histogram.h"
#include "markov.h"
#include "model_options.h"
#include "numerics.h"
#include "session.h"
#include "torus.h"
#include "xpp_util.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace xpp::command {
namespace {

constexpr const char *WINDOW_KEY = "window";
constexpr char LOADED[] = "loaded", RELOADED[] = "reloaded", COMPUTED[] = "computed";
constexpr long long INT_MOST = std::numeric_limits<int>::max();

/* the spectra's, correlations', Poincare map's and coloring's words; a word's position is the number the core keeps */
constexpr auto SPECTRUM_TYPES = c_names(spectrum_type_names);
constexpr std::array SPECTRUM_WINDOWS = {"square", "parzen", "hamming", "bartlett", "hann"};
constexpr std::array CORRELATION_METHODS = {"direct", "fft"};
constexpr std::array POINCARE_TYPES = {"none", "section", "max_min", "period"};
constexpr std::array POINCARE_MAPS = {PoincareMap::none, PoincareMap::section, PoincareMap::max, PoincareMap::period};
constexpr auto DIRECTIONS = c_names(section_sign_names);
constexpr std::array TORUS_MODES = {"all", "none", "choose"};
constexpr std::array COLOR_MODES = {"none", "velocity", "variable"};

template <std::size_t N> std::size_t position_of(const std::array<const char *, N> &words, std::string_view word)
{
    return static_cast<std::size_t>(std::ranges::find(words, word) - words.begin());
}

Result<std::string> seed_random(Session &s, const Command &, const Arguments &args, const Place &)
{
    if (const Result<> set = set_random_seed(s, static_cast<int>(args.get<long long>("seed"))); !set) return std::unexpected<Error>(set.error());
    return ResultObject().integer("seed", s.numerics.rand_seed).done();
}

Result<std::string> stochastic_integrate_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    /* the runs are the Range settings' steps plus one: the sweep takes a run at each of the steps and at the start */
    if (const auto trials = args.optional<long long>("trials")) s.integrator.range.steps = static_cast<int>(*trials - 1);
    const Result<int> runs = stochastic_integrate(s);
    if (!runs) return std::unexpected<Error>(runs.error());
    return ResultObject().integer("trials_completed", *runs).done();
}

/* the stored data of the last run in the data browser again */
Result<std::string> load_data(Session &s, const Command &, const Arguments &, const Place &)
{
    data_back(s);
    return ResultObject().boolean(LOADED, true).done();
}

/* a command with no arguments whose core function either does what it says or says why not: {Key: true} */
template <Result<> (*Do)(Session &), const char *Key> Result<std::string> done_with(Session &s, const Command &, const Arguments &, const Place &)
{
    if (const Result<> done = Do(s); !done) return std::unexpected<Error>(done.error());
    return ResultObject().boolean(Key, true).done();
}

Result<std::string> histogram_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    HIST_INFO &info = s.histogram.info;
    if (const auto column = args.optional<std::string>("column")) info.col = variable_column(s, *column);
    if (const auto bins = args.optional<long long>("bins")) info.nbins = static_cast<int>(*bins);
    if (const auto low = args.optional<double>("low")) info.xlo = *low;
    if (const auto high = args.optional<double>("high")) info.xhi = *high;
    if (const auto condition = args.optional<std::string>("condition")) info.cond = *condition;
    const Result<HistogramCounts> made = compute_hist(s);
    if (!made) return std::unexpected<Error>(made.error());
    return ResultObject().integer("bins", made->bins).integer("total_samples", made->samples).done();
}

Result<std::string> histogram_2d_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    HIST_INFO &info = s.histogram.info;
    if (const auto column = args.optional<std::string>("column1")) info.col = variable_column(s, *column);
    if (const auto column = args.optional<std::string>("column2")) info.col2 = variable_column(s, *column);
    if (const auto bins = args.optional<long long>("bins1")) info.nbins = static_cast<int>(*bins);
    if (const auto bins = args.optional<long long>("bins2")) info.nbins2 = static_cast<int>(*bins);
    if (const auto low = args.optional<double>("low1")) info.xlo = *low;
    if (const auto high = args.optional<double>("high1")) info.xhi = *high;
    if (const auto low = args.optional<double>("low2")) info.ylo = *low;
    if (const auto high = args.optional<double>("high2")) info.yhi = *high;
    if (const Result<> made = new_2d_hist(s); !made) return std::unexpected<Error>(made.error());
    return ResultObject().integer("bins_x", info.nbins).integer("bins_y", info.nbins2).done();
}

/* the Fourier series and the power spectrum of one column */
template <Result<int> (*Transform)(Session &), const char *Count>
Result<std::string> transform_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    if (const auto column = args.optional<std::string>("column")) s.histogram.spec_col = variable_column(s, *column);
    const Result<int> made = Transform(s);
    if (!made) return std::unexpected<Error>(made.error());
    return ResultObject().integer(Count, *made).done();
}
constexpr char HARMONICS[] = "harmonics", FREQUENCIES[] = "frequencies";

Result<std::string> statistics_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    if (const auto column = args.optional<std::string>("column")) s.histogram.info.col = variable_column(s, *column);
    const Result<ColumnStatistics> found = column_mean(s);
    if (!found) return std::unexpected<Error>(found.error());
    return ResultObject().real("mean", found->mean).real("variance", found->variance).real("std_dev", std::sqrt(found->variance)).done();
}

Result<std::string> autocorrelation_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    HIST_INFO &info = s.histogram.info;
    if (const auto column = args.optional<std::string>("column")) info.col = variable_column(s, *column);
    if (const auto bins = args.optional<long long>("bins")) info.nbins = static_cast<int>(*bins);
    if (const auto low = args.optional<double>("low")) info.xlo = *low;
    if (const auto high = args.optional<double>("high")) info.xhi = *high;
    const Result<int> made = compute_stacor(s);
    if (!made) return std::unexpected<Error>(made.error());
    return ResultObject().integer("lags_computed", *made).done();
}

Result<std::string> cross_correlation_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    HIST_INFO &info = s.histogram.info;
    if (const auto column = args.optional<std::string>("column1")) info.col = variable_column(s, *column);
    if (const auto column = args.optional<std::string>("column2")) info.col2 = variable_column(s, *column);
    if (const auto lags = args.optional<long long>("lags")) info.nbins = static_cast<int>(*lags);
    if (const auto method = args.optional<std::string>("method")) info.fftc = static_cast<int>(position_of(CORRELATION_METHODS, *method));
    const Result<int> made = compute_correl(s);
    if (!made) return std::unexpected<Error>(made.error());
    return ResultObject().integer("lags_computed", *made).done();
}

Result<std::string> spectral_density_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    HistogramState &h = s.histogram;
    if (const auto type = args.optional<std::string>("type")) h.spec_type = static_cast<int>(position_of(SPECTRUM_TYPES, *type));
    if (const auto column = args.optional<std::string>("column")) h.spec_col = variable_column(s, *column);
    if (const auto column = args.optional<std::string>("column2")) h.spec_col2 = variable_column(s, *column);
    if (const auto length = args.optional<long long>("window_length")) h.spec_wid = static_cast<int>(*length);
    if (const auto window = args.optional<std::string>("window")) h.spec_win = static_cast<int>(position_of(SPECTRUM_WINDOWS, *window));
    const Result<int> made = compute_sd(s);
    if (!made) return std::unexpected<Error>(made.error());
    return ResultObject().integer("points", *made).done();
}

/* the names, blank separated, a fit's list holds */
std::string joined(const std::vector<std::string> &names)
{
    std::string text;
    for (const std::string &name : names) text += (text.empty() ? "" : " ") + name;
    return text;
}

Result<std::string> fit_curve(Session &s, const Command &, const Arguments &args, const Place &)
{
    FitInfo &fit = s.fit;
    fit.varlist = joined(args.list<std::string>("variables"));
    fit.parlist1 = joined(args.list<std::string>("parameters"));
    fit.parlist2.clear();
    if (args.has("data_columns")) {
        std::string columns;
        for (const long long column : args.list<long long>("data_columns")) columns += (columns.empty() ? "" : " ") + std::to_string(column);
        fit.collist = columns;
    }
    if (const auto file = args.optional<std::string>("file")) fit.file = *file;
    if (const auto tolerance = args.optional<double>("tolerance")) fit.tol = *tolerance;
    if (const auto points = args.optional<long long>("points")) fit.npts = static_cast<int>(*points);
    if (const auto columns = args.optional<long long>("columns")) fit.dim = static_cast<int>(*columns);
    if (const auto epsilon = args.optional<double>("epsilon")) fit.eps = *epsilon;
    if (const auto iterations = args.optional<long long>("max_iterations")) fit.maxiter = static_cast<int>(*iterations);
    const Result<FitOutcome> fitted = test_fit(s);
    if (!fitted) return std::unexpected<Error>(fitted.error());
    std::string parameters = "{";
    for (const auto &[name, value] : fitted->parameters) {
        if (parameters.size() > 1) parameters += ',';
        json_append_string(parameters, name);
        parameters += ':';
        json::json_append_number_shortest(parameters, value);
    }
    return ResultObject().boolean("converged", fitted->result.end == FitEnd::Converged).raw("parameters", parameters + '}').done();
}

Result<std::string> lyapunov_handler(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const bool over_range = args.optional<bool>("range_over_parameters").value_or(false);
    if (!over_range)
        for (const Argument &arg : RUN_RANGE_ARGUMENTS)
            if (args.has(arg.name)) return fail(command.name, xpp::format("\"{}\" is a setting of the sweep: give range_over_parameters", arg.name), place);
    if (over_range) set_range_settings(s.integrator.range, args);
    const Result<LyapunovResult> found = lyapunov_exponent(s, over_range);
    if (!found) return std::unexpected<Error>(found.error());
    return ResultObject().real("max_lyapunov_exponent", found->exponent).integer("runs_completed", found->runs).done();
}

Result<std::string> poincare_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    const PoincareMap map = POINCARE_MAPS[position_of(POINCARE_TYPES, args.get<std::string>("type"))];
    std::optional<int> variable;
    if (const auto name = args.optional<std::string>("variable")) variable = variable_column(s, *name);
    std::optional<SectionSign> direction;
    if (const auto word = args.optional<std::string>("direction")) {
        const auto at = std::ranges::find(section_sign_names, *word);
        direction = static_cast<SectionSign>(static_cast<int>(at - section_sign_names.begin()) - 1);
    }
    if (const Result<> set = set_poincare_map(s, map, variable, args.optional<double>("section"), direction, args.optional<bool>("stop_on_section")); !set)
        return std::unexpected<Error>(set.error());
    return ResultObject().boolean("poincare_configured", s.numerics.poincare_map != PoincareMap::none).done();
}

Result<std::string> torus_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    const TorusMode mode = static_cast<TorusMode>(position_of(TORUS_MODES, args.get<std::string>("mode")));
    std::optional<std::vector<int>> chosen;
    if (args.has("variables")) {
        chosen.emplace();
        for (const std::string &name : args.list<std::string>("variables")) chosen->push_back(variable_column(s, name) - 1);
    }
    if (const Result<> set = set_torus(s, mode, args.optional<double>("period"), chosen); !set) return std::unexpected<Error>(set.error());
    std::string names = "[";
    for (int i = 0; i < s.model().neq; ++i) {
        if (s.itor[static_cast<std::size_t>(i)] != 1 || !s.numerics.torus) continue;
        if (names.size() > 1) names += ',';
        json_append_string(names, s.model().uvar_names[static_cast<std::size_t>(i)]);
    }
    return ResultObject().raw("torus_variables", names + ']').done();
}

Result<std::string> color_coding_handler(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<int> graph = graph_of_command(s, command, args, place);
    if (!graph) return std::unexpected<Error>(graph.error());
    const ActiveGraph active(s, *graph);
    const ColorBy by = static_cast<ColorBy>(position_of(COLOR_MODES, args.get<std::string>("mode")));
    const int variable = args.has("variable") ? variable_column(s, args.get<std::string>("variable")) : s.plot_windows.current->ColorValue;
    std::optional<std::pair<double, double>> limits;
    if (args.has("low")) limits = std::pair{args.get<double>("low"), args.get<double>("high")};
    if (const Result<> set = set_color_coding(s, by, variable, limits); !set) return std::unexpected<Error>(set.error());
    return ResultObject().text("color_coding", COLOR_MODES[static_cast<std::size_t>(s.plot_windows.current->ColorFlag)]).done();
}

Result<std::string> adjoint_interaction(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    for (const auto &[name, formula] : args.map<std::string>("couplings")) {
        const int variable = variable_column(s, name) - 1;
        if (variable < 0 || variable >= s.model().node)
            return fail(command.name, xpp::format("\"{}\" is not a differential equation of the model: only those have a coupling", name), place);
        s.adjoint.coup_string[static_cast<std::size_t>(variable)] = formula;
    }
    if (const Result<> made = new_h_fun(s, true); !made) return std::unexpected<Error>(made.error());
    return ResultObject().boolean(COMPUTED, true).done();
}

Result<std::string> adjoint_configure(Session &s, const Command &, const Arguments &args, const Place &)
{
    const int iterations = static_cast<int>(args.optional<long long>("max_iterates").value_or(s.adjoint.maxit));
    if (const Result<> set = set_adjoint_parameters(s, iterations, args.optional<double>("tolerance").value_or(s.adjoint.err)); !set)
        return std::unexpected<Error>(set.error());
    return ResultObject().boolean("updated", true).done();
}

Result<std::string> adjoint_range(Session &s, const Command &, const Arguments &args, const Place &)
{
    if (const auto enabled = args.optional<bool>("enabled")) s.integrator.adj_range = *enabled ? 1 : 0;
    return ResultObject().boolean("adjoint_range", s.integrator.adj_range == 1).done();
}

constexpr Argument COLUMN = {"column", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column};
constexpr Argument COLUMN1 = {"column1", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column};
constexpr Argument COLUMN2 = {"column2", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column};
constexpr Argument BINS = {"bins", Type::integer, false, MAX_TEXT_BYTES, {}, constant(2), constant(option_limit::histogram_bins)};
constexpr Argument LOW = {"low", Type::real, false};
constexpr Argument HIGH = {"high", Type::real, false};

constexpr std::array SEED_ARGUMENTS = {Argument{"seed", Type::integer, true, MAX_TEXT_BYTES, {}, constant(0), constant(static_cast<double>(INT_MOST))}};
constexpr std::array TRIALS_ARGUMENTS = {Argument{"trials", Type::integer, false, MAX_TEXT_BYTES, {}, constant(2), constant(static_cast<double>(INT_MOST))}};
constexpr std::array HISTOGRAM_ARGUMENTS = {COLUMN, BINS, LOW, HIGH, Argument{"condition", Type::text, false}};
constexpr std::array HISTOGRAM_2D_ARGUMENTS = {
    COLUMN1, COLUMN2,
    Argument{"bins1", Type::integer, false, MAX_TEXT_BYTES, {}, constant(2), constant(option_limit::histogram_bins)},
    Argument{"bins2", Type::integer, false, MAX_TEXT_BYTES, {}, constant(2), constant(option_limit::histogram_bins)},
    Argument{"low1", Type::real, false}, Argument{"high1", Type::real, false}, Argument{"low2", Type::real, false}, Argument{"high2", Type::real, false}};
constexpr std::array COLUMN_ARGUMENTS = {COLUMN};
constexpr std::array AUTOCORRELATION_ARGUMENTS = {COLUMN, BINS, LOW, HIGH};
constexpr std::array CROSS_ARGUMENTS = {
    COLUMN1, COLUMN2, Argument{"lags", Type::integer, false, MAX_TEXT_BYTES, {}, constant(2), constant(option_limit::histogram_bins)},
    Argument{"method", Type::choice, false, MAX_TEXT_BYTES, CORRELATION_METHODS}};
constexpr std::array SPECTRAL_ARGUMENTS = {
    Argument{"type", Type::choice, false, MAX_TEXT_BYTES, SPECTRUM_TYPES}, COLUMN, COLUMN2,
    Argument{"window_length", Type::integer, false, MAX_TEXT_BYTES, {}, constant(2), constant(static_cast<double>(INT_MOST))},
    Argument{"window", Type::choice, false, MAX_TEXT_BYTES, SPECTRUM_WINDOWS}};
constexpr std::array FIT_ARGUMENTS = {
    Argument{"variables", Type::list, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::variable, Type::model_name},
    Argument{"parameters", Type::list, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::either, Type::model_name},
    Argument{"tolerance", Type::real, false, MAX_TEXT_BYTES, {}, constant(std::numeric_limits<double>::min())},
    Argument{"points", Type::integer, false, MAX_TEXT_BYTES, {}, constant(2), constant(static_cast<double>(INT_MOST))},
    Argument{"columns", Type::integer, false, MAX_TEXT_BYTES, {}, constant(2), constant(static_cast<double>(INT_MOST))},
    Argument{"data_columns", Type::list, false, MAX_TEXT_BYTES, {}, constant(2), constant(static_cast<double>(INT_MOST)), NameSet::none, Type::integer},
    Argument{"epsilon", Type::real, false, MAX_TEXT_BYTES, {}, constant(std::numeric_limits<double>::min())},
    Argument{"max_iterations", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(static_cast<double>(INT_MOST))},
    Argument{"file", Type::file, false}};
constexpr std::array LYAPUNOV_OWN = {Argument{"range_over_parameters", Type::boolean, false}};
constexpr auto LYAPUNOV_ARGUMENTS = join(LYAPUNOV_OWN, RUN_RANGE_ARGUMENTS);
constexpr std::array POINCARE_ARGUMENTS = {
    Argument{"type", Type::choice, true, MAX_TEXT_BYTES, POINCARE_TYPES},
    Argument{"variable", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column},
    Argument{"section", Type::real, false},
    Argument{"direction", Type::choice, false, MAX_TEXT_BYTES, DIRECTIONS},
    Argument{"stop_on_section", Type::boolean, false}};
constexpr std::array TORUS_ARGUMENTS = {
    Argument{"mode", Type::choice, true, MAX_TEXT_BYTES, TORUS_MODES},
    Argument{"variables", Type::list, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::variable, Type::model_name},
    Argument{"period", Type::real, false, MAX_TEXT_BYTES, {}, constant(std::numeric_limits<double>::min())}};
constexpr std::array COLOR_ARGUMENTS = {
    Argument{"mode", Type::choice, true, MAX_TEXT_BYTES, COLOR_MODES},
    Argument{"variable", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column},
    Argument{"low", Type::real, false}, Argument{"high", Type::real, false}, Argument{WINDOW_KEY, Type::window, false}};
constexpr std::array COUPLING_ARGUMENTS = {Argument{"couplings", Type::map, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::variable, Type::expression}};
constexpr std::array ADJOINT_CONFIGURE_ARGUMENTS = {
    Argument{"max_iterates", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(static_cast<double>(INT_MOST))},
    Argument{"tolerance", Type::real, false, MAX_TEXT_BYTES, {}, constant(std::numeric_limits<double>::min())}};
constexpr std::array ENABLED_ARGUMENTS = {Argument{"enabled", Type::boolean, false}};

constexpr std::array COLOR_TOGETHER = {"low", "high"};
constexpr std::array COLOR_GROUPS = {Group{GroupRule::together, COLOR_TOGETHER}};

constexpr std::array SEED_RESULTS = {ResultField{"seed", "integer"}};
constexpr std::array TRIALS_RESULTS = {ResultField{"trials_completed", "integer"}};
constexpr std::array LOADED_RESULTS = {ResultField{"loaded", "boolean"}};
constexpr std::array HISTOGRAM_RESULTS = {ResultField{"bins", "integer"}, ResultField{"total_samples", "integer"}};
constexpr std::array RELOADED_RESULTS = {ResultField{"reloaded", "boolean"}};
constexpr std::array HISTOGRAM_2D_RESULTS = {ResultField{"bins_x", "integer"}, ResultField{"bins_y", "integer"}};
constexpr std::array HARMONICS_RESULTS = {ResultField{HARMONICS, "integer"}};
constexpr std::array FREQUENCIES_RESULTS = {ResultField{FREQUENCIES, "integer"}};
constexpr std::array FIT_RESULTS = {ResultField{"converged", "boolean"}, ResultField{"parameters", "map[name, real]"}};
constexpr std::array STATISTICS_RESULTS = {ResultField{"mean", "real"}, ResultField{"variance", "real"}, ResultField{"std_dev", "real"}};
constexpr std::array LYAPUNOV_RESULTS = {ResultField{"max_lyapunov_exponent", "real"}, ResultField{"runs_completed", "integer"}};
constexpr std::array LAGS_RESULTS = {ResultField{"lags_computed", "integer"}};
constexpr std::array POINTS_RESULTS = {ResultField{"points", "integer"}};
constexpr std::array POINCARE_RESULTS = {ResultField{"poincare_configured", "boolean"}};
constexpr std::array TORUS_RESULTS = {ResultField{"torus_variables", "list[name]"}};
constexpr std::array COLOR_RESULTS = {ResultField{"color_coding", "choice"}};
constexpr std::array COMPUTED_RESULTS = {ResultField{"computed", "boolean"}};
constexpr std::array UPDATED_RESULTS = {ResultField{"updated", "boolean"}};
constexpr std::array ADJOINT_RANGE_RESULTS = {ResultField{"adjoint_range", "boolean"}};

constexpr std::array STATE_EVENTS = {"state", "done"};
constexpr std::array RUN_EVENTS = {"computing", "progress", "series", "stopped", "state", "done"};
constexpr std::array BROWSER_EVENTS = {"browser", "state", "done"};
constexpr std::array SHOWN_EVENTS = {"browser", "plots", "series", "state", "done"};
constexpr std::array PLOT_EVENTS = {"plots", "state", "done"};
constexpr std::array DONE_EVENTS = {"done"};

constexpr std::array ROWS = {
    Command{"analysis.seed_random", SEED_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Analysis, "Numerics/Stochastic/New seed", "UHN",
            "Seed the random numbers of the next run.", SEED_RESULTS, STATE_EVENTS, true, seed_random},
    Command{"analysis.stochastic_integrate", TRIALS_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Stochastic/Compute", "UHC",
            "Run the model as many times as trials (the Range settings' steps plus one), keeping the mean and variance of the runs.",
            TRIALS_RESULTS, RUN_EVENTS, true, stochastic_integrate_handler},
    Command{"analysis.load_stochastic_run", {}, XPP_KIND_DATA, CommandCategory::Analysis, "Numerics/Stochastic/Data", "UHD",
            "Show the data of the last run in the data browser again.", LOADED_RESULTS, BROWSER_EVENTS, true, load_data},
    Command{"analysis.load_stochastic_mean", {}, XPP_KIND_DATA, CommandCategory::Analysis, "Numerics/Stochastic/Mean", "UHM",
            "Show the mean of the stochastic runs in the data browser.", LOADED_RESULTS, BROWSER_EVENTS, true, done_with<mean_back, LOADED>},
    Command{"analysis.load_stochastic_variance", {}, XPP_KIND_DATA, CommandCategory::Analysis, "Numerics/Stochastic/Variance", "UHV",
            "Show the variance of the stochastic runs in the data browser.", LOADED_RESULTS, BROWSER_EVENTS, true, done_with<variance_back, LOADED>},
    Command{"analysis.histogram", HISTOGRAM_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Stochastic/Histogram", "UHH",
            "Make the histogram of a column of the stored data, counting the rows where the condition holds.", HISTOGRAM_RESULTS, SHOWN_EVENTS, true,
            histogram_handler},
    Command{"analysis.reload_histogram", {}, XPP_KIND_DATA, CommandCategory::Analysis, "Numerics/Stochastic/Old hist", "UHO",
            "Show the last histogram in the data browser again.", RELOADED_RESULTS, SHOWN_EVENTS, true, done_with<reload_histogram, RELOADED>},
    Command{"analysis.histogram_2d", HISTOGRAM_2D_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Stochastic/2D-hist", "UH2",
            "Make the two-dimensional histogram of two columns of the stored data.", HISTOGRAM_2D_RESULTS, SHOWN_EVENTS, true, histogram_2d_handler},
    Command{"analysis.fourier_series", COLUMN_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Stochastic/Fourier", "UHF",
            "Make the Fourier series of a column of the stored data.", HARMONICS_RESULTS, BROWSER_EVENTS, true, transform_handler<compute_fourier, HARMONICS>},
    Command{"analysis.power_spectrum", COLUMN_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Stochastic/Power", "UHP",
            "Make the power spectrum of a column of the stored data: each harmonic's amplitude and phase.", FREQUENCIES_RESULTS, SHOWN_EVENTS, true,
            transform_handler<compute_power, FREQUENCIES>},
    Command{"analysis.fit_curve", FIT_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Stochastic/fIt data", "UHI",
            "Fit parameters and initial values so that variables of the model follow columns of a data file of the model's folder (Levenberg-Marquardt).",
            FIT_RESULTS, STATE_EVENTS, true, fit_curve},
    Command{"analysis.statistics", COLUMN_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Stochastic/Stat", "UHS",
            "The mean, variance and standard deviation of a column of the stored data.", STATISTICS_RESULTS, DONE_EVENTS, true, statistics_handler},
    Command{"analysis.lyapunov_exponent", LYAPUNOV_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Stochastic/Liapunov", "UHL",
            "The maximal Liapunov exponent of the stored orbit, or with range_over_parameters one for each run of a sweep of the Range settings.",
            LYAPUNOV_RESULTS, SHOWN_EVENTS, true, lyapunov_handler},
    Command{"analysis.autocorrelation", AUTOCORRELATION_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Stochastic/stAutocor", "UHA",
            "Make the histogram of the differences of a column of the stored data with itself.", LAGS_RESULTS, SHOWN_EVENTS, true,
            autocorrelation_handler},
    Command{"analysis.cross_correlation", CROSS_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Stochastic/Xcorrel etc", "UHX",
            "Make the correlation of two columns of the stored data at a number of lags either side of 0, directly or by the FFT.", LAGS_RESULTS,
            SHOWN_EVENTS, true, cross_correlation_handler},
    Command{"analysis.spectral_density", SPECTRAL_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Stochastic/spEc.dns", "UHE",
            "Make the power spectral density of a column, the cross spectrum of two or their coherence, averaged over windows of the stored data.",
            POINTS_RESULTS, SHOWN_EVENTS, true, spectral_density_handler},
    Command{"analysis.poincare_map", POINCARE_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Analysis, "Numerics/Poincare map", "UPN/UPS/UPM/UPP",
            "Set the Poincare map the runs store: none, the crossings of a section, the maxima and minima of a variable, or its period.",
            POINCARE_RESULTS, STATE_EVENTS, true, poincare_handler},
    Command{"analysis.phase_space_torus", TORUS_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Plot, "Phase space/All, None, Choose", "AA/AN/AC",
            "Fold all, none or chosen variables of the phase space into a torus of the given period.", TORUS_RESULTS, STATE_EVENTS, true, torus_handler},
    Command{"analysis.color_coding", COLOR_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Numerics/Color by value", "UCN/UCV/UCA",
            "Color a graph's orbit by its velocity or by a column, between given limits or those of the stored data.", COLOR_RESULTS, PLOT_EVENTS, true,
            color_coding_handler, COLOR_GROUPS},
    Command{"analysis.adjoint_compute", {}, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Adjoint/New adj", "UAN",
            "Compute the adjoint of the stored periodic orbit.", COMPUTED_RESULTS, DONE_EVENTS, true, done_with<new_adjoint, COMPUTED>},
    Command{"analysis.adjoint_interaction", COUPLING_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Adjoint/Make H", "UAM",
            "Make the interaction (H) function of the adjoint for the coupling of each differential equation; one left out keeps its coupling.",
            COMPUTED_RESULTS, DONE_EVENTS, true, adjoint_interaction},
    Command{"analysis.adjoint_load", {}, XPP_KIND_DATA, CommandCategory::Analysis, "Numerics/Adjoint/Adjoint", "UAA",
            "Show the adjoint in the data browser.", LOADED_RESULTS, BROWSER_EVENTS, true, done_with<load_adjoint, LOADED>},
    Command{"analysis.adjoint_load_orbit", {}, XPP_KIND_DATA, CommandCategory::Analysis, "Numerics/Adjoint/Orbit", "UAO",
            "Show the orbit in the data browser.", LOADED_RESULTS, BROWSER_EVENTS, true, load_data},
    Command{"analysis.adjoint_load_interaction", {}, XPP_KIND_DATA, CommandCategory::Analysis, "Numerics/Adjoint/Hfun", "UAH",
            "Show the interaction (H) function in the data browser.", LOADED_RESULTS, BROWSER_EVENTS, true, done_with<load_interaction, LOADED>},
    Command{"analysis.adjoint_configure", ADJOINT_CONFIGURE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Analysis, "Numerics/Adjoint/Parameters", "UAP",
            "Set the adjoint's maximum iterations and error tolerance.", UPDATED_RESULTS, STATE_EVENTS, true, adjoint_configure},
    Command{"analysis.adjoint_range", ENABLED_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Numerics/Adjoint/Range", "UAR",
            "Have the next range run also compute the orbit, adjoint and H function at each step; the range itself is run.range.",
            ADJOINT_RANGE_RESULTS, STATE_EVENTS, true, adjoint_range}};

} // namespace

std::span<const Command> analysis_commands() { return ROWS; }

} // namespace xpp::command
