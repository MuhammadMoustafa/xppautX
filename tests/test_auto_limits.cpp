/* W298 (#356): AUTO's model-sized storage and continuation beyond the old limits. */
#include "xpptest.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "auto_settings.h"
#include "auto_nox.h"
#include "diagram.h"
#include "xpp_session_auto.h"
#include "xpp_ui.h"
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
    // W301: the homoclinic form has two dimensions and two values per model state.
    const xpp::XppUi saved_ui=xpp::ui;
    xpp::XppUi form_ui=saved_ui;
    form_ui.question_policy=[](bool) { return xpp::QuestionPolicy::User; };
    form_ui.string_box=[](int row,int col,std::string_view title,const char *const *names,
                          std::span<std::string> values,const int *kinds) {
        CHECK(row==402 && col==2 && title=="Homoclinic info");
        CHECK(values.size()==804);
        CHECK(std::string_view(names[401])=="X400_L" && std::string_view(names[803])=="X400_R");
        CHECK(kinds[0]==xpp::XPP_FIELD_INTEGER && kinds[402]==xpp::XPP_FIELD_INTEGER);
        CHECK(values[401]=="0" && values[803]=="0");
        values[0]="2";
        values[402]="399";
        values[401]="2.75";
        values[803]="3.25";
        return 1;
    };
    xpp::set_ui(&form_ui);
    int unstable=1,stable=400;
    s.auto_state.homo_flag=2;
    CHECK(xpp::get_homo_info(s,2,&unstable,&stable,s.auto_state.homo_l.data(),s.auto_state.homo_r.data())==1);
    xpp::set_ui(&saved_ui);
    CHECK(unstable==2 && stable==399);
    CHECK(s.auto_state.homo_l[400]==2.75 && s.auto_state.homo_r[400]==3.25);
    s.auto_state.homo_flag=0;
    xpp::AutoSettingsSet set;
    set.par1="P39";
    set.var="X400";
    for(const auto &[field,value]: {std::pair{xpp::AUTO_NUM_NMX,4.0},std::pair{xpp::AUTO_NUM_NPR,1.0},std::pair{xpp::AUTO_NUM_SUPPBP,1.0}}){
        set.has_num[field]=1;
        set.num[field]=value;
    }
    std::string why;
    // W339: every model parameter is available directly; a bad name changes nothing.
    xpp::AutoSettingsSet selection;
    selection.par1="not_a_parameter";
    selection.has_num[xpp::AUTO_NUM_NMX]=1;
    selection.num[xpp::AUTO_NUM_NMX]=99;
    const int old_max=s.auto_state.bifur.nmx;
    CHECK(xpp::auto_settings_apply(s,selection,why)!=0);
    CHECK(s.auto_state.bifur.nmx==old_max && s.auto_state.axes().icp1==0);
    CHECK(xpp::auto_settings_apply(s,set,why)==0);
    CHECK(s.auto_state.axes().icp1==39 && s.auto_state.model_parameters==40);
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
    CHECK(last && last->icp1==39 && last->par.size()==1 && last->branch->parameters.size()==40);
    CHECK(last && diagram_parameter(*last,39)>0 && std::fabs(last->u0[400]-diagram_parameter(*last,39))<1e-8);
    CHECK(last && std::fabs(last->u0[399]-diagram_parameter(*last,39))<1e-8 && std::fabs(last->u0[0])<1e-8);
    const auto csv=xpp::snapx::auto_members::diagram_csv(s.diagram.points,{s.model().uvar_names.data(),401});
    const auto read=xpp::snapx::auto_members::parse_diagram_csv(csv,401,s.model().nupar,"limits.snapx/auto/diagram.csv");
    CHECK(read && read->size()==4 && read->back().d.par==last->par);
    const auto hostile=xpp::check_auto_solutions("1 1 4 1 2 0 1 402 2 0 0 999999\n", "sent.snapx/auto/solutions.s",401,s.auto_state.parameter_count());
    CHECK(!hostile && hostile.error().place.line==1 && hostile.error().place.file=="sent.snapx/auto/solutions.s");
    TEST_REPORT("auto limits");
}
