/* A model beyond XPPAUT's limits loads and runs (W243, #299): XPPAUT stops
   at 5000 variables, 400 parameters and 10000 symbols (XPPAUT's MAXODE, MAXPAR, MAX_SYMBS).
   The model is generated here, 5200 equations x_i' = -p_k x_i, each with
   one of 450 parameters, and one auxiliary quantity, so it needs 10404
   variables with their primes and 450 parameters; it loads, integrates
   (Runge-Kutta, one time unit) and writes its table. The solution is
   exp(-p_k t), checked at three variables, and the table's width and
   length are checked (data, never a time). */
#include "xpptest.h"
#include "session.h"
#include "model.h"
#include "storage.h"
#include "browse.h"
#include "graphics.h"
#include "integrate.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "expr.h"
#include "xpp_util.h"

#include <cmath>
#include <string>

namespace {
constexpr int EQUATIONS = 5200, PARAMETERS = 450;

/* the parameter of equation i, a rate between 0.5 and 0.95 */
double rate(int i) { return 0.5 + 0.001 * (i % PARAMETERS); }

std::string source()
{
    std::string text;
    for (int k = 0; k < PARAMETERS; k++) text += xpp::format("par p{} = {}\n", k, xpp::number(0.5 + 0.001 * k));
    for (int i = 0; i < EQUATIONS; i++) text += xpp::format("x{}' = -p{}*x{}\ninit x{} = 1\n", i, i % PARAMETERS, i, i);
    text += "aux both = x0 + x" + std::to_string(EQUATIONS - 1) + "\n";
    text += "options {\n  integration: { total_time: 1, dt: 0.1, method: runge_kutta },\n}\n";
    return text;
}

/* the lines of a text file */
int lines_of(const std::string &path)
{
    xpp::LineReader reader(path);
    int n = 0;
    while (reader.next()) n++;
    return n;
}
}

int main(void)
{
    xpp::TempDir temp;
    CHECK(!temp.path().empty());
    const std::string model_path = temp.file("large.odex");
    {
        xpp::Writer w(model_path);
        w.print("{}", source());
        CHECK(w.commit().has_value());
    }
    char arg0[] = "test_large_model";
    std::string path = model_path;
    char *argv[] = {arg0, path.data(), NULL};
    CHECK(xpp::load_model(2, argv, 1).has_value());
    xpp::Session &s = xpp::client_session();
    init_browser(s);
    init_all_graph(s);
    const xpp::Model &m = s.model();
    CHECK(m.node == EQUATIONS && m.neq == EQUATIONS + 1 && m.nupar == PARAMETERS);
    CHECK(m.nvar > 2 * EQUATIONS && s.parser.nsym > 10000 && s.parser.ncon > 400);
    CHECK(m.uvar_names[EQUATIONS - 1] == "X5199" && m.uvar_names[EQUATIONS] == "BOTH");

    /* W291: namespaces still resolve past the former fixed table sizes. */
    int column=-1;
    find_variable(s,"x5199",&column);
    CHECK(column==EQUATIONS);
    find_variable(s,"both",&column);
    CHECK(column==EQUATIONS+1);
    CHECK(xpp::get_var_index(s,"x5199")==EQUATIONS);
    CHECK(xpp::get_param_index(s,"p449")==m.upar_con[PARAMETERS-1]);
    CHECK(xpp::find_user_name(m,PARAMBOX,"p449")==PARAMETERS-1);
    const std::size_t indexed=m.names.size();

    xpp::get_ic(s, 2, s.data_store.current.data());
    s.numerics.storflag=true;
    CHECK(xpp::usual_integrate_stuff(s, s.data_store.current.data()) == 0);
    CHECK(m.names.size()==indexed);
    const int rows = s.data_store.rows;
    CHECK(rows == 11);
    for (int i : {0, 2600, EQUATIONS - 1})
        CHECK(std::fabs(static_cast<double>(s.data_store.col[i + 1][rows - 1]) - std::exp(-rate(i))) < 1e-5);
    CHECK(std::fabs(static_cast<double>(s.data_store.col[EQUATIONS + 1][rows - 1]) - (std::exp(-rate(0)) + std::exp(-rate(EQUATIONS - 1)))) < 1e-5);

    const std::string table = temp.file("table.dat");
    {
        xpp::Writer w(table);
        write_mybrowser_data(s, w);
        CHECK(w.commit().has_value());
    }
    CHECK(lines_of(table) == rows);
    TEST_REPORT("large model");
}
