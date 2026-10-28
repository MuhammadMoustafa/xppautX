/* W386 (#448): input-sized writes, including recoverable growth failure. */
#include "load_args.h"
#include "xpptest.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_ui.h"
#include "xpp_globals.h"
#include "xpp_mem.h"
#include "model_files.h"
#include "integrate.h"
#include "pp_shoot.h"
#include "expr_decode.h"
#include "auto_c.h"
#include <cstdlib>
#include <limits>

namespace {
void fail_growth(bool fail)
{
#ifdef _WIN32
    CHECK(_putenv_s("XPP_STORAGE_FAIL_GROW",fail?"1":"")==0);
#else
    CHECK((fail?setenv("XPP_STORAGE_FAIL_GROW","1",1):unsetenv("XPP_STORAGE_FAIL_GROW"))==0);
#endif
}
}

int main()
{
    char name[]="test_input_bounds", file[]="tools/models/input_bounds.odex";
    char *argv[]={name,file,nullptr};
    const auto loaded=load_args(2,argv,1);
    CHECK(loaded.has_value());
    if(!loaded)TEST_REPORT("input bounds");
    auto &s=xpp::client_session();
    xpp::batch_start(s);
    const auto saved_ui=xpp::ui;
    auto test_ui=saved_ui;
    test_ui.question_policy=[](bool){return xpp::QuestionPolicy::User;};
    test_ui.string_box=[](int,int,std::string_view,const char *const *,std::span<std::string>,const int *){return 1;};
    test_ui.typed_form=[](std::string_view,std::span<xpp::FormField>){return 1;};
    test_ui.err_msg=[](const xpp::Error &e){CHECK(e.where=="storage");};
    xpp::set_ui(&test_ui);
    s.integrator.eq_range.item="a";
    s.integrator.eq_range.steps=9;
    s.integrator.eq_range.plow=1;
    s.integrator.eq_range.phigh=10;
    s.integrator.eq_range.shoot=0;
    s.integrator.eq_range.mc=0;
    s.integrator.eq_range.movie=0;
    for(bool fail:{false,true}){
        s.data_store.allocate(2,s.model().neq+1);
        fail_growth(fail);
        double x=1;
        xpp::do_eq_range(s,&x);
        CHECK(s.data_store.rows==(fail?2:10));
        CHECK(s.data_store.col[0][0]==1 && s.data_store.col[0][1]==2);
        if(!fail)CHECK(s.data_store.col[0][9]==10);
    }
    fail_growth(false);
    s.shoot_range.item="a";
    s.shoot_range.steps=9;
    s.shoot_range.plow=1;
    s.shoot_range.phigh=10;
    CHECK(xpp::compile_bvp(s).has_value());
    s.numerics.storflag=false;
    for(bool fail:{false,true}){
        s.data_store.allocate(2,s.model().neq+1);
        fail_growth(fail);
        double start=1,end=1;
        const xpp::Result<int> swept=xpp::do_sh_range(s,&start,&end);
        CHECK(swept.has_value()==!fail);
        CHECK(fail?(!swept&&swept.error().where=="storage"):*swept==10);
        CHECK(s.data_store.rows==(fail?2:10));
        CHECK(s.data_store.col[0][0]==1 && s.data_store.col[0][1]==2);
        if(!fail)CHECK(s.data_store.col[0][9]==10);
    }
    fail_growth(false);
    xpp::set_ui(&saved_ui);

    /* Right association holds 301 operands before the first addition. */
    std::string formula="1";
    for(int i=0;i<300;++i)formula="1+("+formula+")";
    std::vector<int> code;
    CHECK(xpp::add_expr(s,formula,code)==0);
    std::size_t depth=0;
    CHECK(xpp::expr::decode(s,code,-1,&depth).has_value());
    CHECK(depth==301);
    CHECK(xpp::evaluate(s,code.data())==301);
    const auto *values=s.parser.stack.values.data();
    CHECK(xpp::evaluate(s,code.data())==301);
    CHECK(values==s.parser.stack.values.data());

    /* Past INT_MAX and size_t: reject before any cast or huge allocation. */
    char bad_file[]="tools/models/input_bounds_delay_bad.odex";
    char *bad_args[]={name,bad_file,nullptr};
    const auto delay=load_args(2,bad_args,1);
    CHECK(!delay);
    if(delay)TEST_REPORT("input bounds");
    if(!delay){
        CHECK(delay.error().where=="delay");
        CHECK(delay.error().place.file==bad_file && delay.error().place.line==4);
        CHECK(delay.error().what.find("delay=")!=std::string::npos);
        CHECK(delay.error().what.find("dt=")!=std::string::npos);
    }
    CHECK(&xpp::client_session()==&s);
    CHECK(xpp::alloc_delay(s,1).has_value());
    double history=3;
    for(std::size_t i=0;i<s.delay.rows+1;++i)xpp::stor_delay(s,&history);
    CHECK(xpp::get_delay(s,0,0)==3);

    CHECK(xpp::size_product(65536,65536)==4294967296ULL);
    CHECK(sizeof(integer)>=8);
    SolvbvWork work;
    bool overflow=false;
    try{work.resize(std::numeric_limits<std::ptrdiff_t>::max(),2,2,1,1,1);}
    catch(const std::length_error &){overflow=true;}
    CHECK(overflow && work.a.empty());
    overflow=false;
    try{xpp::Matrix3<double> view({},1LL<<32,1LL<<32,2);}
    catch(const std::length_error &){overflow=true;}
    CHECK(overflow);
    TEST_REPORT("input bounds");
}
