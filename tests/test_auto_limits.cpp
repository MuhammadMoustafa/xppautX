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
#include "xpp_log.h"
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
    CHECK(s.auto_state.parameter_layout.size()>36);
    // W340/#401: every model index is invertible and all AUTO slots are excluded.
    const auto &layout=s.auto_state.parameter_layout;
    CHECK(layout.size()==layout.block+s.model().nupar);
    for(int k=0;k<s.model().nupar;k++)CHECK(layout.model_index(layout.solver_index(k))==k);
    for(int slot=0;slot<layout.block;slot++)CHECK(layout.model_index(slot)==-1);
    CHECK(layout.model_index(layout.size())==-1);
    CHECK(layout.solver_index(9)==layout.block+9);
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
    s.auto_state.homo_flag=HomoclinicRun::Heteroclinic;
    CHECK(xpp::get_homo_info(s,&unstable,&stable,s.auto_state.homo_l.data(),s.auto_state.homo_r.data())==1);
    xpp::set_ui(&saved_ui);
    CHECK(unstable==2 && stable==399);
    CHECK(s.auto_state.homo_l[400]==2.75 && s.auto_state.homo_r[400]==3.25);
    // W365: dimensions that do not add up to the model's start no run and keep the ones before.
    form_ui.string_box=[](int,int,std::string_view,const char *const *,std::span<std::string> values,const int *) {
        values[0]="2";
        values[402]="400";
        return 1;
    };
    xpp::set_ui(&form_ui);
    CHECK(xpp::get_homo_info(s,&unstable,&stable,s.auto_state.homo_l.data(),s.auto_state.homo_r.data())==0);
    xpp::set_ui(&saved_ui);
    CHECK(unstable==2 && stable==399);
    s.auto_state.homo_flag=HomoclinicRun::None;
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
    // W331/#392: a form can create more marks than any old storage layer held.
    form_ui.new_string=[](std::string_view title,std::string &value,int kind) {
        CHECK(title=="Mark values: how many?" && kind==xpp::XPP_FIELD_INTEGER);
        value="128";
        return 1;
    };
    form_ui.string_box=[](int,int,std::string_view,const char *const *names,
                          std::span<std::string> values,const int *) {
        CHECK(values.size()==128 && std::string_view(names[127])=="Uzr128");
        CHECK(values[0]=="T=11" && values[8]=="T=35" && values[127]=="T=392");
        for(std::size_t i=0;i<values.size();i++)values[i]=xpp::format("T={}",100+i);
        return 1;
    };
    xpp::set_ui(&form_ui);
    xpp::auto_per_par(s);
    xpp::set_ui(&saved_ui);
    const auto marks=s.auto_state.bifur.marks;
    CHECK(marks.size()==128 && marks.back().value==227);
    const auto marks_text=xpp::snapx::auto_members::settings_text(xpp::auto_settings_now(s));
    const auto marks_read=xpp::snapx::auto_members::parse_settings(marks_text,"marks.snapx/auto/settings.txt");
    CHECK(marks_read && marks_read->set.nmarks==128);
    s.auto_state.bifur.marks.clear();
    CHECK(marks_read && xpp::auto_settings_apply(s,marks_read->set,why)==0);
    CHECK(s.auto_state.bifur.marks==marks);
    auto invalid=xpp::auto_settings_now(s);
    invalid.num[xpp::AUTO_NUM_NMX]=99;
    invalid.mark_name.back()="unknown";
    CHECK(xpp::auto_settings_apply(s,invalid,why)!=0);
    CHECK(s.auto_state.bifur.marks==marks && s.auto_state.bifur.nmx==4);
    form_ui.string_box=[](int,int,std::string_view,const char *const *,std::span<std::string> values,const int *) {
        values.front()="T=5";
        values.back()="T=not_a_number";
        return 1;
    };
    xpp::set_ui(&form_ui);
    xpp::auto_per_par(s);
    xpp::set_ui(&saved_ui);
    CHECK(s.auto_state.bifur.marks==marks);
    form_ui.new_string=[](std::string_view,std::string &value,int) { value="1"; return 1; };
    form_ui.string_box=[](int,int,std::string_view,const char *const *,std::span<std::string>,const int *) { return 0; };
    xpp::set_ui(&form_ui);
    xpp::auto_per_par(s);
    xpp::set_ui(&saved_ui);
    CHECK(s.auto_state.bifur.marks==marks);
    invalid=xpp::auto_settings_now(s);
    invalid.nmarks=129;
    CHECK(xpp::auto_settings_apply(s,invalid,why)!=0 && s.auto_state.bifur.marks==marks);
    // A count from a recording must be checked before sizing a form.
    form_ui.new_string=[](std::string_view,std::string &value,int) { value="2147483647"; return 1; };
    xpp::set_ui(&form_ui);
    xpp::auto_per_par(s);
    xpp::set_ui(&saved_ui);
    CHECK(s.auto_state.bifur.marks==marks);
    xpp::auto_start_diff_ss(s);
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
    xpp::auto_start_diff_ss(s);
    last=last_diagram(s.diagram);
    CHECK(last && last->icp1.is_parameter(9) && last->parameter_indices==std::vector<int>{9});
    CHECK(last && std::string_view(xpp::auto_par_name(s,last->icp1.index))==s.model().upar_names[9]);
    CHECK(s.diagram.points.size()==8);
    CHECK(last && diagram_parameter(*last,9)!=0 && std::abs(last->u0[9]-diagram_parameter(*last,9))<1e-8);
    const auto hostile=xpp::check_auto_solutions("1 1 4 1 2 0 1 402 2 0 0 999999\n", "sent.snapx/auto/solutions.s",401,s.auto_state.parameter_layout.size(),false);
    CHECK(!hostile && hostile.error().place.line==1 && hostile.error().place.file=="sent.snapx/auto/solutions.s");
    TEST_REPORT("auto limits");
}
