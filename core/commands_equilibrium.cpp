/* Equilibria's registry commands (W522). The numerical routines remain in their owners. */
#include "command_def.h"
#include "equilibria.h"
#include "integrate.h"
#include "del_stab.h"
#include "session.h"
#include "many_pops.h"
#include "graf_par.h"
#include "numerics_settings.h"
#include "xpp_util.h"
#include "xpp_ui.h"
#include "getvar.h"
#include "json_reader.h"
#include "json_number.h"
#include "browse.h"
#include "my_rhs.h"
#include "derived.h"
#include "typed_values.h"
#include "form_ode.h"
#include <array>
#include <limits>

namespace xpp::command {
namespace {
std::string values_json(const Session &s,std::span<const double> values)
{
    std::string result="{";
    for(std::size_t i=0;i<values.size();++i){
        if(i)result+=',';
        json_append_string(result,s.model().uvar_names[i]);result+=':';
        json::json_append_number_shortest(result,values[i]);
    }
    return result+'}';
}
std::string equilibrium_result(const Session &s)
{
    std::string result="{\"converged\":true,\"equilibrium\":"+values_json(s,s.last_equilibrium)+",\"eigenvalues\":[";
    for(std::size_t i=0;i<s.last_equilibrium_eigenvalues.size();i+=2){
        if(i)result+=',';
        result+="{\"real\":";json::json_append_number_shortest(result,s.last_equilibrium_eigenvalues[i]);
        result+=",\"imaginary\":";json::json_append_number_shortest(result,s.last_equilibrium_eigenvalues[i+1]);result+='}';
    }
    result+="],\"stability\":";json_append_string(result,s.last_equilibrium_stability);
    return result+'}';
}
/* The old Equilibria entry prepared the numerical context before any of its searches. */
Result<> prepare_equilibrium(Session &s,const Command &c,const Place &p)
{
    if(s.numerics.fft||s.numerics.hist||s.model().nkernel>0)
        return fail(c.name,"Equilibrium finding is unavailable for this model or data",p);
    s.numerics.storflag=false;
    s.numerics.poincare_map=PoincareMap::none;
    s.numerics.store_from=0;
    evaluate_derived(s);
    return {};
}
Result<std::string> find_point(Session &s,const Command &c,const Arguments &a,const Place &p)
{
    std::vector<double> x(s.model().neq);
    get_ic(s,2,x.data());
    std::optional<ActiveGraph> active;
    if(a.has("coordinates")){
        const auto target=find_window(s,a.optional<std::string>("window").value_or(s.selected_window));
        if(!target||target->kind!=WindowKind::graph)return fail(c.name,"Not in a usable 2D plane",p);
        active.emplace(s,target->index);
        const auto &graph=s.plot_windows.graph[target->index];
        const int ix=graph.xv[0]-1,iy=graph.yv[0]-1;
        if(!phase_plane_states(s,graph))
            return fail(c.name,"Not in a usable 2D plane",p);
        const auto coordinates=a.map<double>("coordinates");
        if(coordinates.size()!=2)return fail(c.name,"coordinates must name the two plotted variables",p);
        for(const auto &[name,value]:coordinates){
            const int index=find_user_name(s.model(),ICBOX,name);
            if(index!=ix&&index!=iy)return fail(c.name,"coordinates must name the two plotted variables",p);
            x[index]=value;
        }
    }
    const double oldtrans=s.numerics.store_from;
    if(const auto ready=prepare_equilibrium(s,c,p);!ready)return std::unexpected(ready.error());
    int ierr=0;float stability=0;
    const auto &settings=s.numerics.equilibria;
    const auto found=s.delay.flag
        ?do_delay_sing(s,x.data(),settings.jacobian_step,settings.newton_tolerance,settings.search_limit,settings.max_iterations,s.model().node,&ierr,&stability)
        :do_sing(s,x.data(),settings.jacobian_step,settings.newton_tolerance,settings.search_limit,settings.max_iterations,s.model().node,&ierr,&stability);
    s.numerics.store_from=oldtrans;
    if(!found)return std::unexpected(found.error());
    return equilibrium_result(s);
}
Result<std::string> guess(Session &s,const Command &c,const Arguments &a,const Place &p){return find_point(s,c,a,p);}
Result<std::string> range(Session &s,const Command &c,const Arguments &a,const Place &p)
{
    auto r=s.integrator.eq_range;
    r.item=a.get<std::string>("parameter");r.plow=a.get<double>("start");r.phigh=a.get<double>("end");
    r.steps=static_cast<int>(a.get<long long>("steps"));
    r.shoot=a.optional<bool>("shoot").value_or(r.shoot);
    r.col=static_cast<int>(a.optional<long long>("stability_column").value_or(r.col));
    r.movie=a.optional<bool>("movie").value_or(r.movie);r.mc=a.optional<bool>("monte_carlo").value_or(r.mc);
    if(const auto ready=prepare_equilibrium(s,c,p);!ready)return std::unexpected(ready.error());
    s.integrator.eq_range=std::move(r);
    const auto run=equilibrium_range(s);
    if(!run)return std::unexpected(run.error());
    return xpp::format("{{\"points_found\":{}}}",*run);
}
Result<std::string> search(Session &s,const Command &c,const Arguments &a,const Place &p)
{
    auto settings=s.integrator.fixptguess;
    for(const auto &[name,bounds]:a.map<std::array<double,2>>("bounds")){
        const int index=find_user_name(s.model(),ICBOX,name);
        if(index>=s.model().node)return fail(c.name,"bounds must name differential variables",p);
        if(bounds[0]>bounds[1])return fail(c.name,"each lower bound must be at most its upper bound",p);
        settings.xlo[index]=bounds[0];settings.xhi[index]=bounds[1];
    }
    settings.n=static_cast<int>(a.get<long long>("guesses"));settings.tol=a.get<double>("tolerance");
    if(const auto ready=prepare_equilibrium(s,c,p);!ready)return std::unexpected(ready.error());
    s.integrator.fixptguess=std::move(settings);
    s.equilibrium_append=a.optional<bool>("append").value_or(s.equilibrium_append);
    s.equilibrium_shoot=a.optional<bool>("shoot").value_or(s.equilibrium_shoot);
    if(const auto run=do_monte_carlo_search(s,s.equilibrium_append,1,s.equilibrium_shoot);!run)return std::unexpected(run.error());
    const auto &points=s.integrator.fixptlist;
    std::string result=xpp::format("{{\"found\":{},\"equilibria\":[",points.n);
    for(int i=0;i<points.n;++i){if(i)result+=',';result+=values_json(s,points.x[i]);}
    return result+"]}";
}
Result<std::string> import_as_ic(Session &s,const Command &c,const Arguments &,const Place &p)
{
    if(s.last_equilibrium.empty())return fail(c.name,"No equilibrium has been found",p);
    eq_import(s,s.last_equilibrium.data(),static_cast<int>(s.last_equilibrium.size()));
    return "{\"applied\":"+values_json(s,s.last_equilibrium)+'}';
}
Result<std::string> export_file(Session &s,const Command &,const Arguments &a,const Place &)
{
    const auto &file=a.get<std::string>("file");
    const bool shoot=a.optional<bool>("shoot").value_or(s.equilibrium_shoot);
    if(const auto saved=write_equilibrium(s,file.c_str(),shoot,a.optional<bool>("confirm").value_or(false));!saved)return std::unexpected(saved.error());
    s.equilibrium_shoot=shoot;
    std::string result="{\"file\":";json_append_string(result,file);return result+",\"written\":true}";
}
/* the first CONFIG_NUMBERS keys are numbers (the first a whole one), the rest booleans */
constexpr std::size_t CONFIG_NUMBERS=4;
constexpr std::array CONFIG_KEYS={"max_iterations","newton_tolerance","jacobian_step","search_limit","print_eigenvalues","draw_invariant_sets","draw_strong_sets"};
constexpr std::array CONFIG_ARGS={"max_iterates","newton_tolerance","jacobian_epsilon","search_limit","print_eigenvalues","draw_invariant_sets","draw_strong_sets"};
Result<std::string> configure(Session &s,const Command &c,const Arguments &a,const Place &p)
{
    std::array<std::string,CONFIG_KEYS.size()> values;
    for(std::size_t i=0;i<values.size();++i){
        if(!a.has(CONFIG_ARGS[i]))continue;
        if(i>=CONFIG_NUMBERS)values[i]=xpp::format("{}",a.get<bool>(CONFIG_ARGS[i]));
        else values[i]=i==0?xpp::format("{}",a.get<long long>(CONFIG_ARGS[i])):xpp::number(a.get<double>(CONFIG_ARGS[i]));
        std::string why;
        if(numerics_settings_check(s,CONFIG_KEYS[i],values[i],why)<0)return fail(c.name,why,p);
    }
    for(std::size_t i=0;i<values.size();++i)if(!values[i].empty()){
        std::string why;
        if(numerics_settings_set(s,CONFIG_KEYS[i],values[i],why,&p)<0)return fail(c.name,why,p);
    }
    const auto &n=s.numerics.equilibria;
    return xpp::format("{{\"settings\":{{\"max_iterates\":{},\"newton_tolerance\":{},\"jacobian_epsilon\":{},\"search_limit\":{}}},\"print_eigenvalues\":{},\"draw_invariant_sets\":{},\"draw_strong_sets\":{}}}",
        n.max_iterations,n.newton_tolerance,n.jacobian_step,n.search_limit,n.print_eigenvalues,n.draw_invariant_sets,n.draw_strong_sets);
}
constexpr std::array GUESS={Argument{"coordinates",Type::map,true,MAX_COMMAND_BYTES,{},{},{},NameSet::variable,Type::real},Argument{"window",Type::window,false}};
constexpr std::array RANGE={Argument{"parameter",Type::model_name,true,MAX_COMMAND_BYTES,{},{},{},NameSet::parameter},Argument{"start",Type::real,true},Argument{"end",Type::real,true},
    Argument{"steps",Type::integer,true,MAX_TEXT_BYTES,{},constant(1),constant(std::numeric_limits<int>::max()-1)},Argument{"shoot",Type::boolean,false},
    Argument{"stability_column",Type::integer,false,MAX_TEXT_BYTES,{},constant(0),session_quantity("columns",[](const Session &s){return static_cast<double>(s.model().neq+1);})},
    Argument{"movie",Type::boolean,false},Argument{"monte_carlo",Type::boolean,false}};
constexpr std::array SEARCH={Argument{"append",Type::boolean,false},Argument{"guesses",Type::integer,true,MAX_TEXT_BYTES,{},constant(1),constant(std::numeric_limits<int>::max())},
    Argument{"tolerance",Type::real,true,MAX_TEXT_BYTES,{},constant(std::numeric_limits<double>::denorm_min())},Argument{"shoot",Type::boolean,false},
    Argument{"bounds",Type::map,true,MAX_COMMAND_BYTES,{},{},{},NameSet::variable,Type::real_pair}};
constexpr std::array FILE={Argument{"file",Type::file,true},Argument{"shoot",Type::boolean,false},Argument{"confirm",Type::boolean,false}};
constexpr std::array CONFIG={Argument{"max_iterates",Type::integer,false,MAX_TEXT_BYTES,{},constant(1),constant(std::numeric_limits<int>::max())},
    Argument{"newton_tolerance",Type::real,false},Argument{"jacobian_epsilon",Type::real,false},Argument{"search_limit",Type::real,false},
    Argument{"print_eigenvalues",Type::boolean,false},Argument{"draw_invariant_sets",Type::boolean,false},Argument{"draw_strong_sets",Type::boolean,false}};
constexpr std::array FOUND={ResultField{"converged","boolean"},ResultField{"equilibrium","map[name, real]"},ResultField{"eigenvalues","list[map[string, real]]"},ResultField{"stability","choice"}};
constexpr std::array POINTS={ResultField{"points_found","integer"}};
constexpr std::array SEARCHED={ResultField{"found","integer"},ResultField{"equilibria","list[map[name, real]]"}};
constexpr std::array APPLIED={ResultField{"applied","map[name, real]"}};
constexpr std::array EXPORTED={ResultField{"file","file"},ResultField{"written","boolean"}};
constexpr std::array SETTINGS={ResultField{"settings","map[string, real]"},ResultField{"print_eigenvalues","boolean"},ResultField{"draw_invariant_sets","boolean"},ResultField{"draw_strong_sets","boolean"}};
constexpr std::array EVENTS={"equilibrium","marks","series","state","done"};
constexpr std::array DONE={"done"};
constexpr std::array ROWS={
    Command{"equilibrium.find",{},XPP_KIND_COMPUTE,CommandCategory::Analysis,"Analysis/Equilibria/Go","Find an equilibrium from the current initial conditions.",FOUND,EVENTS,true,find_point},
    Command{"equilibrium.guess",GUESS,XPP_KIND_COMPUTE,CommandCategory::Analysis,"Analysis/Equilibria/Mouse","Find an equilibrium from a guess in plot coordinates.",FOUND,EVENTS,true,guess},
    Command{"equilibrium.range",RANGE,XPP_KIND_COMPUTE,CommandCategory::Analysis,"Analysis/Equilibria/Range","Find equilibria over a parameter range.",POINTS,EVENTS,true,range},
    Command{"equilibrium.search",SEARCH,XPP_KIND_COMPUTE,CommandCategory::Analysis,"Analysis/Equilibria/Monte Carlo","Search for equilibria within variable bounds.",SEARCHED,EVENTS,true,search},
    Command{"equilibrium.import_as_ic",{},XPP_KIND_SETTING,CommandCategory::Analysis,"Analysis/Equilibrium/Import","Import the last equilibrium as initial conditions.",APPLIED,EVENTS,true,import_as_ic},
    Command{"equilibrium.export",FILE,XPP_KIND_DATA,CommandCategory::Analysis,"Analysis/export","Write the equilibrium and its eigenvalues.",EXPORTED,DONE,true,export_file,{}, {}, "write", "*.dat"},
    Command{"equilibrium.configure",CONFIG,XPP_KIND_SETTING,CommandCategory::Analysis,"Analysis/Equilibria/Settings","Configure the equilibrium search: Newton's method and what a found point prints and draws.",SETTINGS,EVENTS,true,configure}};
}
std::span<const Command> equilibrium_commands(){return ROWS;}
}
