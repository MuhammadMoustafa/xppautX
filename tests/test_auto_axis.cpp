/* auto_axis: a stored AUTO axis is a tagged value (W330, #391). A model
   parameter and an AUTO unknown with the same number are different axes,
   the period is not parameter 10, and the file's integer is made and read
   only by auto_axis_encode / auto_axis_decode, a bad one refused. */
#include "xpptest.h"
#include "auto_axis.h"
#include "auto_state.h"

int main()
{
    using xpp::AutoAxis;
    /* P10 (the model's tenth parameter) is not the period T (AUTO's slot 10) */
    CHECK(AutoAxis::parameter(10) != AutoAxis::period());
    CHECK(AutoAxis::parameter(10).is_parameter(10) && !AutoAxis::period().is_parameter());
    CHECK(AutoAxis::period().is_period() && !AutoAxis::parameter(10).is_period() && !AutoAxis::unknown(9).is_period());
    CHECK(AutoAxis::unknown(xpp::auto_unknown::period) == AutoAxis::period());

    /* a model with n parameters: parameters are 0..n-1, an unknown's slot follows */
    for (int n : {1, 3, 11, 40}) {
        for (int k = 0; k < n; k++) {
            const auto back = xpp::auto_axis_decode(xpp::auto_axis_encode(AutoAxis::parameter(k), n), n);
            CHECK(back && *back == AutoAxis::parameter(k));
        }
        for (int slot = 0; slot < xpp::auto_unknown::block_size; slot++) {
            const int code = xpp::auto_axis_encode(AutoAxis::unknown(slot), n);
            CHECK(code == n + slot);
            const auto back = xpp::auto_axis_decode(code, n);
            CHECK(back && *back == AutoAxis::unknown(slot));
        }
        /* neither a parameter nor one of AUTO's slots */
        CHECK(!xpp::auto_axis_decode(-1, n));
        CHECK(!xpp::auto_axis_decode(n + xpp::auto_unknown::block_size, n));
        CHECK(!xpp::auto_axis_decode(100000, n));
    }

    /* growing the model rewrites the file's integer for an unknown, never the axis */
    const AutoAxis period = AutoAxis::period();
    CHECK(xpp::auto_axis_encode(period, 3) == 13 && xpp::auto_axis_encode(period, 5) == 15);
    CHECK(xpp::auto_axis_decode(13, 3) == period && xpp::auto_axis_decode(15, 5) == period);
    CHECK(xpp::auto_axis_decode(13, 5) == AutoAxis::unknown(8)); /* the same integer, another model: another slot */

    /* the run's solver slots: AUTO's block, then the model's parameters */
    AutoParameterLayout layout;
    layout.model_parameters = 4;
    CHECK(layout.axis(xpp::auto_unknown::period) == period);
    CHECK(layout.axis(layout.solver_index(2)) == AutoAxis::parameter(2));
    CHECK(layout.axis(0) == AutoAxis::unknown(0));

    TEST_REPORT("auto_axis");
}
