/* The independent subsystems (W433): the connected components of the
   states' dependency graph a load finds (Model::subsystems), what joins
   every state into one, and BlockLu, which factors a block-diagonal matrix
   block by block, bit for bit as lu_factor and lu_solve do the whole matrix. */
#include "load_args.h"
#include "xpptest.h"
#include "model.h"
#include "session.h"
#include "expr_native.h"
#include "numerics_settings.h"
#include "xpp_math.h"
#include "xpp_util.h"

#include <bit>
#include <cstdint>
#include <random>
#include <string>
#include <vector>
#include <cstddef>
#include <memory>
#include <span>

namespace {

using Blocks = std::vector<std::vector<int>>;

const xpp::Model &load(const char *path)
{
    std::string file = path;
    char name[] = "test_subsystems";
    char *argv[] = {name, file.data(), nullptr};
    CHECK(load_args(2, argv, 1).has_value());
    return xpp::client_session().model();
}

void check_components()
{
    /* a graph with known components: 0-3, 1-2-4 and 5 alone, an edge one way only */
    const std::vector<std::vector<int>> graph{{3}, {2}, {4}, {}, {2}, {}};
    CHECK((xpp::connected_components(graph) == Blocks{{0, 3}, {1, 2, 4}, {5}}));
    CHECK(xpp::connected_components({}).empty());
    CHECK((xpp::connected_components({{}, {}}) == Blocks{{0}, {1}}));
    CHECK((xpp::connected_components({{1}, {2}, {3}, {}}) == Blocks{{0, 1, 2, 3}}));
}

void check_models()
{
    /* five states, three subsystems: x and y read each other, u and v, w alone */
    const xpp::Model &split = load("tools/models/subsystems.odex");
    CHECK(split.subsystem_joined_by.empty());
    CHECK((split.subsystems == Blocks{{0, 2}, {1, 3}, {4}}));
    /* a fixed variable that reads states of two of them joins all */
    const xpp::Model &joined = load("tools/models/subsystems_joined.odex");
    CHECK(joined.subsystem_joined_by.empty() && joined.subsystems.empty());
    /* a global event (the model's own spike reset) joins every state, whatever they read */
    const xpp::Model &event = load("examples/ode/izh_2003_ch.odex");
    CHECK(event.subsystem_joined_by == "global flags (events)" && event.subsystems.empty());
    /* a Poincare stop on x: two subsystems by the equations, one when the split is by the run (W433) */
    load("tools/models/subsystems_stop.odex");
    xpp::Session &stop = xpp::client_session();
    CHECK((stop.model().subsystems == Blocks{{0}, {1}}) && stop.numerics->subsystem_split == xpp::SubsystemSplit::equations);
    CHECK(xpp::has_subsystems(stop) && xpp::run_joined_by(stop).empty());
    stop.numerics->subsystem_split = xpp::SubsystemSplit::run;
    CHECK(!xpp::has_subsystems(stop) && xpp::run_joined_by(stop) == "a Poincare stop");
    CHECK(stop.model().subsystems == (Blocks{{0}, {1}})); /* the solvers keep the equations' split */
    load("tools/models/subsystems_stop_run.odex");
    CHECK(xpp::client_session().numerics->subsystem_split == xpp::SubsystemSplit::run && !xpp::has_subsystems(xpp::client_session()));
    /* a delay */
    const xpp::Model &delayed = load("examples/ode/delay.odex");
    CHECK(delayed.subsystem_joined_by == "delays" && delayed.subsystems.empty());
}

/* a random n x n matrix, block diagonal under blocks (zeros elsewhere) */
std::vector<double> block_diagonal(const Blocks &blocks, int n, std::mt19937_64 &random)
{
    std::uniform_real_distribution<double> value(-2.0, 2.0);
    std::vector<double> a(static_cast<std::size_t>(n) * static_cast<std::size_t>(n), 0.0);
    for (const auto &block : blocks)
        for (const int i : block)
            for (const int j : block) a[static_cast<std::size_t>(i * n + j)] = value(random) + (i == j ? 1.0 : 0.0);
    return a;
}

bool same_bits(const std::vector<double> &a, const std::vector<double> &b)
{
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::bit_cast<std::uint64_t>(a[i]) != std::bit_cast<std::uint64_t>(b[i])) return false;
    return true;
}

/* BlockLu's solve against lu_factor's and lu_solve's of the whole matrix: the same bits */
void check_block_lu(const Blocks &blocks, int n, bool transposed, std::mt19937_64 &random)
{
    const std::vector<double> a = block_diagonal(blocks, n, random);
    std::vector<double> b(static_cast<std::size_t>(n));
    for (double &x : b) x = std::uniform_real_distribution<double>(-1.0, 1.0)(random);
    std::vector<double> full = a, whole_solution = b;
    std::vector<int> pivots(static_cast<std::size_t>(n));
    int info;
    xpp::lu_factor(full.data(), n, n, pivots.data(), &info);
    CHECK(info == -1);
    xpp::lu_solve(full.data(), n, n, pivots.data(), whole_solution.data());
    /* the matrix as BlockLu is given it: its transpose when the layout is column-major */
    std::vector<double> given = a;
    if (transposed)
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) given[static_cast<std::size_t>(j * n + i)] = a[static_cast<std::size_t>(i * n + j)];
    xpp::BlockLu lu;
    int block_info;
    lu.factor(given.data(), n, n, blocks, &block_info, transposed);
    CHECK(block_info == -1);
    std::vector<double> block_solution = b;
    lu.solve(given.data(), n, block_solution.data());
    CHECK(same_bits(whole_solution, block_solution));
    /* a second right-hand side from the same factors */
    std::vector<double> again = whole_solution;
    xpp::lu_solve(full.data(), n, n, pivots.data(), again.data());
    block_solution = whole_solution;
    lu.solve(given.data(), n, block_solution.data());
    CHECK(same_bits(again, block_solution));
}

void check_singular()
{
    /* a zero pivot in a block is the last zero pivot lu_factor finds, by its index in the whole matrix */
    const Blocks blocks{{0, 2}, {1, 3}};
    std::vector<double> a(16, 0.0);
    a[0] = 1;  a[2] = 2;  a[8] = 2;  a[10] = 4; /* rows 0 and 2: {{1,2},{2,4}} singular */
    a[5] = 1;  a[7] = 0;  a[13] = 0; a[15] = 3;
    std::vector<double> full = a;
    std::vector<int> pivots(4);
    int info, block_info;
    xpp::lu_factor(full.data(), 4, 4, pivots.data(), &info);
    xpp::BlockLu lu;
    lu.factor(a.data(), 4, 4, blocks, &block_info);
    CHECK(info != -1 && block_info == info);
}

} // namespace

TEST_CASE("test_subsystems")
{
    check_components();
    check_models();
    std::mt19937_64 random(433);
    check_block_lu({{0, 2}, {1, 3}, {4}}, 5, false, random);
    check_block_lu({{0, 5, 6}, {1, 2}, {3}, {4, 7}}, 8, false, random);
    check_block_lu({{0, 5, 6}, {1, 2}, {3}, {4, 7}}, 8, true, random);
    check_block_lu({{0, 1, 2}}, 3, true, random);  /* one block: the whole matrix, column-major */
    check_block_lu({{0, 1, 2}}, 3, false, random); /* and in place */
    check_block_lu({{0, 1, 2, 3, 4, 5, 6, 7}}, 8, true, random); /* a larger one, with interchanges */
    check_singular();
    TEST_REPORT("subsystems");
}
