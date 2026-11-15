#include "tabular.h"
#include "expr_tables.h"
#include "getvar.h"
#include "storage.h"
#include "xpp_zip.h"
#include "xpp_lines.h"
#include "xpp_text.h"
#include "xpp_ui.h"


#include "expr.h"

#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

/*********************************************************
     This is code for read-in tables in XPP  
     This should probably be accessible from within the program
     as well.  It will probably be added to the Numerics Menu

     The files consist of y-values of a function evaluated at
     equally spaced points as well as some header information.  
     They are ascii files of the form:

     npts <-- Integer
     xlo <--- fp
     xhi <--  fp
     y1  <-- fp
     y2
     ...
     yn

     Thus   dx = (xhi-xlo)/(npts-1)

  In the creation of the file, one can instead use the following:
  
 table <name> % numpts xlo xhi formula

 to create a "formula" table which is linearly interpolated

 table <name> @ filename creates an array for two-valued 
                functions

 filename has the following info:
 nxpts
 nypts
 xlo
 xhi
 ylo
 yhi
 
 nx*ny points as follows

 f(x1,y1), f(x2,y1),....,f(xn,y1),
 ...
 f(x1,ym), ..., f(xn,ym)

to be added later
**************************************************************/

#include <math.h>
#include <stdio.h>
#include <cstddef>
#include <expected>
#include <memory>
#include <utility>
#include "model.h"
#include "model_files.h"
#include "model_place.h"
#include "expr_program.h"
#include "model_options.h"

namespace xpp {


namespace {
/* Every caller fills all values. Release old capacity so replacing and
   shrinking tables cannot accumulate storage beyond the session budget.
   y stays a raw double*: see TABULAR (tabular.h). */
void resize_values(TABULAR &t, int length)
{
  std::vector<double>(static_cast<size_t>(length)).swap(t.y_storage);
  t.y=t.y_storage.data();
}

/* where the model defines table index (its table line), for its errors */
xpp::Place table_place(const SessionParts &s, int index)
{
  return xpp::model_place(s.model(),s.tables[index].name);
}

/* Replacements may reuse their table's budget; all other tables share it. */
bool table_count_ok(const SessionParts &s, int index, int count)
{
  if (count < 2 || count > table_points_limit) return false;
  size_t total = static_cast<size_t>(count);
  for (int i = 0; i < static_cast<int>(s.tables.size()); ++i)
    if (i != index) total += s.tables[i].y_storage.size();
  return total <= static_cast<size_t>(table_points_limit);
}
}

void set_autox_eval_flags(std::vector<TABULAR> &tables, int f)
{
 for(TABULAR &t:tables)
    t.autoeval=f;
}
Result<int> find_table(const SessionParts &s, std::string_view name)
{
  for(int i=0;i<s.ntable;i++)
    if(equal_ignoring_case(s.tables[i].name,name))return i;
  return fail("table",xpp::format("The model has no table named \"{}\"",name),command_place());
}

Result<int> edit_table(const SessionParts &s, int index, const TableEdit &edit)
{
  TABULAR &table=s.tables[index];
  if(table.flag==1){
    if(edit.points||edit.x_min||edit.x_max||edit.formula||edit.auto_evaluate)
      return fail("table",xpp::format("{} is a table read from a file: give its file, not a formula, points, range or auto-evaluate",table.name),table_place(s,index));
    const std::string file=edit.file.value_or(table.filename);
    if(auto loaded=load_table(s,file,index,0);!loaded)return std::unexpected(loaded.error());
    table.filename=file;
    return table.n;
  }
  if(table.flag!=2)return fail("table",xpp::format("{} is not defined yet: it has no formula or file to edit",table.name),table_place(s,index));
  if(edit.file)
    return fail("table",xpp::format("{} is a table of a formula: give its formula, points and range, not a file",table.name),table_place(s,index));
  if(auto made=create_fun_table(s,edit.points.value_or(table.n),edit.x_min.value_or(table.xlo),edit.x_max.value_or(table.xhi),edit.formula.value_or(table.filename),index);!made)
    return std::unexpected(made.error());
  if(edit.auto_evaluate)table.autoeval=*edit.auto_evaluate?1:0;
  return table.n;
}

xpp::Result<> redo_all_fun_tables(const SessionParts &s)
{
  int i;
  xpp::FirstError first;
  for(i=0;i<s.ntable;i++){
    if(s.tables[i].flag==2&&s.tables[i].autoeval==1)
      first.keep(eval_fun_table(s,i,s.tables[i].n,s.tables[i].xlo,
		     s.tables[i].xhi,s.tables[i].filename,s.tables[i].y));
  }
  update_all_ffts(s);
  return first.result();
}

xpp::Result<> eval_fun_table(const SessionParts &s, int index, int n, double xlo, double xhi, const std::string &formula, double *y)
{
  int i;
  
  double dx;
  double oldt;
  Program command;
  int ncold=s.parser.ncon,nsym=s.parser.nsym;
  if(add_expr(s.parser,formula,command.rpn,&i)){
    s.parser.ncon=ncold;
    s.parser.nsym=nsym;
    return xpp::fail("table",xpp::format("Illegal formula {}",formula),table_place(s,index));
  }
  oldt=getvar(s.parser,0);
  dx=(xhi-xlo)/(static_cast<double>(n-1));
  for(i=0;i<n;i++){
    setvar(s.parser,0,dx*i+xlo);
    y[i]=evaluate(s.parser,command);
    if (s.parser.step_error) {
      setvar(s.parser,0,oldt);
      s.parser.ncon=ncold;
      s.parser.nsym=nsym;
      return std::unexpected(xpp::take_stop(s.parser));
    }
  }
  setvar(s.parser,0,oldt);
  s.parser.ncon=ncold;
  s.parser.nsym=nsym;
  return {};
}

xpp::Result<> create_fun_table(const SessionParts &s, int npts, double xlo, double xhi, std::string_view formula, int index)
{
  int length=npts;

   if(s.tables[index].flag==1){
    return xpp::fail("table",xpp::format("{} is not a function table",s.tables[index].name),table_place(s,index));
  }
  if(xlo>xhi){
    return xpp::fail("table",xpp::format("{}: its low end {:g} is above its high end {:g}",s.tables[index].name,xlo,xhi),table_place(s,index));
  }
  if(npts<2){
    return xpp::fail("table",xpp::format("{}: {} points is too few (at least 2)",s.tables[index].name,npts),table_place(s,index));
  }
  if (!table_count_ok(s,index,npts))
    return xpp::fail("table",xpp::format("{} points exceeds the session table budget of {} points",npts,table_points_limit),table_place(s,index));
  if (auto fits=check_network_table_length(s,index,npts); !fits) return fits;
  resize_values(s.tables[index],length);
  s.tables[index].flag=2;
  auto ev=eval_fun_table(s,index,npts,xlo,xhi,std::string(formula),s.tables[index].y);
  if(!ev)return ev;
  s.tables[index].xlo=xlo;
  s.tables[index].xhi=xhi;
  s.tables[index].n=npts;
  s.tables[index].dx=(xhi-xlo)/(static_cast<double>(npts-1));
  s.tables[index].filename=formula;
  return {};
}

xpp::Result<> load_table(const SessionParts &s, std::string_view filename, int index, int model_file)
{
  int i;
  int length;
  double xlo,xhi;
  /* the name without its quotes, up to a closing one */
  std::string filename2;
  bool quoted=false;
  for(char c:filename){
    if(c=='"'){
      if(quoted)break;
      quoted=true;
    }
    else
      filename2+=c;
  }

  if(s.tables[index].flag==2){
    return xpp::fail("table",xpp::format("{} is not a file table",s.tables[index].name),table_place(s,index));
  }

  if(model_file){
    const xpp::Result<> allowed=xpp::approve_model_path(s.model(),filename2,table_place(s,index));
    if(!allowed)return std::unexpected(allowed.error());
  }
  std::string bytes;
  if(model_file){
    if(!xpp::read_model_file(s.model(),filename2,bytes))
      return xpp::fail_reading("table",xpp::format("cannot be read (not found in {})",s.model().fs->working_dir()),filename2);
  }
  else{
    xpp::Result<std::string> read=s.model().fs->read("table",filename2,xpp::zip::archive_bytes_limit);
    if(!read)return xpp::fail_reading("table",xpp::format("cannot be read (not found in {})",s.model().fs->working_dir()),filename2);
    bytes=std::move(*read);
  }
  const std::vector<std::string_view> table_lines=xpp::split_lines(bytes);
  int at=0; /* the file's lines read */
  std::string source;
  auto next_line=[&table_lines,&at,&source]() -> std::optional<std::string> {
    if(static_cast<std::size_t>(at)>=table_lines.size()) return std::nullopt;
    source = table_lines[static_cast<std::size_t>(at++)];
    return source;
  };
  /* a problem at the line read last; a file that ends too soon at its
     last line */
  auto problem=[&](std::string what){
    return xpp::fail("table",std::move(what),xpp::Place{filename2,at>0?at:1,0,source});
  };
  auto too_short=[&](){
    return problem(xpp::format("The table file ends after {} lines: it is too short",at));
  };

  /* a table number: the whole line, blanks around it allowed, one finite
     number (parse_number takes nan and inf too) */
  auto number=[](const std::string &line,double &out){
    return xpp::parse_number(xpp::trim_blanks(line),out)&&std::isfinite(out);
  };
  /* the file's values and kind are applied only once the whole file is read */
  int interp=0; /* 0 linear, 1 closest step, 2 cubic spline */
  auto line0=next_line();
  if(!line0){
    return too_short();
  }
  {
    const char *bob=line0->c_str();
    if (bob[0]=='i') /* closest step value */
      {
        interp=1;
        bob++;  /* skip past initial "i" to length */
      };
    if (bob[0]=='s') /* cubic spline  */
      {
        interp=2;
        bob++;  /* skip past initial "i" to length */
      };
    if (!xpp::parse_int(bob,length))
      return problem(xpp::format("\"{}\" is not a point count",bob));
  }
  if(length<2){
    return problem(xpp::format("A table has at least 2 values, not {}",length));
  }
  if (!table_count_ok(s,index,length))
    return problem(xpp::format("{} points exceeds the session table budget of {} points",length,table_points_limit));
  auto line1=next_line();
  if(!line1){
    return too_short();
  }
  if(!number(*line1,xlo)) return problem(xpp::format("\"{}\" is not a finite number: the table's low end",*line1));
  auto line2=next_line();
  if(!line2){
    return too_short();
  }
  if(!number(*line2,xhi)) return problem(xpp::format("\"{}\" is not a finite number: the table's high end",*line2));
  if(xlo>=xhi){
    return problem(xpp::format("Its high end {:g} is not above its low end {:g}",xhi,xlo));
  }
  if(!std::isfinite(xhi-xlo)) return problem(xpp::format("The table's range from {:g} to {:g} is too wide",xlo,xhi));
  bool fresh=(s.tables[index].flag==0);
  std::vector<double> values(static_cast<std::size_t>(length));
  for(i=0;i<length;i++){
    auto line=next_line();
    if(!line) return too_short();
    if(!number(*line,values[i])) return problem(xpp::format("\"{}\" is not a finite number: table value {} of {}",*line,i+1,length));
  }
  /* a re-read must keep every network's values in range (nothing is applied
     when it does not) */
  const xpp::Result<> fits=check_network_table_length(s,index,length);
  if(!fits) return fits;
  s.tables[index].interp=interp;
  resize_values(s.tables[index],length);
  std::copy(values.begin(),values.end(),s.tables[index].y);
  s.tables[index].xlo=xlo;
  s.tables[index].xhi=xhi;
  s.tables[index].n=length;
  s.tables[index].dx=(xhi-xlo)/(length-1);
  s.tables[index].flag=1;
  if(fresh) s.tables[index].filename=filename2;
  return {};
}
   
xpp::Result<> add_file_table(const SessionParts &s, int index, std::string_view file)
{
  /* the name's printable characters */
  std::string file2;
  for(char c:file)
    if(c>31&&c<127)
      file2+=c;
  return load_table(s,file2,index,1);
}

} // namespace xpp
