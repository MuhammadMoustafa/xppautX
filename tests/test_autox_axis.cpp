/* autox_axis: a stored autoX axis is a tagged value (W330, #391). A model
   parameter and an autoX unknown with the same number are different axes,
   the period is not parameter 10, and the file's std::ptrdiff_t is made and read
   only by autox_axis_encode / autox_axis_decode, a bad one refused. */
#include "xpptest.h"
#include "autox_axis.h"
#include "autox_state.h"

TEST_CASE("test_autox_axis")
{
    using xpp::AutoxAxis;
    /* P10 (the model's tenth parameter) is not the period T (autoX's slot 10) */
    CHECK(AutoxAxis::parameter(10) != AutoxAxis::period());
    CHECK(AutoxAxis::parameter(10).is_parameter(10) && !AutoxAxis::period().is_parameter());
    CHECK(AutoxAxis::period().is_period() && !AutoxAxis::parameter(10).is_period() && !AutoxAxis::unknown(9).is_period());
    CHECK(AutoxAxis::unknown(xpp::autox_unknown::period) == AutoxAxis::period());

    /* a model with n parameters: parameters are 0..n-1, an unknown's slot follows */
    for (int n : {1, 3, 11, 40}) {
        for (int k = 0; k < n; k++) {
            const auto back = xpp::autox_axis_decode(xpp::autox_axis_encode(AutoxAxis::parameter(k), n), n);
            CHECK(back && *back == AutoxAxis::parameter(k));
        }
        for (int slot = 0; slot < xpp::autox_unknown::block_size; slot++) {
            const int code = xpp::autox_axis_encode(AutoxAxis::unknown(slot), n);
            CHECK(code == n + slot);
            const auto back = xpp::autox_axis_decode(code, n);
            CHECK(back && *back == AutoxAxis::unknown(slot));
        }
        /* neither a parameter nor one of autoX's slots */
        CHECK(!xpp::autox_axis_decode(-1, n));
        CHECK(!xpp::autox_axis_decode(n + xpp::autox_unknown::block_size, n));
        CHECK(!xpp::autox_axis_decode(100000, n));
    }

    /* growing the model rewrites the file's integer for an unknown, never the axis */
    const AutoxAxis period = AutoxAxis::period();
    CHECK(xpp::autox_axis_encode(period, 3) == 13 && xpp::autox_axis_encode(period, 5) == 15);
    CHECK(xpp::autox_axis_decode(13, 3) == period && xpp::autox_axis_decode(15, 5) == period);
    CHECK(xpp::autox_axis_decode(13, 5) == AutoxAxis::unknown(8)); /* the same integer, another model: another slot */

    /* the run's solver slots: autoX's block, then the model's parameters */
    AutoxParameterLayout layout;
    layout.model_parameters = 4;
    CHECK(layout.axis(xpp::autox_unknown::period) == period);
    CHECK(layout.axis(layout.solver_index(2)) == AutoxAxis::parameter(2));
    CHECK(layout.axis(0) == AutoxAxis::unknown(0));

    TEST_REPORT("autox_axis");
}
