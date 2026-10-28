/* The plot windows' bookkeeping (many_pops.h's XppPlotWindows): creating, activating and destroying graphs, and checking their bounds. */
#include "many_pops.h"
#include "session.h"
#include <tuple>
#include "xpp_util.h"
#include "xpp_ui.h"
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

namespace xpp {

std::optional<WindowTarget> find_window(const Session &s, std::string_view name)
{
    for (int i=0;i<static_cast<int>(s.plot_windows.graph.size());++i)
        if (s.plot_windows.graph[i].Use && s.plot_windows.graph[i].name==name)
            return WindowTarget{WindowKind::graph,i};
    if (s.auto_state.bifur.exist)
        for (int i=0;i<static_cast<int>(s.auto_state.views.size());++i)
            if (s.auto_state.views[i].name==name) return WindowTarget{WindowKind::diagram,i};
    if (name==BROWSER_WINDOW) return WindowTarget{WindowKind::browser,0};
    if (name==ANIMATION_WINDOW && s.animation.vcr.iexist) return WindowTarget{WindowKind::animation,0};
    if (name==ARRAY_PLOT_WINDOW && s.array_plot.plot.alive) return WindowTarget{WindowKind::array_plot,0};
    return std::nullopt;
}

std::string_view window_kind_name(WindowKind kind)
{
    switch (kind) {
    case WindowKind::graph: return "graph";
    case WindowKind::diagram: return "diagram";
    case WindowKind::browser: return "browser";
    case WindowKind::animation: return "animation";
    case WindowKind::array_plot: return "array_plot";
    }
    return {};
}

bool window_hidden(const Session &s, const WindowTarget &target)
{
    switch (target.kind) {
    case WindowKind::graph: return s.plot_windows.graph[target.index].hidden;
    case WindowKind::diagram: return s.auto_state.views[target.index].hidden;
    case WindowKind::browser: return s.tools_hidden.browser;
    case WindowKind::animation: return s.tools_hidden.animation;
    case WindowKind::array_plot: return s.tools_hidden.array_plot;
    }
    return false;
}

void set_window_hidden(Session &s, const WindowTarget &target, bool hidden)
{
    switch (target.kind) {
    case WindowKind::graph: s.plot_windows.graph[target.index].hidden = hidden; break;
    case WindowKind::diagram: s.auto_state.views[target.index].hidden = hidden; break;
    case WindowKind::browser: s.tools_hidden.browser = hidden; break;
    case WindowKind::animation: s.tools_hidden.animation = hidden; break;
    case WindowKind::array_plot: s.tools_hidden.array_plot = hidden; break;
    }
}

std::string window_content(const Session &s, XppWinId w)
{
    const auto count = [](long n, const char *one, const char *many) { return xpp::format("{} {}", n, n == 1 ? one : many); };
    const long labels = std::ranges::count(s.labels, w, &LABEL::w), objects = std::ranges::count(s.grobs, w, &GROB::w),
               frozen = std::ranges::count_if(s.frozen_curves.curve, [w](const auto &c) { return c.use == 1 && c.w == w; });
    std::string held;
    for (const auto &[n, one, many] : {std::tuple{labels, "text", "texts"}, {objects, "arrow, pointer or marker", "arrows, pointers and markers"},
                                       {frozen, "frozen curve", "frozen curves"}})
        if (n > 0) held += (held.empty() ? "" : ", ") + count(n, one, many);
    return held;
}

std::string windows_json(const Session &s)
{
    std::string o = "[";
    for (const WindowInfo &w : list_windows(s)) {
        if (o.size() > 1) o += ',';
        o += "{\"name\":";
        json_append_string(o, w.name);
        o += xpp::format(",\"kind\":\"{}\",\"shown\":{}}}", window_kind_name(w.kind), w.shown);
    }
    return o + ']';
}

std::vector<WindowInfo> list_windows(const Session &s)
{
    std::vector<WindowInfo> windows;
    const auto add = [&](std::string name, const WindowTarget &target) {
        windows.push_back({std::move(name), target.kind, !window_hidden(s, target)});
    };
    for (int i=0;i<static_cast<int>(s.plot_windows.graph.size());++i)
        if (s.plot_windows.graph[i].Use) add(s.plot_windows.graph[i].name, {WindowKind::graph,i});
    if (s.auto_state.bifur.exist)
        for (int i=0;i<static_cast<int>(s.auto_state.views.size());++i) add(s.auto_state.views[i].name, {WindowKind::diagram,i});
    if (s.model().loaded()) add(std::string(BROWSER_WINDOW), {WindowKind::browser,0});
    if (s.animation.vcr.iexist) add(std::string(ANIMATION_WINDOW), {WindowKind::animation,0});
    if (s.array_plot.plot.alive) add(std::string(ARRAY_PLOT_WINDOW), {WindowKind::array_plot,0});
    return windows;
}

std::string next_window_name(const Session &s, WindowKind kind)
{
    for (std::size_t i=1;;++i) {
        std::string name=xpp::format("{}_{}",kind==WindowKind::graph?"graph":"diagram",i);
        if (!find_window(s,name) && (kind!=WindowKind::diagram || !std::ranges::any_of(s.auto_state.views,[&](const AutoDiagramView &view){return view.name==name;}))) return name;
    }
}

std::string window_name(const Session &s, XppWinId handle)
{
    if (handle==WIN_AUTO) return s.auto_state.views[s.auto_state.active_view].name;
    if (handle==WIN_ANI || handle==WIN_APLOT) return std::string(fixed_window_name(handle));
    return s.plot_windows.graph[graph_of(s,handle)].name;
}

std::string_view fixed_window_name(XppWinId handle)
{
    return handle==WIN_ANI ? ANIMATION_WINDOW : ARRAY_PLOT_WINDOW;
}

std::string window_name_problem(std::string_view name)
{
    if (name.empty() || name.size()>WINDOW_NAME_BYTES) return xpp::format("A window name has 1 to {} UTF-8 bytes",WINDOW_NAME_BYTES);
    if (name==BROWSER_WINDOW || name==ANIMATION_WINDOW || name==ARRAY_PLOT_WINDOW) return xpp::format("\"{}\" is the name of a window of its own",name);
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
    for (i = 1; i < static_cast<int>(s.plot_windows.graph.size()); i++)
        if (s.plot_windows.graph[i].Use == 0) break;
    if (i == static_cast<int>(s.plot_windows.graph.size())) {
        s.plot_windows.graph.emplace_back();
        s.plot_display.emplace_back();
        s.marks_shown.windows.emplace_back();
        s.phase_shown.windows.emplace_back();
        s.plot_windows.current = &s.plot_windows.graph[s.plot_windows.active];
    }
    const std::string name=requested.empty()?next_window_name(s,WindowKind::graph):std::string(requested);
    copy_graph(s, i, s.plot_windows.active);
    s.plot_windows.graph[i].name=name;
    s.plot_windows.graph[i].hidden=false;
    s.plot_display[i]={};
    s.marks_shown.windows[i]={};
    s.phase_shown.windows[i]={};
    s.plot_windows.graph[i].x11Wid=extra_plot_width;
    s.plot_windows.graph[i].x11Hgt=extra_plot_height;
    s.plot_windows.graph[i].w = i + 1;
    // Protocol auxiliary windows occupy 101 through 105; plots skip this range.
    if(s.plot_windows.graph[i].w>=WIN_AUTO)s.plot_windows.graph[i].w+=WIN_APLOT-WIN_AUTO+1;
    set_active_windows(s);
    make_active(s, i, 1);
    ui.create_plot_window(s);
    get_draw_area(s);
}

void destroy_a_pop(Session &s)
{
    const int index=s.plot_windows.active;
    if(index==0){
        command_error("windows","Cannot destroy the main plot window");
        return;
    }
    kill_frz(s);
    destroy_labels_and_grobs(s,s.plot_windows.graph[index].w);
    ui.destroy_plot_window(s);
    s.plot_windows.graph[index].Use=0;
    s.plot_windows.graph[index].hidden=false;
    make_active(s,0,1);
    set_active_windows(s);
}

void kill_all_pops(Session &s)
{
    for(int i=1;i<static_cast<int>(s.plot_windows.graph.size());i++){
        if(!s.plot_windows.graph[i].Use)continue;
        make_active(s,i,1);
        destroy_a_pop(s);
    }
    make_active(s,0,1);
}

void make_active(xpp::Session &s, int i, int flag)
{
    s.plot_windows.active = i;
    s.plot_windows.current = &s.plot_windows.graph[s.plot_windows.active];
    s.plot_windows.draw_win = s.plot_windows.current->w;
    s.selected_window=s.plot_windows.current->name;
    get_draw_area_flag(s, flag);
}

int graph_of(const xpp::Session &s, XppWinId w)
{
    for (int i = 0; i < static_cast<int>(s.plot_windows.graph.size()); i++)
        if (s.plot_windows.graph[i].Use && s.plot_windows.graph[i].w == w) return i;
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
    s.nullclines.df_flag = 0;
}

void set_active_windows(xpp::Session &s)
{
  int i,np=0;
  s.plot_windows.open.resize(s.plot_windows.graph.size());
   for(i=0;i<static_cast<int>(s.plot_windows.graph.size());i++){
   if(s.plot_windows.graph[i].Use==1){
     s.plot_windows.open[np]=i;
     np++;
   }
 }
 s.plot_windows.count=np;
}  

void check_windows(xpp::Session &s)
{
 double zip,zap;
 check_val(&s.plot_windows.current->xmin,&s.plot_windows.current->xmax,&s.plot_windows.current->xbar,&s.plot_windows.current->dx);
 check_val(&s.plot_windows.current->ymin,&s.plot_windows.current->ymax,&s.plot_windows.current->ybar,&s.plot_windows.current->dy);
 check_val(&s.plot_windows.current->zmin,&s.plot_windows.current->zmax,&s.plot_windows.current->zbar,&s.plot_windows.current->dz);
 check_val(&s.plot_windows.current->xlo,&s.plot_windows.current->xhi,&zip,&zap);
 check_val(&s.plot_windows.current->ylo,&s.plot_windows.current->yhi,&zip,&zap);
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

std::string batch_plot_name(const xpp::Session &s, int i)
{
  const std::string &file=s.model().this_file,&set=s.this_internset;
  const std::string &format=s.plot_export.format;
  return i<0?xpp::format("{:.100}{:.100}.{:.10}",file,set,format)
            :xpp::format("{:.100}{:.100}_{:04d}.{:.10}",file,set,i,format);
}

void dump_ps(xpp::Session &s, int i)
{
  const std::string filename=batch_plot_name(s,i);
   const xpp::ImageFormat *fmt=xpp::find_image_format_by_extension(s.plot_export.format);
   if (!fmt || !fmt->write) {
     xpp::command_error("mkplot",xpp::format("Unsupported batch picture format {}",s.plot_export.format));
     return;
   }
   if(xpp::ok_or_show(xpp::begin_picture(s,*fmt,filename,s.plot_export.color)) && s.plot_file.writer)
     vector_restore(s);
}

void   redo_stuff(xpp::Session &s)
    {
      evaluate_derived(s);
   xpp::re_evaluate_kernels(s);
	  xpp::ok_or_show(redo_all_fun_tables(s));
        evaluate_derived(s);
}
} // namespace xpp
