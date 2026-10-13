/* W298 (#356): AUTO's model-sized storage and continuation beyond the old limits. */
#include "xpptest.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "auto_settings.h"
#include "auto_nox.h"
#include "diagram.h"
#include "xpp_session_auto.h"
#include <cmath>
#include <string>

int main()
{
    char name[]="test_auto_limits";
    char file[]="tools/models/auto_limits.odex";
    char *argv[]={name,file,nullptr};
    CHECK(xpp::load_model(2,argv,1).has_value());
    xpp::Session &s=xpp::client_session();
    xpp::TempDir temp;
    s.auto_state.dir=temp.path();
    xpp::init_auto_win(s);
    CHECK(s.model().node==401 && s.model().nupar==40);
    CHECK(s.auto_state.grabpt.u0.size()==401 && s.auto_state.homo_l.size()==401);
    CHECK(s.auto_state.parameter_count()>36);
    xpp::AutoSettingsSet set;
    set.npars=40;
    for(int k=0;k<set.npars;k++)set.pars.push_back(xpp::format("P{}",k));
    set.par1="P39";
    set.var="X400";
    for(const auto &[field,value]: {std::pair{xpp::AUTO_NUM_NMX,4.0},std::pair{xpp::AUTO_NUM_NPR,1.0},std::pair{xpp::AUTO_NUM_SUPPBP,1.0}}){
        set.has_num[field]=1;
        set.num[field]=value;
    }
    std::string why;
    // A stored period stays distinct when the model selection grows past its old index.
    diagram_first(s.diagram)->icp2=AUTO_PERIOD_INDEX;
    CHECK(xpp::auto_settings_apply(s,set,why)==0);
    CHECK(diagram_first(s.diagram)->icp2>=s.auto_state.npar);
    CHECK(s.auto_state.npar==40 && s.auto_state.axes().icp1==39);
    CHECK(s.auto_state.parameter_slot(39)>36);
    set.nmarks=2;
    set.mark_name[0]="P10";
    set.mark_name[1]="T";
    set.mark_value[0]=0.2;
    set.mark_value[1]=6.3;
    CHECK(xpp::auto_settings_apply(s,set,why)==0);
    const auto marked=xpp::auto_settings_now(s);
    CHECK(marked.mark_name[0]==s.model().upar_names[10] && marked.mark_name[1]=="T");
    CHECK(s.auto_state.bifur.uzrpar[0]!=s.auto_state.bifur.uzrpar[1]);
    set.nmarks=0;
    CHECK(xpp::auto_settings_apply(s,set,why)==0);
    xpp::auto_start_diff_ss(s);
    CHECK(diagram_count(s.diagram)==4);
    const xpp::DIAGRAM *last=last_diagram(s.diagram);
    CHECK(last && last->icp1==39 && last->par.size()==static_cast<std::size_t>(s.auto_state.diagram_parameters()));
    CHECK(last && last->par[39]>0 && std::fabs(last->u0[400]-last->par[39])<1e-8);
    CHECK(last && std::fabs(last->u0[399]-last->par[39])<1e-8 && std::fabs(last->u0[0])<1e-8);
    const auto csv=xpp::snapx::auto_members::diagram_csv(s.diagram.points,{s.model().uvar_names.data(),401});
    const auto read=xpp::snapx::auto_members::parse_diagram_csv(csv,401,s.auto_state.diagram_parameters(),"limits.snapx/auto/diagram.csv");
    CHECK(read && read->size()==4 && read->back().d.par==last->par);
    const auto hostile=xpp::check_auto_solutions("1 1 4 1 2 0 1 402 2 0 0 999999\n", "sent.snapx/auto/solutions.s",401,s.auto_state.parameter_count());
    CHECK(!hostile && hostile.error().place.line==1 && hostile.error().place.file=="sent.snapx/auto/solutions.s");
    TEST_REPORT("auto limits");
}
