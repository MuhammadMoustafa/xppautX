/* Nullclines' typed commands (W522), sharing nullcline.cpp's numerical routines. */
#include "command_def.h"
#include "integrate.h"
#include <limits>
#include "command_registry.h"
#include "xpp_ui.h"
#include "nullcline.h"
#include "data_formats.h"
#include "session.h"
#include "numerics_settings.h"
#include "model_options.h"
#include "getvar.h"
#include "graf_par.h"
#include "json_reader.h"
#include "xpp_io.h"
#include <array>

namespace xpp::command {
namespace {
Result<std::string> compute(Session &s,const Command &c,const Arguments &,const Place &p)
{
    if(!phase_plane(*s.plot_windows.current))return fail(c.name,"Not in a usable 2D plane",p);
    compute_nullclines(s);
    if(s.integrator.step_error)return std::unexpected(take_step_error(s).error());
    return xpp::format("{{\"curves_computed\":{}}}",s.numerics.null_here?2:0);
}
Result<std::string> redraw(Session &s,const Command &,const Arguments &,const Place &)
{
    return xpp::format("{{\"drawn\":{}}}",restore_nullclines(s));
}
Result<std::string> auto_mode(Session &s,const Command &,const Arguments &a,const Place &)
{
    s.plot_windows.current->Nullrestore=a.get<bool>("enabled");
    return xpp::format("{{\"auto_redraw\":{}}}",a.get<bool>("enabled"));
}
Result<std::string> freeze(Session &s,const Command &c,const Arguments &,const Place &p)
{
    if(s.nullcline_state.frozen.size()>=max_frozen_clines)return fail(c.name,"The frozen nullcline list is full",p);
    freeze_nullclines(s);
    return xpp::format("{{\"frozen_curves_count\":{}}}",s.nullcline_state.frozen.size());
}
Result<std::string> clear_frozen(Session &s,const Command &,const Arguments &,const Place &)
{
    clear_frozen_nullclines(s);
    return "{\"cleared\":true}";
}
Result<std::string> range_freeze(Session &s,const Command &c,const Arguments &a,const Place &p)
{
    if(!phase_plane(*s.plot_windows.current))return fail(c.name,"Not in a usable 2D plane",p);
    if(a.get<double>("end")<=a.get<double>("start"))return fail(c.name,"end must be above start",p);
    if(static_cast<std::size_t>(a.get<long long>("steps"))+1>max_frozen_clines-s.nullcline_state.frozen.size())return fail(c.name,"The range exceeds the remaining frozen nullcline capacity",p);
    auto &range=s.nullcline_state.range;
    range.rv=a.get<std::string>("parameter");
    range.xlo=a.get<double>("start");range.xhi=a.get<double>("end");
    range.nstep=static_cast<int>(a.get<long long>("steps"));
    const auto before=s.nullcline_state.frozen.size();
    range_nullclines(s);
    if(s.integrator.step_error)return std::unexpected(take_step_error(s).error());
    return xpp::format("{{\"steps_frozen\":{}}}",s.nullcline_state.frozen.size()-before);
}
Result<std::string> animate(Session &s,const Command &,const Arguments &a,const Place &)
{
    const bool drawn=animate_nullclines(s,static_cast<int>(a.optional<long long>("delay_ms").value_or(s.nullcline_state.animation_delay_ms)));
    return xpp::format("{{\"animated\":{}}}",drawn);
}
Result<std::string> export_file(Session &s,const Command &,const Arguments &a,const Place &)
{
    const auto &file=a.get<std::string>("file");
    const auto written=export_nullclines(s,file,a.optional<std::string>("format").value_or(""),a.optional<bool>("confirm").value_or(false));
    if(!written)return std::unexpected(written.error());
    std::string result="{\"file\":";json_append_string(result,file);
    return result+xpp::format(",\"points_written\":{}}}",*written);
}
Result<std::string> set_mesh(Session &s,const Command &c,const Arguments &a,const Place &p)
{
    std::string why;
    if(numerics_settings_set(s,"nullcline_mesh",xpp::format("{}",a.get<long long>("mesh")),why,&p)<0)return fail(c.name,why,p);
    return xpp::format("{{\"nullcline_mesh\":{}}}",s.numerics.nullcline_mesh);
}
constexpr std::array ENABLED={Argument{"enabled",Type::boolean,true}};
constexpr std::array RANGE={Argument{"parameter",Type::model_name,true,MAX_COMMAND_BYTES,{},{},{},NameSet::parameter},
    Argument{"start",Type::real,true},Argument{"end",Type::real,true},
    Argument{"steps",Type::integer,true,MAX_TEXT_BYTES,{},constant(1),constant(max_frozen_clines-1)}};
constexpr std::array DELAY={Argument{"delay_ms",Type::integer,false,MAX_TEXT_BYTES,{},constant(0),constant(std::numeric_limits<int>::max())}};
constexpr std::array FILE={Argument{"file",Type::file,true},Argument{"format",Type::choice,false,MAX_TEXT_BYTES,DATA_FORMAT_NAMES},Argument{"confirm",Type::boolean,false}};
constexpr std::array MESH={Argument{"mesh",Type::integer,true,MAX_TEXT_BYTES,{},constant(1),constant(option_limit::mesh)}};
constexpr std::array COMPUTED={ResultField{"curves_computed","integer"}};
constexpr std::array DRAWN={ResultField{"drawn","boolean"}};
constexpr std::array AUTO={ResultField{"auto_redraw","boolean"}};
constexpr std::array FROZEN={ResultField{"frozen_curves_count","integer"}};
constexpr std::array CLEARED={ResultField{"cleared","boolean"}};
constexpr std::array STEPS={ResultField{"steps_frozen","integer"}};
constexpr std::array ANIMATED={ResultField{"animated","boolean"}};
constexpr std::array EXPORTED={ResultField{"file","file"},ResultField{"points_written","integer"}};
constexpr std::array MESH_RESULT={ResultField{"nullcline_mesh","integer"}};
constexpr std::array EVENTS={"nullclines","state","done"};
constexpr std::array DONE={"done"};
constexpr std::array ROWS={
    Command{"nullcline.compute",{},XPP_KIND_COMPUTE,CommandCategory::Analysis,"Analysis/Nullclines/New","Compute the phase plane's nullclines.",COMPUTED,EVENTS,true,compute},
    Command{"nullcline.redraw",{},XPP_KIND_VIEW,CommandCategory::Analysis,"Analysis/Nullclines/Restore","Draw stored nullclines again.",DRAWN,EVENTS,true,redraw},
    Command{"nullcline.auto_mode",ENABLED,XPP_KIND_VIEW,CommandCategory::Analysis,"Analysis/Nullclines/Auto or Manual","Set automatic nullcline redrawing.",AUTO,EVENTS,true,auto_mode},
    Command{"nullcline.freeze",{},XPP_KIND_VIEW,CommandCategory::Analysis,"Analysis/Nullclines/Freeze/Freeze","Freeze the current nullclines.",FROZEN,EVENTS,true,freeze},
    Command{"nullcline.clear_frozen",{},XPP_KIND_VIEW,CommandCategory::Analysis,"Analysis/Nullclines/Freeze/Delete all","Clear frozen nullclines.",CLEARED,EVENTS,true,clear_frozen},
    Command{"nullcline.range_freeze",RANGE,XPP_KIND_COMPUTE,CommandCategory::Analysis,"Analysis/Nullclines/Freeze/Range","Freeze nullclines over a parameter range.",STEPS,EVENTS,true,range_freeze},
    Command{"nullcline.animate",DELAY,XPP_KIND_VIEW,CommandCategory::Analysis,"Analysis/Nullclines/Freeze/Animate","Replay frozen nullclines.",ANIMATED,DONE,true,animate},
    Command{"nullcline.export",FILE,XPP_KIND_DATA,CommandCategory::Analysis,"Analysis/Nullclines/Save","Write the current nullcline segments.",EXPORTED,DONE,true,export_file,{}, {}, "write", "*.dat"},
    Command{"nullcline.set_mesh",MESH,XPP_KIND_SETTING,CommandCategory::Analysis,"Analysis/Numerics/Nullcline mesh","Set the nullcline mesh.",MESH_RESULT,EVENTS,true,set_mesh}};
}
std::span<const Command> nullcline_commands(){return ROWS;}
}

namespace xpp::command {
Result<> nullcline_button(Session &s,int button,const Place &place)
{
    constexpr std::array names={"nullcline.compute","nullcline.redraw","nullcline.auto_mode","nullcline.auto_mode",
        "nullcline.export","nullcline.freeze","nullcline.clear_frozen","nullcline.range_freeze","nullcline.animate"};
    if(button<0||static_cast<std::size_t>(button)>=names.size())return fail("nullcline button","Unknown imported nullcline button",place);
    std::string line="{\"cmd\":";json_append_string(line,names[button]);
    if(button==2||button==3)line+=xpp::format(",\"enabled\":{}",button==2);
    if(button==4){
        std::string file=s.nullclines.file;
        if(!file_selector("Save nullclines",file,"*.dat"))return {};
        line+=",\"file\":";json_append_string(line,file);
    }
    if(button==7){
        auto range=s.nullcline_state.range;
        if(!new_string("Parameter",range.rv))return {};
        if(new_float(s,"Start",&range.xlo)==-1||new_float(s,"End",&range.xhi)==-1)return {};
        new_int("Steps",&range.nstep);
        line+=",\"parameter\":";json_append_string(line,range.rv);
        line+=xpp::format(",\"start\":{},\"end\":{},\"steps\":{}",range.xlo,range.xhi,range.nstep);
    }
    if(button==8){int delay=s.nullcline_state.animation_delay_ms;new_int("Delay (msec)",&delay);line+=xpp::format(",\"delay_ms\":{}",delay);}
    return run_button(s,line+'}',place);
}
}
