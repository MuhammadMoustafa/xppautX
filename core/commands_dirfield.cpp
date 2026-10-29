/* Direction fields and flows' typed commands (W522). */
#include "command_def.h"
#include "command_registry.h"
#include "xpp_ui.h"
#include "nullcline.h"
#include "session.h"
#include "graf_par.h"
#include "model_options.h"
#include "xpp_ui.h"
#include "json_reader.h"
#include "xpp_io.h"
#include <array>

namespace xpp::command {
namespace {
Result<std::string> draw(Session &s,const Command &c,const Arguments &a,const Place &p,int mode,const char *count)
{
    if(!phase_plane(*s.plot_windows.current))return fail(c.name,"Not in a usable 2D plane",p);
    const int grid=static_cast<int>(a.get<long long>("grid"));
    if(const auto drawn=draw_direction_field(s,mode,grid);!drawn)return std::unexpected(drawn.error());
    if(mode!=1){create_new_cline(s);ui.redraw_graph(s);}
    const long long points=static_cast<long long>(grid+1)*(grid+1);
    return xpp::format("{{\"grid\":{},\"{}\":{}}}",grid,count,mode==1?2*points:points);
}
Result<std::string> compute(Session &s,const Command &c,const Arguments &a,const Place &p)
{
    return draw(s,c,a,p,a.optional<bool>("scaled").value_or(s.nullcline_state.dfield_type==0)?4:0,"vectors_drawn");
}
Result<std::string> flow(Session &s,const Command &c,const Arguments &a,const Place &p){return draw(s,c,a,p,1,"trajectories_run");}
Result<std::string> colorize(Session &s,const Command &c,const Arguments &a,const Place &p){return draw(s,c,a,p,3,"cells_colored");}
Result<std::string> clear(Session &s,const Command &,const Arguments &,const Place &)
{
    s.nullclines.df_flag=0;
    create_new_cline(s);ui.redraw_graph(s);
    return "{\"cleared\":true}";
}
Result<std::string> export_file(Session &s,const Command &c,const Arguments &a,const Place &p)
{
    if(!s.nullclines.df_flag)return fail(c.name,"No direction field is shown",p);
    const auto &file=a.get<std::string>("file");
    if(const auto saved=write_dfield(s,file,a.optional<bool>("confirm").value_or(false));!saved)return std::unexpected(saved.error());
    std::string result="{\"file\":";json_append_string(result,file);
    return result+",\"written\":true}";
}
constexpr Argument GRID={"grid",Type::integer,true,MAX_TEXT_BYTES,{},constant(2),constant(option_limit::mesh)};
constexpr std::array COMPUTE={GRID,Argument{"scaled",Type::boolean,false}};
constexpr std::array GRID_ARGS={GRID};
constexpr std::array FILE={Argument{"file",Type::file,true},Argument{"confirm",Type::boolean,false}};
constexpr std::array VECTORS={ResultField{"grid","integer"},ResultField{"vectors_drawn","integer"}};
constexpr std::array TRAJECTORIES={ResultField{"grid","integer"},ResultField{"trajectories_run","integer"}};
constexpr std::array CELLS={ResultField{"grid","integer"},ResultField{"cells_colored","integer"}};
constexpr std::array CLEARED={ResultField{"cleared","boolean"}};
constexpr std::array EXPORTED={ResultField{"file","file"},ResultField{"written","boolean"}};
constexpr std::array EVENTS={"dfield","flow","nullclines","state","done"};
constexpr std::array DONE={"done"};
constexpr std::array ROWS={
    Command{"dirfield.compute",COMPUTE,XPP_KIND_COMPUTE,CommandCategory::Analysis,"Analysis/Two-D Fun/Direct or Scaled Field","Compute the phase plane's direction field.",VECTORS,EVENTS,true,compute},
    Command{"dirfield.flow",GRID_ARGS,XPP_KIND_COMPUTE,CommandCategory::Analysis,"Analysis/Two-D Fun/Flow","Integrate trajectories on the phase plane grid.",TRAJECTORIES,EVENTS,true,flow},
    Command{"dirfield.clear",{},XPP_KIND_VIEW,CommandCategory::Analysis,"Analysis/Two-D Fun/No dir. fld.","Clear the direction field.",CLEARED,EVENTS,true,clear},
    Command{"dirfield.colorize",GRID_ARGS,XPP_KIND_COMPUTE,CommandCategory::Analysis,"Analysis/Two-D Fun/Colorize","Color the phase plane grid.",CELLS,EVENTS,true,colorize},
    Command{"dirfield.export",FILE,XPP_KIND_DATA,CommandCategory::Analysis,"Analysis/export","Write direction field arrows.",EXPORTED,DONE,true,export_file,{}, {}, "write", "*.dat"}};
}
std::span<const Command> dirfield_commands(){return ROWS;}
}

namespace xpp::command {
Result<> dirfield_button(Session &s,int button,const Place &place)
{
    constexpr std::array names={"dirfield.compute","dirfield.flow","dirfield.clear","dirfield.colorize","dirfield.compute"};
    if(button<0||static_cast<std::size_t>(button)>=names.size())return fail("dirfield button","Unknown imported direction field button",place);
    std::string line="{\"cmd\":";json_append_string(line,names[button]);
    if(button!=2){
        int grid=s.nullclines.df_grid;new_int("Grid:",&grid);
        line+=xpp::format(",\"grid\":{}",grid);
        if(button==0||button==4)line+=xpp::format(",\"scaled\":{}",button==4);
    }
    return run_button(s,line+'}',place);
}
}
