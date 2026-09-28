/* The .set file round trip: write_lunch() then read_lunch() must bring back
   every parameter, initial condition and numerics setting, and writing again
   must give the same file. A field that one side writes and the other reads
   in a different order shifts every value after it, which a user only
   notices as a restored session that behaves differently.

   The model is loaded the way xppautX -silent loads it (xpp_batch_main),
   without integrating. make test runs this from the top of the tree. */
#include "xpptest.h"
#include "session.h"
#include "lunch-new.h"
#include "xpp_batch.h"
#include "expr.h"
#include "browse.h"
#include "graphics.h"
#include "load_eqn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* the file without its first line, which carries the time it was written */
static char *body(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long n;
    char *s, *nl;
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    rewind(fp);
    s = static_cast<char *>(calloc(static_cast<size_t>(n) + 1, 1));
    if (fread(s, 1, static_cast<size_t>(n), fp) != static_cast<size_t>(n)) n = 0;
    fclose(fp);
    nl = strchr(s, '\n');
    return nl ? static_cast<char *>(memmove(s, nl + 1, strlen(nl + 1) + 1)) : s;
}

static void save(const char *path)
{
    FILE *fp = fopen(path, "w");
    write_lunch(fp);
    fclose(fp);
}

int main(void)
{
    char arg0[] = "test_lunch", arg1[] = "examples/ode/lecar.ode";
    char *argv[] = {arg0, arg1, NULL};
    const char *a = "build/test_lunch_a.set", *b = "build/test_lunch_b.set";
    double iapp, v0, tend, dt, x;
    FILE *fp;
    char *sa, *sb;

    xpp_load_model(2, argv, 1);
    init_browser();
    init_all_graph();

    get_val("iapp", &iapp);
    v0 = xpp::session().last_ic[0];
    tend = xpp::session().numerics.tend;
    dt = xpp::session().numerics.delta_t;
    save(a);

    set_val("iapp", iapp + 1);
    xpp::session().last_ic[0] = v0 + 1;
    xpp::session().numerics.tend = tend * 2;
    xpp::session().numerics.delta_t = dt / 2;

    fp = fopen(a, "r");
    CHECK(fp != NULL);
    if (!fp) TEST_REPORT("lunch round trip");
    CHECK(read_lunch(fp) == 1);
    fclose(fp);

    get_val("iapp", &x);
    CHECK(x == iapp);
    CHECK(xpp::session().last_ic[0] == v0);
    CHECK(xpp::session().numerics.tend == tend);
    CHECK(xpp::session().numerics.delta_t == dt);

    save(b);
    sa = body(a);
    sb = body(b);
    CHECK(sa && sb && strlen(sa) > 100);
    CHECK(sa && sb && strcmp(sa, sb) == 0);
    free(sa);
    free(sb);
    remove(a);
    remove(b);
    TEST_REPORT("lunch round trip");
}
