/* Importing a foreign XPPAUT diagram (.auto): its parameters, marks, axes, points and restart solutions read from the
   text, checked whole, then applied to the Session in one step through the session members' own restore
   (xpp_session_autox.h). It is a file reader of ours, so it lives with the session files; autoX does not read files. */
#include "xpp_session_autox.h"
#include "session.h"
#include "autox_state.h"
#include "autox_c.h"
#include "autox_nox.h"
#include "autox_data.h"
#include "autox_options.h"
#include "autox_axis.h"
#include "model.h"
#include "xpp_fs.h"
#include "xpp_zip.h"
#include "xpp_lines.h"
#include "xpp_error.h"
#include "xpp_text.h"
#include "diagram.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <expected>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xpp {

// Foreign PAR numbering is translated here; unselected prefix entries are not model values.
static int imported_solver_index(const xpp::Session &s, std::span<const int> imported, int slot)
{
  return slot>=0 && static_cast<std::size_t>(slot)<imported.size()
    ? s.autox_state->parameter_layout.solver_index(imported[slot]) : slot;
}

static xpp::Result<std::string> convert_autox_solutions(const xpp::Session &s, std::span<const int> imported,
    std::string_view text, const std::string &file)
{
  /* Validate the foreign restart payload before translating its selected slots. */
  const auto checked=xpp::check_autox_solutions(text,file,AutoxState::state_copies*s.model().node,s.autox_state->parameter_layout.block,true);
  if(!checked)return std::unexpected(checked.error());
  const auto converted=xpp::read_lines("import autoX solutions",file,text,[&s,imported](xpp::Lines &lines){
    std::string out;

    while(!lines.at_end()){
      xpp::AutoxSolutionHeader h;
      const auto header=lines.next();
      if(xpp::trim_blanks(header).empty())continue;
      auto reader=xpp::TokenReader::of_text(header);
      if(!xpp::read_autox_solution_header(reader,h,s.autox_state->parameter_layout.block))lines.fail("invalid foreign solution header");
      if(h.npar<static_cast<long>(imported.size()))lines.fail("foreign solutions omit selected parameters");
      const std::size_t orbit=static_cast<std::size_t>(h.ntpl)*h.nar;
      const std::size_t prefix=orbit+(h.ntst?2*h.nfpr+static_cast<std::size_t>(h.ntpl)*(h.nar-1):0);
      // A finite double's shortest round-trip spelling plus separator fits in 32 bytes.
      constexpr std::size_t imported_number_bytes = 32;
      // Header integers also fit in that allowance; reserve twelve header fields.
      const std::size_t expanded=(prefix+s.autox_state->parameter_layout.size()+autox_solution_header_fields)*imported_number_bytes;
      if(expanded>xpp::zip::archive_bytes_limit-out.size())lines.fail("translated autoX solutions exceed the session memory limit");
      std::vector<double> parameters(s.autox_state->parameter_layout.size());
      std::string body;
      std::size_t count=0;
      int body_rows=0;
      for(int row=0;row<h.nskip;row++){
        const auto text_row=lines.next();
        xpp::Tokens tokens(text_row);
        std::string converted_row;
        while(const auto word=tokens.next(" \t\r\n")){
          if(count>=prefix){
            const int slot=static_cast<int>(count-prefix);
            double value;
            if(!xpp::parse_number(*word,value))lines.fail("invalid foreign parameter");
            if(slot>=xppaut_parameter_prefix || static_cast<std::size_t>(slot)<imported.size())parameters[imported_solver_index(s,imported,slot)]=value;
          } else {
            std::string value(*word);
            if(h.ntst && count>=orbit && count<orbit+static_cast<std::size_t>(h.nfpr)){
              int slot;
              if(!xpp::parse_int(*word,slot))lines.fail("invalid foreign restart index");
              if(slot<xppaut_parameter_prefix && static_cast<std::size_t>(slot)>=imported.size())lines.fail("restart names an unselected foreign parameter");
              value=xpp::number(imported_solver_index(s,imported,slot));
            }
            if(!converted_row.empty())converted_row+=' ';
            converted_row+=value;
          }
          count++;
        }
        if(!converted_row.empty()){
          body+=converted_row+'\n';
          body_rows++;
        }
      }
      h.npar=static_cast<long>(parameters.size());
      h.nskip=body_rows+(h.npar+autox_solution_columns-1)/autox_solution_columns;
      const std::string header_text=xpp::format("{} {} {} {} {} {} {} {} {} {} {} {}\n",h.ibr,h.ntot,xpp::autox_point_encode(h.itp),h.lab,h.nfpr,h.isw,h.ntpl,h.nar,h.nskip,h.ntst,h.ncol,h.npar);
      if(header_text.size()+body.size()+parameters.size()*imported_number_bytes>xpp::zip::archive_bytes_limit-out.size())
        lines.fail("translated autoX solutions exceed the session memory limit");
      out+=header_text;
      out+=body;
      for(std::size_t i=0;i<parameters.size();i++)out+=xpp::number(parameters[i])+((i+1)%autox_solution_columns==0 || i+1==parameters.size()?"\n":" ");
    }
    return out;
  });
  return converted;
}


namespace {
struct ImportedAutox {
  xpp::AutoxSettingsSet options;
  int mark_count=0;
  std::vector<int> parameters;
  std::deque<DIAGRAM> points;
  std::string solutions;
};
}

xpp::Result<> import_autox_file(xpp::Session &s, std::string_view text, const std::string &file)
{
  auto read=xpp::read_lines("import autoX",file,text,[&](xpp::Lines &lines){
    ImportedAutox held;
    auto &options=held.options;
    std::map<std::string,int> locations;
    auto first=xpp::TokenReader::of_text(lines.next("selected parameters and marks"));
    int count;
    if(!first.read(count) || count<1 || count>std::min(s.model().nupar,xppaut_parameter_prefix))
      lines.fail("invalid foreign selected parameter count");
    for(int k=0;k<count;k++){
      int parameter;
      if(!first.read(parameter) || parameter<0 || parameter>=s.model().nupar
          || std::find(held.parameters.begin(),held.parameters.end(),parameter)!=held.parameters.end())
        lines.fail("invalid or repeated foreign model parameter index");
      held.parameters.push_back(parameter);
    }
    if(!first.read(held.mark_count) || held.mark_count<0 || held.mark_count>xpp::autox_foreign_mark_rows || !first.at_end())
      lines.fail("foreign mark count exceeds the stored mark slots");
    // Validate every encoded row, including inactive foreign marks.
    options.nmarks=xpp::autox_foreign_mark_rows;
    options.mark_name.resize(*options.nmarks);
    options.mark_value.resize(*options.nmarks);
    options.par1=s.model().upar_names[held.parameters.front()];
    if(count>1)options.par2=s.model().upar_names[held.parameters[1]];
    for(int k=0;k<xpp::autox_foreign_mark_rows;k++){
      int slot;
      double value;
      xpp::numbers(lines, value,slot);
      const int solver=imported_solver_index(s,held.parameters,slot);
      const int parameter=s.autox_state->parameter_layout.model_index(solver);
      if(slot<0 || (solver!=xpp::autox_unknown::period && parameter<0))
        lines.fail("invalid foreign mark parameter index");
      options.mark_value[k]=value;
      options.mark_name[k]=solver==xpp::autox_unknown::period ? "T" : s.model().upar_names[parameter];
      locations[xpp::format("mark{}",k)]=lines.line();
    }
    const auto numeric=[&](std::initializer_list<int> fields){
      auto row=xpp::TokenReader::of_text(lines.next("autoX numerics"));
      for(int field:fields){
        auto &value=options.num[field];
        if(!row.read(value))lines.fail("invalid or incomplete autoX numerics");
        if(field==xpp::AUTOX_NUM_MAX_BRANCH_SWITCHES){
          /* XPPAUT's MXBF is one signed number: its sign is the direction (W531 keeps them apart) */
          options.num[xpp::AUTOX_NUM_SWITCH_ONE_DIRECTION]=value<0;
          options.has_num[xpp::AUTOX_NUM_SWITCH_ONE_DIRECTION]=1;
          value=std::fabs(value);
        }
        std::string why;
        if(!xpp::autox_num_ok(field,value,why))lines.fail(why);
        options.has_num[field]=1;
        locations[xpp::autox_settings_num_key(field)]=lines.line();
      }
      if(!row.at_end())lines.fail("extra autoX numeric fields");
    };
    numeric({xpp::AUTOX_NUM_MESH_INTERVALS,xpp::AUTOX_NUM_MAX_STEPS,xpp::AUTOX_NUM_OUTPUT_EVERY});
    numeric({xpp::AUTOX_NUM_STEP_SIZE,xpp::AUTOX_NUM_MIN_STEP_SIZE,xpp::AUTOX_NUM_MAX_STEP_SIZE});
    numeric({xpp::AUTOX_NUM_PARAMETER_MIN,xpp::AUTOX_NUM_PARAMETER_MAX,xpp::AUTOX_NUM_NORM_MIN,xpp::AUTOX_NUM_NORM_MAX});
    numeric({xpp::AUTOX_NUM_MESH_ADAPT_EVERY,xpp::AUTOX_NUM_MAX_BRANCH_SWITCHES,xpp::AUTOX_NUM_OUTPUT_DETAIL,xpp::AUTOX_NUM_MAX_LOCATION_ITERATIONS,xpp::AUTOX_NUM_MAX_NEWTON_ITERATIONS,xpp::AUTOX_NUM_FULL_NEWTON_AFTER,xpp::AUTOX_NUM_STEP_ADAPT_EVERY});
    int var,plot;
    xpp::numbers(lines, options.range[0],options.range[2],options.range[1],options.range[3],var,plot);
    if(var<0 || var>=s.model().node)lines.fail("foreign axis variable is outside the model");
    options.var=s.model().uvar_names[var];
    options.plot=xpp::autox_projection_from_xppaut(plot);
    if(!options.plot)lines.fail(xpp::format("{} is not one of XPPAUT's plot types (0-4, 10, 11)",plot));
    options.has_range.fill(1);
    // XPPAUT omits Ncol; the retained collocation order must still be supported.
    options.has_num[xpp::AUTOX_NUM_COLLOCATION_POINTS]=1;
    options.num[xpp::AUTOX_NUM_COLLOCATION_POINTS]=xpp::autox_settings_now(xpp::autox_parts(s)).num[xpp::AUTOX_NUM_COLLOCATION_POINTS];
    if(const xpp::Result<> checked=xpp::autox_settings_check(xpp::autox_parts(s),options,-1);!checked){
      const std::string &key=checked.error().field;
      lines.fail(locations.contains(key)?locations.at(key):lines.line(),checked.error().what);
    }
    std::vector<double> parameters;
    for(int k=0;k<s.model().nupar;k++)parameters.push_back(s.parser.constants[s.model().upar_con[k]]);
    held.points=xpp::snapx::autox_members::read_foreign_diagram(lines,s.model().node,held.parameters,parameters,
        s.autox_state->type_of_calc,s.autox_state->two_param,s.autox_state->blrtn.torper);
    const int offset=lines.line();
    std::string tail;
    while(!lines.at_end())tail+=std::string(lines.next())+'\n';
    const auto solutions=convert_autox_solutions(s,held.parameters,tail,file);
    if(!solutions)lines.fail(offset+solutions.error().place.line,solutions.error().what);
    held.solutions=*solutions;
    return held;
  });
  if(!read)return std::unexpected(read.error());
  auto held=std::move(*read);
  held.options.nmarks=held.mark_count;
  held.options.mark_name.resize(held.mark_count);
  held.options.mark_value.resize(held.mark_count);
  // The session owner commits restart bytes before any held state, as for .snapx.
  auto restored=xpp::snapx::autox_members::restore_members(s,
      {held.options,std::move(held.points),{},held.solutions});
  if(!restored)return restored;
  s.autox_state->imported_parameters=std::move(held.parameters);
  forget_autox_diagram(xpp::autox_parts(s),false);
  return {};
}

/* Foreign XPPAUT diagrams remain an explicit import, separate from Load. */
xpp::Result<> import_autox(xpp::Session &s, const std::string &file)
{
  xpp::Result<std::string> text=s.model().fs->read("import autoX",file,xpp::zip::archive_bytes_limit);
  if(!text)return std::unexpected(text.error());
  return import_autox_file(s,*text,file);
}

} // namespace xpp
