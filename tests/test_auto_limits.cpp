/* W298 (#356): AUTO's model-sized storage and continuation beyond the old limits. */
#include "load_args.h"
#include "xpptest.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "auto_settings.h"
#include "auto_nox.h"
#include "auto_run.h"
#include "diagram.h"
#include "xpp_session_auto.h"
#include "xpp_ui.h"
#include "xpp_log.h"
#include <cmath>
#include <string>

int main()
{
    char name[]="test_auto_limits";
    char file[]="tools/models/auto_limits.odex";
    char *argv[]={name,file,nullptr};
    CHECK(load_args(2,argv,1).has_value());
    xpp::Session &s=xpp::client_session();
    xpp::TempDir temp;
    s.auto_state.dir=temp.path();
    xpp::init_auto_win(s);
    CHECK(s.model().node==401 && s.model().nupar==40);
    CHECK(s.auto_state.grabpt.u0.size()==401 && s.auto_state.homo_l.size()==401);
    CHECK(s.auto_state.parameter_layout.size()>36);
    // W340/#401: every model index is invertible and all AUTO slots are excluded.
    const auto &layout=s.auto_state.parameter_layout;
    CHECK(layout.size()==layout.block+s.model().nupar);
    for(int k=0;k<s.model().nupar;k++)CHECK(layout.model_index(layout.solver_index(k))==k);
    for(int slot=0;slot<layout.block;slot++)CHECK(layout.model_index(slot)==-1);
    CHECK(layout.model_index(layout.size())==-1);
    CHECK(layout.solver_index(9)==layout.block+9);
    // W301: the common form supplies model-sized homoclinic lists, not a fixed field array.
    xpp::HomoclinicStart start{2,399,s.auto_state.homo_l,s.auto_state.homo_r};
    start.left[400]=2.75;
    start.right[400]=3.25;
    CHECK(start.left.size()==401 && start.right.size()==401);
    CHECK(start.left[400]==2.75 && start.right[400]==3.25);
    // W365: dimensions that do not add up to the model's start no run and keep the ones before.
    xpp::AutoRunRequest request;
    request.kind=xpp::AutoRunKind::heteroclinic;
    request.homoclinic=xpp::HomoclinicStart{2,400,start.left,start.right};
    const xpp::Result<> refused=xpp::auto_run_start(s,request);
    CHECK(!refused && refused.error().what.find("must add up to the number of variables, 401")!=std::string::npos);
    CHECK(s.auto_state.run.nunstab==1);
    s.auto_state.homo_flag=HomoclinicRun::None;
    xpp::AutoSettingsSet set;
    set.par1="P39";
    set.var="X400";
    for(const auto &[field,value]: {std::pair{xpp::AUTO_NUM_MAX_STEPS,4.0},std::pair{xpp::AUTO_NUM_OUTPUT_EVERY,1.0},std::pair{xpp::AUTO_NUM_SUPPRESS_BRANCH_POINTS,1.0}}){
        set.has_num[field]=1;
        set.num[field]=value;
    }
    std::string why;
    // W339: every model parameter is available directly; a bad name changes nothing.
    xpp::AutoSettingsSet selection;
    selection.par1="not_a_parameter";
    selection.has_num[xpp::AUTO_NUM_MAX_STEPS]=1;
    selection.num[xpp::AUTO_NUM_MAX_STEPS]=99;
    const int old_max=s.auto_state.bifur.nmx;
    CHECK(xpp::auto_settings_apply(s,selection,why)!=0);
    CHECK(s.auto_state.bifur.nmx==old_max && s.auto_state.axes().icp1==0);
    CHECK(xpp::auto_settings_apply(s,set,why)==0);
    CHECK(s.auto_state.axes().icp1==39 && s.auto_state.parameter_layout.model_parameters==40);
    CHECK(s.auto_state.parameter_layout.solver_index(39)>36);
    set.nmarks=2;
    set.mark_name={"P10","T"};
    set.mark_value={0.2,6.3};
    CHECK(xpp::auto_settings_apply(s,set,why)==0);
    const auto marked=xpp::auto_settings_now(s);
    CHECK(marked.mark_name[0]==s.model().upar_names[10] && marked.mark_name[1]=="T");
    CHECK(!s.auto_state.bifur.marks[0].period && s.auto_state.bifur.marks[1].period);
    set.nmarks=0;
    set.mark_name.clear();
    set.mark_value.clear();
    CHECK(xpp::auto_settings_apply(s,set,why)==0);
    // W331/#392: registry list values can create more marks than the old storage held.
    xpp::AutoSettingsSet many;
    many.nmarks=128;
    for(int i=0;i<many.nmarks;i++) {many.mark_name.emplace_back("T");many.mark_value.push_back(100+i);}
    CHECK(xpp::auto_settings_apply(s,many,why)==0);
    const auto marks=s.auto_state.bifur.marks;
    CHECK(marks.size()==128 && marks.back().value==227);
    const auto marks_text=xpp::snapx::auto_members::settings_text(xpp::auto_settings_now(s));
    const auto marks_read=xpp::snapx::auto_members::parse_settings(marks_text,"marks.snapx/auto/settings.txt");
    CHECK(marks_read && marks_read->set.nmarks==128);
    s.auto_state.bifur.marks.clear();
    CHECK(marks_read && xpp::auto_settings_apply(s,marks_read->set,why)==0);
    CHECK(s.auto_state.bifur.marks==marks);
    auto invalid=xpp::auto_settings_now(s);
    invalid.num[xpp::AUTO_NUM_MAX_STEPS]=99;
    invalid.mark_name.back()="unknown";
    CHECK(xpp::auto_settings_apply(s,invalid,why)!=0);
    CHECK(s.auto_state.bifur.marks==marks && s.auto_state.bifur.nmx==4);
    invalid=xpp::auto_settings_now(s);
    invalid.nmarks=129;
    CHECK(xpp::auto_settings_apply(s,invalid,why)!=0 && s.auto_state.bifur.marks==marks);
    // An input count must be checked against its values before any allocation.
    invalid.nmarks=2147483647;
    CHECK(xpp::auto_settings_apply(s,invalid,why)!=0 && s.auto_state.bifur.marks==marks);
    CHECK(xpp::auto_run_start(s,xpp::AutoRunRequest{}));
    CHECK(diagram_count(s.diagram)==4);
    CHECK(s.auto_state.run.marks.size()==128 && s.auto_state.run.marks.back().value==227);
    const xpp::DIAGRAM *last=last_diagram(s.diagram);
    CHECK(last && last->icp1.is_parameter(39) && last->par.size()==1 && last->branch->parameters.size()==40);
    CHECK(last && diagram_parameter(*last,39)>0 && std::fabs(last->u0[400]-diagram_parameter(*last,39))<1e-8);
    CHECK(last && std::fabs(last->u0[399]-diagram_parameter(*last,39))<1e-8 && std::fabs(last->u0[0])<1e-8);
    const auto csv=xpp::snapx::auto_members::diagram_csv(s.diagram.points,s.model().nupar,{s.model().uvar_names.data(),401});
    const auto read=xpp::snapx::auto_members::parse_diagram_csv(csv,401,s.model().nupar,"limits.snapx/auto/diagram.csv");
    CHECK(read && read->size()==4 && read->back().par==last->par);
    // Continue the tenth parameter as a model parameter, independently of AUTO's block.
    // Start from the equilibrium just computed, including the held fortieth parameter.
    std::copy_n(last->u0.begin(),s.model().node,s.last_ic.begin());
    set.par1="P9";
    CHECK(xpp::auto_settings_apply(s,set,why)==0);
    CHECK(xpp::auto_run_start(s,xpp::AutoRunRequest{}));
    last=last_diagram(s.diagram);
    CHECK(last && last->icp1.is_parameter(9) && last->parameter_indices==std::vector<int>{9});
    CHECK(last && std::string_view(xpp::auto_par_name(s,last->icp1.index))==s.model().upar_names[9]);
    CHECK(s.diagram.points.size()==8);
    CHECK(last && diagram_parameter(*last,9)!=0 && std::abs(last->u0[9]-diagram_parameter(*last,9))<1e-8);
    const auto hostile=xpp::check_auto_solutions("1 1 4 1 2 0 1 402 2 0 0 999999\n", "sent.snapx/auto/solutions.s",401,s.auto_state.parameter_layout.size(),false);
    CHECK(!hostile && hostile.error().place.line==1 && hostile.error().place.file=="sent.snapx/auto/solutions.s");
    TEST_REPORT("auto limits");
}
