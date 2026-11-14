/* The plot windows' bookkeeping (many_pops.h's XppPlotWindows): creating, activating and destroying graphs, and checking their bounds. */
#include "many_pops.h"
#include <tuple>
#include "xpp_util.h"
#include "xpp_ui.h"
#include "xpp_files.h"
#include "graphics.h"
#include "graf_par.h"
#include "grobs.h"
#include "axes2.h"
#include "picture.h"
#include "image_format.h"
#include "integrate.h"
#include "derived.h"
#include "xpp_log.h"
#include "xpp_globals.h"
#include "expr.h"
#include "nullcline.h"
#include "plot_data.h"
#include "delay_handle.h"
#include "numerics.h"
#include "browse.h"
#include "form_ode.h"
#include "volterra2.h"
#include "tabular.h"
#include "session.h"
#include "aniparse.h"
#include "arrayplot.h"
#include "autox_state.h"
#include "display_state.h"
#include "marks_data.h"
#include "phase_data.h"
#include "model.h"

namespace xpp {

std::optional<WindowTarget> find_window(const Session &s, std::string_view name)
{
    for (int i=0;i<static_cast<int>(s.plot_windows->graph.size());++i)
        if (s.plot_windows->graph[i].Use && s.plot_windows->graph[i].name==name)
            return WindowTarget{WindowKind::graph,i};
    if (s.autox_state->bifur.exist)
        for (int i=0;i<static_cast<int>(s.autox_state->views.size());++i)
            if (s.autox_state->views[i].name==name) return WindowTarget{WindowKind::diagram,i};
    if (name==DATA_WINDOW) return WindowTarget{WindowKind::data,0};
    if (name==ANIMATION_WINDOW && s.animation->vcr.iexist) return WindowTarget{WindowKind::animation,0};
    if (name==ARRAY_PLOT_WINDOW && s.array_plot->plot.alive) return WindowTarget{WindowKind::array_plot,0};
    return std::nullopt;
}

std::string_view window_kind_name(WindowKind kind)
{
    switch (kind) {
    case WindowKind::graph: return "graph";
    case WindowKind::diagram: return "diagram";
    case WindowKind::data: return "data";
    case WindowKind::animation: return "animation";
    case WindowKind::array_plot: return "array_plot";
    }
    return {};
}

bool window_hidden(const Session &s, const WindowTarget &target)
{
    switch (target.kind) {
    case WindowKind::graph: return s.plot_windows->graph[target.index].hidden;
    case WindowKind::diagram: return s.autox_state->views[target.index].hidden;
    case WindowKind::data: return s.tools_hidden->data;
    case WindowKind::animation: return s.tools_hidden->animation;
    case WindowKind::array_plot: return s.tools_hidden->array_plot;
    }
    return false;
}

bool window_second(const Session &s, const WindowTarget &target)
{
    switch (target.kind) {
    case WindowKind::graph: return s.plot_windows->graph[target.index].second;
    case WindowKind::diagram: return s.autox_state->views[target.index].second;
    default: return false;
    }
}

void set_window_hidden(Session &s, const WindowTarget &target, bool hidden)
{
    switch (target.kind) {
    case WindowKind::graph: s.plot_windows->graph[target.index].hidden = hidden; break;
    case WindowKind::diagram: s.autox_state->views[target.index].hidden = hidden; break;
    case WindowKind::data: s.tools_hidden->data = hidden; break;
    case WindowKind::animation: s.tools_hidden->animation = hidden; break;
    case WindowKind::array_plot: s.tools_hidden->array_plot = hidden; break;
    }
}

std::string window_content(const Session &s, XppWinId w)
{
    const auto count = [](long n, const char *one, const char *many) { return xpp::format("{} {}", n, n == 1 ? one : many); };
    const long labels = std::ranges::count(s.labels, w, &LABEL::w), objects = std::ranges::count(s.grobs, w, &GROB::w),
               frozen = std::ranges::count_if(s.frozen_curves->curve, [w](const auto &c) { return c.use == 1 && c.w == w; });
    std::string held;
    for (const auto &[n, one, many] : {std::tuple{labels, "text", "texts"}, {objects, "arrow, pointer or marker", "arrows, pointers and markers"},
                                       {frozen, "frozen curve", "frozen curves"}})
        if (n > 0) held += (held.empty() ? "" : ", ") + count(n, one, many);
    return held;
}

/* the windows that are in a group of the split view: the graphs and the autoX views */
namespace {
bool grouped(WindowKind kind) { return kind == WindowKind::graph || kind == WindowKind::diagram; }
} // namespace

std::string windows_json(const Session &s)
{
    std::string o = "[";
    for (const WindowInfo &w : list_windows(s)) {
        if (o.size() > 1) o += ',';
        o += "{\"name\":";
        json_append_string(o, w.name);
        o += xpp::format(",\"kind\":\"{}\",\"shown\":{}", window_kind_name(w.kind), w.shown);
        if (grouped(w.kind)) o += xpp::format(",\"group\":\"{}\"", group_name(w.second));
        o += '}';
    }
    return o + ']';
}

std::string windows_fields(const Session &s) { return "\"windows\":" + windows_json(s) + ",\"split\":" + split_json(s); }

std::vector<WindowInfo> list_windows(const Session &s)
{
    std::vector<WindowInfo> windows;
    const auto add = [&](std::string name, const WindowTarget &target) {
        windows.push_back({std::move(name), target.kind, !window_hidden(s, target), window_second(s, target)});
    };
    for (const int i:s.plot_windows->open) add(s.plot_windows->graph[i].name, {WindowKind::graph,i});
    if (s.autox_state->bifur.exist)
        for (int i=0;i<static_cast<int>(s.autox_state->views.size());++i) add(s.autox_state->views[i].name, {WindowKind::diagram,i});
    if (s.model().loaded()) add(std::string(DATA_WINDOW), {WindowKind::data,0});
    if (s.animation->vcr.iexist) add(std::string(ANIMATION_WINDOW), {WindowKind::animation,0});
    if (s.array_plot->plot.alive) add(std::string(ARRAY_PLOT_WINDOW), {WindowKind::array_plot,0});
    return windows;
}

std::string next_window_name(const Session &s, WindowKind kind)
{
    for (std::size_t i=1;;++i) {
        std::string name=xpp::format("{}_{}",kind==WindowKind::graph?"graph":"diagram",i);
        if (!find_window(s,name) && (kind!=WindowKind::diagram || !std::ranges::any_of(s.autox_state->views,[&](const AutoxDiagramView &view){return view.name==name;}))) return name;
    }
}

std::string window_name(const Session &s, XppWinId handle)
{
    if (handle==WIN_AUTOX) return s.autox_state->views[s.autox_state->active_view].name;
    if (handle==WIN_ANI || handle==WIN_APLOT) return std::string(fixed_window_name(handle));
    return s.plot_windows->graph[graph_of(s,handle)].name;
}

std::string_view fixed_window_name(XppWinId handle)
{
    return handle==WIN_ANI ? ANIMATION_WINDOW : ARRAY_PLOT_WINDOW;
}

std::string window_name_problem(std::string_view name)
{
    if (name.empty() || name.size()>WINDOW_NAME_BYTES) return xpp::format("A window name has 1 to {} UTF-8 bytes",WINDOW_NAME_BYTES);
    if (name==DATA_WINDOW || name==ANIMATION_WINDOW || name==ARRAY_PLOT_WINDOW) return xpp::format("\"{}\" is the name of a window of its own",name);
    if (name.front()==' ' || name.back()==' ') return "A window name has no leading or trailing space";
    for (unsigned char ch:name) if (ch<0x20 || ch==0x7f) return "A window name has no control characters";
    return {};
}

std::string window_name_problem(const Session &s,std::string_view name,std::string_view replacing)
{
    if(auto problem=window_name_problem(name);!problem.empty())return problem;
    if (name!=replacing && find_window(s,name)) return xpp::format("Window \"{}\" is already open",name);
    return {};
}

/* ---- graph bookkeeping ----------------------------------------------- */

void create_a_pop(Session &s, std::string_view requested)
{
    int i;
    for (i = 1; i < static_cast<int>(s.plot_windows->graph.size()); i++)
        if (s.plot_windows->graph[i].Use == 0) break;
    if (i == static_cast<int>(s.plot_windows->graph.size())) {
        s.plot_windows->graph.emplace_back();
        s.plot_display.emplace_back();
        s.marks_shown->windows.emplace_back();
        s.phase_shown->windows.emplace_back();
        s.plot_windows->current = &s.plot_windows->graph[s.plot_windows->active];
    }
    const std::string name=requested.empty()?next_window_name(s,WindowKind::graph):std::string(requested);
    copy_graph(s, i, s.plot_windows->active);
    s.plot_windows->graph[i].name=name;
    s.plot_windows->graph[i].hidden=false;
    s.plot_windows->graph[i].second=false;
    s.plot_display[i]={};
    s.marks_shown->windows[i]={};
    s.phase_shown->windows[i]={};
    s.plot_windows->graph[i].x11Wid=extra_plot_width;
    s.plot_windows->graph[i].x11Hgt=extra_plot_height;
    s.plot_windows->graph[i].w = i + 1;
    // Protocol auxiliary windows occupy 101 through 105; plots skip this range.
    if(s.plot_windows->graph[i].w>=WIN_AUTOX)s.plot_windows->graph[i].w+=WIN_APLOT-WIN_AUTOX+1;
    set_active_windows(s);
    make_active(s, i, 1);
    ui.create_plot_window(s);
    get_draw_area(s);
}

void destroy_a_pop(Session &s)
{
    const int index=s.plot_windows->active;
    if(index==0){
        command_error("windows","Cannot destroy the main plot window");
        return;
    }
    kill_frz(s);
    destroy_labels_and_grobs(s,s.plot_windows->graph[index].w);
    ui.destroy_plot_window(s);
    s.plot_windows->graph[index].Use=0;
    s.plot_windows->graph[index].hidden=false;
    s.plot_windows->graph[index].second=false;
    make_active(s,0,1);
    set_active_windows(s);
}

void kill_all_pops(Session &s)
{
    for(int i=1;i<static_cast<int>(s.plot_windows->graph.size());i++){
        if(!s.plot_windows->graph[i].Use)continue;
        make_active(s,i,1);
        destroy_a_pop(s);
    }
    make_active(s,0,1);
}

void make_active(xpp::Session &s, int i, int flag)
{
    s.plot_windows->active = i;
    s.plot_windows->current = &s.plot_windows->graph[s.plot_windows->active];
    s.plot_windows->draw_win = s.plot_windows->current->w;
    s.selected_window=s.plot_windows->current->name;
    get_draw_area_flag(s, flag);
}

int graph_of(const xpp::Session &s, XppWinId w)
{
    for (int i = 0; i < static_cast<int>(s.plot_windows->graph.size()); i++)
        if (s.plot_windows->graph[i].Use && s.plot_windows->graph[i].w == w) return i;
    return 0;
}

void clr_scrn(xpp::Session &s)
{
    ui.blank_draw_window(s);
    do_axes(s);
}
void clear_windows(xpp::Session &s)
{
    clr_all_scrns(s);
    plot_data_picture(s, 0); /* and blanks its picture */
    s.nullclines->df_flag = 0;
}

void set_active_windows(xpp::Session &s)
{
    /* the tab order is kept (move_window): a graph that closed leaves it, one that opened goes last */
    std::vector<int> &open=s.plot_windows->open;
    const std::vector<GRAPH> &graph=s.plot_windows->graph;
    const auto used=[&](int i){return i<static_cast<int>(graph.size()) && graph[i].Use==1;};
    std::erase_if(open,[&](int i){return !used(i);});
    for(int i=0;i<static_cast<int>(graph.size());i++)
        if(used(i) && std::ranges::find(open,i)==open.end())open.push_back(i);
    s.plot_windows->count=static_cast<int>(open.size());
}

long long window_position(const Session &s, const WindowTarget &target)
{
    if (target.kind==WindowKind::graph) return std::ranges::find(s.plot_windows->open,target.index)-s.plot_windows->open.begin();
    return target.index;
}

std::string move_window(Session &s, const WindowTarget &target, long long position)
{
    const auto out_of_range=[&](long long first,std::size_t count){
        return xpp::format("Position {} is not a place among the {}: {} to {}",position,target.kind==WindowKind::graph?"graphs":"autoX views",first,count-1);
    };
    if (target.kind==WindowKind::graph) {
        std::vector<int> &open=s.plot_windows->open;
        if (target.index==0) return "The main graph stays first";
        if (position<1 || position>=static_cast<long long>(open.size())) return out_of_range(1,open.size());
        open.erase(std::ranges::find(open,target.index));
        open.insert(open.begin()+static_cast<std::ptrdiff_t>(position),target.index);
        return {};
    }
    std::vector<AutoxDiagramView> &views=s.autox_state->views;
    if (position<0 || position>=static_cast<long long>(views.size())) return out_of_range(0,views.size());
    const std::string active=views[static_cast<std::size_t>(s.autox_state->active_view)].name;
    const auto from=views.begin()+target.index, to=views.begin()+static_cast<std::ptrdiff_t>(position);
    if (from<to) std::rotate(from,from+1,to+1);
    else std::rotate(to,from,from+1);
    s.autox_state->active_view=static_cast<int>(std::ranges::find(views,active,&AutoxDiagramView::name)-views.begin());
    return {};
}

std::string_view split_direction_name(SplitDirection direction)
{
    switch (direction) {
    case SplitDirection::none: return "none";
    case SplitDirection::right: return "right";
    case SplitDirection::down: return "down";
    }
    return {};
}

std::optional<SplitDirection> split_direction_from_name(std::string_view name)
{
    for (const SplitDirection d : {SplitDirection::none, SplitDirection::right, SplitDirection::down})
        if (split_direction_name(d) == name) return d;
    return std::nullopt;
}

std::optional<bool> group_second_from_name(std::string_view name)
{
    if (name == GROUP_FIRST) return false;
    if (name == GROUP_SECOND) return true;
    return std::nullopt;
}

namespace {

void set_window_second(Session &s, const WindowTarget &target, bool second)
{
    if (target.kind == WindowKind::graph) s.plot_windows->graph[target.index].second = second;
    else s.autox_state->views[target.index].second = second;
}

/* the first move's check: only a graph or an autoX view is in a group */
std::string group_problem(const WindowTarget &target)
{
    if (!grouped(target.kind))
        return xpp::format("The {} window is not in a group: only a graph or an autoX view is", window_kind_name(target.kind));
    if (target.kind == WindowKind::graph && target.index == 0) return "The main graph stays in the first group";
    return {};
}

} // namespace

std::string split_window(Session &s, const WindowTarget &target, SplitDirection direction)
{
    if (const std::string problem = group_problem(target); !problem.empty()) return problem;
    if (direction == SplitDirection::none) return "A split goes right or down";
    set_window_second(s, target, true);
    s.plot_windows->split.direction = direction;
    return {};
}

std::string join_window(Session &s, const WindowTarget &target)
{
    if (const std::string problem = group_problem(target); !problem.empty()) return problem;
    if (!window_second(s, target)) return "The window is already in the first group";
    set_window_second(s, target, false);
    return {};
}

std::string set_split_share(Session &s, double share)
{
    if (s.plot_windows->split.direction == SplitDirection::none) return "There is no split: window.split makes one";
    if (!(share >= SPLIT_SHARE_MIN && share <= 1 - SPLIT_SHARE_MIN))
        return xpp::format("The first group's share {} is not between {} and {}", xpp::number(share), xpp::number(SPLIT_SHARE_MIN), xpp::number(1 - SPLIT_SHARE_MIN));
    s.plot_windows->split.share = share;
    return {};
}

void split_upkeep(Session &s)
{
    WindowSplit &split = s.plot_windows->split;
    const std::vector<WindowInfo> windows = list_windows(s);
    if (!std::ranges::any_of(windows, [](const WindowInfo &w) { return w.second; })) {
        split = {};
        return;
    }
    const auto in_group = [&](const WindowInfo &w, bool second) { return grouped(w.kind) && w.second == second; };
    const auto selected = std::ranges::find(windows, s.selected_window, &WindowInfo::name);
    for (const bool second : {false, true}) {
        std::string &shown = split.shown[second];
        if (selected != windows.end() && in_group(*selected, second)) {
            shown = selected->name;
            continue;
        }
        const auto kept = std::ranges::find(windows, shown, &WindowInfo::name);
        if (kept != windows.end() && in_group(*kept, second) && kept->shown) continue;
        const auto first = std::ranges::find_if(windows, [&](const WindowInfo &w) { return in_group(w, second) && w.shown; });
        shown = first == windows.end() ? std::string() : first->name;
    }
}

std::string split_json(const Session &s)
{
    const WindowSplit &split = s.plot_windows->split;
    std::string o = xpp::format("{{\"direction\":\"{}\",\"share\":{},\"shown\":{{\"first\":", split_direction_name(split.direction), xpp::number(split.share));
    json_append_string(o, split.shown[false]);
    o += ",\"second\":";
    json_append_string(o, split.shown[true]);
    o += "},\"selected\":";
    json_append_string(o, s.selected_window);
    return o + '}';
}

void check_windows(xpp::Session &s)
{
 double zip,zap;
 check_val(&s.plot_windows->current->xmin,&s.plot_windows->current->xmax,&s.plot_windows->current->xbar,&s.plot_windows->current->dx);
 check_val(&s.plot_windows->current->ymin,&s.plot_windows->current->ymax,&s.plot_windows->current->ybar,&s.plot_windows->current->dy);
 check_val(&s.plot_windows->current->zmin,&s.plot_windows->current->zmax,&s.plot_windows->current->zbar,&s.plot_windows->current->dz);
 check_val(&s.plot_windows->current->xlo,&s.plot_windows->current->xhi,&zip,&zap);
 check_val(&s.plot_windows->current->ylo,&s.plot_windows->current->yhi,&zip,&zap);
} 

/* graf_par.h's */
void check_val(double *x1, double *x2, double *xb, double *xd)
{
 double temp;

/* 
  see get_max for details
*/   
      
 if(*x1==*x2){
   temp=.05*lmax(fabs(*x1),1.0);
   *x1=*x1-temp;
   *x2=*x2+temp;
 }
 if(*x1>*x2){
	     temp=*x2;
             *x2=*x1;
             *x1=temp;
	     
            }
	    *xb=.5*(*x1+*x2);
	    *xd=2.0/(*x2-*x1);

}

void dump_ps(xpp::Session &s, int i)
{
  const xpp::ImageFormat *fmt=xpp::picture_format_of_file(s.picture_file);
  if(xpp::ok_or_show(xpp::begin_picture(s,*fmt,xpp::files::run_file_name(s.picture_file,i))) && s.plot_file->writer)
    xpp::ok_or_show(vector_restore(s));
}

ActiveGraph::ActiveGraph(Session &s,int index):s_(s),previous_(s.plot_windows->active),selected_(s.selected_window)
{
    if(index!=previous_)make_active(s_,index,1);
}
ActiveGraph::~ActiveGraph()
{
    if(s_.plot_windows->active!=previous_)make_active(s_,previous_,1);
    s_.selected_window=selected_;
}
} // namespace xpp
