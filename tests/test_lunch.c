/* The .set file round trip: write_lunch() then read_lunch() must bring back
   every parameter, initial condition and numerics setting, and writing again
   must give the same file. A field that one side writes and the other reads
   in a different order shifts every value after it, which a user only
   notices as a restored session that behaves differently.

   The model is loaded the way xppautX -silent loads it (xpp_batch_main),
   without integrating. make test runs this from the top of the tree. */
#include "xpptest.h"
#include "lunch-new.h"
#include "xpp_batch.h"
#include "parserslow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern double TEND, DELTA_T;
extern double last_ic[];
void init_browser(void);
void init_all_graph(void);

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
    s = calloc((size_t)n + 1, 1);
    if (fread(s, 1, (size_t)n, fp) != (size_t)n) n = 0;
    fclose(fp);
    nl = strchr(s, '\n');
    return nl ? memmove(s, nl + 1, strlen(nl + 1) + 1) : s;
}

static void save(const char *path)
{
    FILE *fp = fopen(path, "w");
    write_lunch(fp);
    fclose(fp);
}

int main(void)
{
    char *argv[] = {"test_lunch", "examples/ode/lecar.ode", NULL};
    const char *a = "build/test_lunch_a.set", *b = "build/test_lunch_b.set";
    double iapp, v0, tend, dt, x;
    FILE *fp;
    char *sa, *sb;

    xpp_load_model(2, argv, 1);
    init_browser();
    init_all_graph();

    get_val("iapp", &iapp);
    v0 = last_ic[0];
    tend = TEND;
    dt = DELTA_T;
    save(a);

    set_val("iapp", iapp + 1);
    last_ic[0] = v0 + 1;
    TEND = tend * 2;
    DELTA_T = dt / 2;

    fp = fopen(a, "r");
    CHECK(fp != NULL);
    if (!fp) TEST_REPORT("lunch round trip");
    CHECK(read_lunch(fp) == 1);
    fclose(fp);

    get_val("iapp", &x);
    CHECK(x == iapp);
    CHECK(last_ic[0] == v0);
    CHECK(TEND == tend);
    CHECK(DELTA_T == dt);

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
