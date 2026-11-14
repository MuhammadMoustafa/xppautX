/* The data side of the data table: the one DATA_VIEW instance, its storage
   pointer, row/column bookkeeping and the file writer. The page
   displays it. */
#include <limits>
#include "xpp_files.h"
#include "many_pops.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "model.h"
#include "getvar.h"
#include "typed_values.h"
#include "storage.h"
#include "json_number.h"
#include "expr.h"
#include "xpp_io.h"
#include "browse.h"
#include "xpp_ui.h"
#include "integrate.h"
#include <cctype>
#include <strings.h>
#include "load_eqn.h"
#include "data_formats.h"
#include "graf_par.h"
#include "menus.h"
#include "expr_program.h"
#include "numerics_settings.h"
#include "session.h"
#include "struct.h"
#include "xpp_error.h"
#include <algorithm>
#include <span>
#include <expected>
#include <memory>
#include <optional>
#include <utility>

namespace xpp {

ColumnTable get_table_data(xpp::Session &s)
{
  return s.data->view.data;
}

/* show another data set in the data table: its columns from new_dat[1] on,
   dat_len rows (the adjoint, the Fourier modes, a histogram, ...) */
void new_browse_dat(xpp::Session &s, ColumnTable new_dat, int dat_len)
{
  s.data->view.data=new_dat;
  refresh_data(s,dat_len);
}

Result<> show_derived(xpp::Session &s, bool exists, const std::string &command, const std::string &missing, void (*show)(xpp::Session &))
{
  if(!exists)return fail(command,missing,command_place());
  show(s);
  return {};
}

Column get_data_col(const xpp::Session &s, int c)
{
  return s.data->view.data[c];
}

void waitasec(int msec)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}

std::string browse_column_name(const xpp::Session &s, int j)
{
  if(j==0)return "T";
  const xpp::Model &m=s.model();
  if(j>0&&j<=m.neq)return m.uvar_names[j-1];
  const std::vector<AddedColumn> &added=s.data->added_columns;
  if(j>m.neq){
    const std::size_t k=static_cast<std::size_t>(j-m.neq-1);
    if(k<added.size())return added[k].name;
  }
  return "";
}

namespace {

/* the rows First..Last of b's columns cols */
xpp::DataTable data_table(const xpp::Session &s, const DATA_VIEW &b, std::span<const int> cols)
{
  xpp::DataTable t;
  t.stored_floats=s.data_store->precision()==StoragePrecision::single;
  for(int j : cols){
    t.names.push_back(browse_column_name(s,j));
    if(b.iend>b.istart)t.columns.push_back(b.data[j].values(b.istart,b.iend-b.istart));
    else t.columns.emplace_back();
  }
  return t;
}

/* every column b has */
std::vector<int> all_columns(const DATA_VIEW &b)
{
  std::vector<int> cols(b.maxcol>0?b.maxcol:0);
  for(std::size_t j=0;j<cols.size();j++)cols[j]=static_cast<int>(j);
  return cols;
}

} // namespace

namespace {

/* the columns a batch run writes: the "only" list, else every column */
std::vector<int> output_columns(const xpp::Session &s, const DATA_VIEW &b)
{
  return !s.plot_list.empty()?s.plot_list:all_columns(b);
}

} // namespace

void write_mytable_data(xpp::Session &s, xpp::Writer &w)
{
  const DATA_VIEW &b=s.data->view;
  xpp::data_format_named("dat")->write(data_table(s,b,output_columns(s,b)),w);
}

xpp::DataTable stored_data_table(const xpp::Session &s)
{
  const int ncol=s.model().neq+1;
  xpp::DataTable t;
  for(int j=0;j<ncol;j++){
    t.names.push_back(browse_column_name(s,j));
    t.columns.push_back(s.data_store->col[j].values(0,s.data_store->rows));
  }
  t.stored_floats=s.data_store->precision()==StoragePrecision::single;
  t.seed=s.numerics->last_seed;
  return t;
}

int put_stored_data(xpp::Session &s, const xpp::DataTable &t)
{
  const int neq=s.model().neq;
  const int rows=static_cast<int>(t.rows());
  if(const xpp::Result<> r=s.data_store->fit(neq+1,rows);!r){
    xpp::show_error(r.error());
    return 0;
  }
  for(std::size_t k=0;k<t.columns.size();k++){
    int col;
    find_variable(s,xpp::data_column_name(t,k),&col);
    if(col<0||col>neq||t.columns[k].size()<static_cast<std::size_t>(rows))continue;
    s.data_store->col[col].store(0,std::span(t.columns[k]).first(static_cast<std::size_t>(rows)));
  }
  s.data_store->rows=rows;
  /* the store's own columns, which may just have grown (moved): not a
     derived data set shown before, whose lent columns would be stale */
  s.data->view.data=s.data_store->col;
  refresh_data(s,rows);
  return rows;
}

void find_variable(const xpp::Session &s, std::string_view name, int *col)
{
 *col=-1;
  if(xpp::equal_ignoring_case("T",name)){
   *col=0;
    return;
   }
  *col=xpp::find_user_name(s.model(),2,name);
  if(*col>-1){
    *col=*col+1;
    return;
  }
  const std::vector<AddedColumn> &added=s.data->added_columns;
  for(std::size_t k=0;k<added.size();k++){
    if(xpp::equal_ignoring_case(added[k].name,name)){
      *col=s.model().neq+1+static_cast<int>(k);
      return;
    }
  }
 }

void  refresh_data(xpp::Session &s, int length)
{
 s.data->view.dataflag=1;
 s.data->view.maxrow=length;
 s.data->view.iend=length;
 if(s.data->view.data==s.data_store->col){
   /* a fresh run's own data, the Model's own columns unchanged: recompute
      every added column over it (docs/manual/07-data-table.md: it
      stays computed "as though ... another auxiliary variable") rather
      than dropping it (docs/roadmap.md W77) */
   const xpp::Model &m=s.model();
   for(std::size_t k=0;k<s.data->added_columns.size();k++){
     const int col_index=m.neq+1+static_cast<int>(k);
     s.data_store->add_column(col_index); /* fresh max_rows zeros */
     ok_or_show(compute_added_column(s,s.data->added_columns[k].formula,col_index,length));
   }
   s.data->view.maxcol=m.neq+1+static_cast<int>(s.data->added_columns.size());
 }
 ui.data_changed(length);
}

void reset_data(xpp::Session &s)
{
  s.data->view.maxrow=0;
  s.data->view.dataflag=0;
}

void init_data(xpp::Session &s)
{
 
 s.data->view.dataflag=0;
 s.data->view.data=s.data_store->col;
 s.data->view.maxcol=s.model().neq+1;
 s.data->view.maxrow=0;
 s.data->view.row0=0;
 s.data->view.istart=0;
 s.data->view.iend=0;
 s.data->added_columns.clear();

}

Writer ask_output_writer(const Session &s, std::string_view title, std::string_view ext,
                         std::string_view what, bool binary)
{
 std::string file=xpp::files::output_name(s.model().this_file,ext,what);
 if(!file_selector(title,file,xpp::format("*{}",ext)))return {};
 return open_writer_asking(file,binary);
}

bool save_ready(bool available)
{
 if(available)return true;
 ui.save_result("",false);
 command_error("save","Nothing to save");
 return false;
}

namespace {

/* the writer of open_writer_asking and open_writer_replacing: ask is
   whether the replace decision is still to be had (a command's, a
   dialog's, or the user's answer) */
xpp::Writer open_writer(std::string_view fil, bool binary, Result<> *opened, bool ask, bool independent, const Place &at)
{
 if(opened)*opened={};
 const int status=xpp::files::output_status(fil);
 if(status!=XPP_FILES_OK){
   Error failure=status==XPP_FILES_IO ? xpp::files::write_error("save",fil)
       : Error{"save",xpp::format("Cannot save {}: {}",fil,xpp::files::status_text(status)),command_place()};
   if((!at.file.empty() || !at.source.empty()) && !xpp::files::serving_reads())failure.place=at;
   const Result<> error=std::unexpected(std::move(failure));
   /* A replay's path guard must stop the step before later commands run,
      even when the basename was rejected before Writer could open it. */
   xpp::files::write_path_ok(fil,true);
   ui.save_result(fil,false);
   if(opened)*opened=error; else show_error(error.error());
   return {};
 }
 int answer=ask?ui.save_replace(independent):SAVE_REPLACE;
 /* A native dialog or an explicit command already made the decision.
    Otherwise only an existing destination needs confirmation. */
 if(answer==SAVE_ASK){
   const bool asked=xpp::files::exists(fil);
   answer=asked&&TwoChoice("Yes","No",xpp::format("{} exists. Replace it?",fil),"yn")!='y'
          ?SAVE_DECLINE:SAVE_REPLACE;
   ui.save_decision(answer,asked);
 }
 if(answer==SAVE_DECLINE){
   ui.save_result(fil,false);
   return {};
 }
 xpp::Writer w=binary?xpp::Writer::binary(fil):xpp::Writer(fil);
 if(!w){
   Error failure=xpp::files::write_error("save",fil);
   if((!at.file.empty() || !at.source.empty()) && !xpp::files::serving_reads())failure.place=at;
   const Result<> error=std::unexpected(std::move(failure));
   ui.save_result(fil,false);
   if(opened)*opened=error;
   else show_error(error.error());
 }
 return w;
}

} // namespace

xpp::Writer open_writer_asking(std::string_view fil, bool binary, Result<> *opened, bool independent, const Place &at)
{
 return open_writer(fil,binary,opened,true,independent,at);
}

xpp::Writer open_writer_replacing(std::string_view fil, bool binary, Result<> *opened)
{
 return open_writer(fil,binary,opened,false,false,{});
}

Result<> commit_save(Writer &w)
{
 Result<> result=w.commit();
 ui.save_result(w.path(),result.has_value());
 if(!result){
   result.error().what=xpp::format("Cannot save {}: {}",w.path(),result.error().what);
   result.error().place=command_place();
 }
 return result;
}

void abort_save(Writer &w)
{
 w.abort();
 ui.save_result(w.path(),false);
}

int file_replace_choice(bool open_existing)
{
 return TwoChoice(open_existing?"Replace":"Yes",open_existing?"Open existing .odex":"No",
                  "File Exists! Overwrite?","yn");
}

void  wipe_rep(DataState &b)
 {
    if(!b.replaced)return;
    std::vector<double>().swap(b.old_column);
    b.replaced=0;
  }

void ic_of_row(xpp::Session &s, int in)
{
 int i;
 setvar(s,0,static_cast<double>(s.data_store->col[0][in]));
 for(i=0;i<s.model().node;i++)
 {
  s.last_ic[i]=static_cast<double>(s.data_store->col[i+1][in]);
  setvar(s,i+1,s.last_ic[i]);
 } 
 for(i=0;i<s.model().nmarkov;i++){
   s.last_ic[i+s.model().node]=static_cast<double>(s.data_store->col[i+s.model().node+1][in]);
   setvar(s,i+1+s.model().node+s.model().fix_var,s.last_ic[i+s.model().node]);
 }
 for(i=s.model().node+s.model().nmarkov;i<s.model().neq;i++)
   xpp::set_val(s,s.model().uvar_names[i],s.data_store->col[i+1][in]);

 redraw_ics();
}

void data_get_mytable(xpp::Session &s, int row)
{
  s.data->view.row0=row;
  ic_of_row(s,row);
}

void get_data_xyz(const xpp::Session &s, float *x, float *y, float *z, int i1, int i2, int i3, int off)
{
  int in=s.data->view.row0+off;
  *x=static_cast<float>(s.data->view.data[i1][in]);
  *y=static_cast<float>(s.data->view.data[i2][in]);
  *z=static_cast<float>(s.data->view.data[i3][in]);
}

/* ---- the data table's commands (were in browse.c); the widget calls them ---- */

Result<> require_stored_data(const xpp::Session &s)
{
 if(s.data->view.data!=s.data_store->col)return fail("data","Only data can be in the data table",command_place());
 return {};
}

Result<> delete_added_column(xpp::Session &s, std::string_view name)
{
  DATA_VIEW *b=&s.data->view;
  if(const Result<> stored=require_stored_data(s);!stored)return stored;
  if(s.data->added_columns.empty())
    return fail("data","There are no added columns to delete",command_place());
  int col=-1;
  find_variable(s,name,&col);
  const int first=s.model().neq+1;
  if(col<first)
    return fail("data","Choose an added column; time and model columns cannot be deleted",command_place());
  for(const auto &g:s.plot_windows->graph){
    if(!g.Use)continue;
    for(int i=0;i<g.nvars;i++){
      if(g.xv[i]==col||g.yv[i]==col||(g.ThreeDFlag&&g.zv[i]==col))
        return fail("data","This column is plotted; change its plot axes before deleting it",command_place());
    }
  }
  if(const auto removed=s.data_store->remove_column(col,b->maxcol);!removed)
    return std::unexpected<Error>(removed.error());
  s.data->added_columns.erase(s.data->added_columns.begin()+(col-first));
  for(auto &g:s.plot_windows->graph){
    if(!g.Use)continue;
    for(int i=0;i<g.nvars;i++){
      if(g.xv[i]>col)--g.xv[i];
      if(g.yv[i]>col)--g.yv[i];
      if(g.zv[i]>col)--g.zv[i];
    }
  }
  if(s.data->replaced_col==col)wipe_rep(*s.data);
  else if(s.data->replaced_col>col)--s.data->replaced_col;
  --b->maxcol;
  ui.data_redraw(1);
  return {};
}

namespace {

/* the parser's values set to one stored row, for a formula computed row by
   row: T and the ODEs by index, the Markov variables and the aux
   quantities through their names, looked up once for all the rows */
class RowValues {
public:
  explicit RowValues(xpp::Session &s) : s_(s)
  {
    const xpp::Model &m=s.model();
    for(int j=m.node;j<m.neq;j++)slots_.push_back(xpp::value_slot(s,m.uvar_names[j]));
  }
  void set(ColumnTable data, int row)
  {
    const int node=s_.model().node;
    for(int j=0;j<node+1;j++)setvar(s_,j,static_cast<double>(data[j][row]));
    for(std::size_t k=0;k<slots_.size();k++)
      if(slots_[k])*slots_[k]=static_cast<double>(data[node+1+static_cast<int>(k)][row]);
  }
private:
  xpp::Session &s_;
  std::vector<double *> slots_;
};

}

namespace {

/* add_expr may have added constants to the parser's working symbol
   table (ParserState::ncon/nsym, session.h): rolled back to the Model's
   own end, like a histogram condition (histogram.cpp) -- an added column
   is not a symbol a later formula can name */
void forget_formula_symbols(xpp::Session &s)
{
  s.parser.ncon=s.model().ncon_start;
  s.parser.nsym=s.model().nsym_start;
}

} // namespace

Result<> compute_added_column(xpp::Session &s, const std::string &formula, int col_index, int nrows)
{
  Program com;
  int length=0;
  if(xpp::add_expr(s,formula,com.rpn,&length)){
    forget_formula_symbols(s);
    return fail("data","Bad Formula .... ",command_place());
  }
  RowValues row(s);
  for(int i=0;i<nrows;i++){
    row.set(s.data_store->col,i);
    s.data_store->col[col_index][i]=xpp::evaluate(s,com);
  }
  forget_formula_symbols(s);
  return {};
}

bool added_column_compiles(xpp::Session &s, std::string_view formula)
{
  std::vector<int> com;
  int length=0;
  const bool compiles=xpp::add_expr(s,formula,com,&length)==0;
  forget_formula_symbols(s);
  return compiles;
}

Result<> add_data_column(xpp::Session &s, std::string_view name, std::string_view formula)
{
  DATA_VIEW *b=&s.data->view;
  if(const Result<> stored=require_stored_data(s);!stored)return stored;
  const xpp::Model &m=s.model();
  /* the added column's data_store index: right after the model's own
     columns and every column added so far (the Model stays as the load
     left it -- docs/roadmap.md W77 -- so this count never advances neq) */
  const int col_index=m.neq+1+static_cast<int>(s.data->added_columns.size());
  if(static_cast<int>(s.data->added_columns.size())>=xpp::DataStore::ADDED_COLUMNS)
    return fail("data","Too many columns",command_place());
  if(name.empty())return fail("data","Give the new column a name",command_place());
  int existing=-1;
  find_variable(s,name,&existing);
  if(existing>=0)return fail("data",xpp::format("The data already has a column named \"{}\"",name),command_place());
  std::string col_name(name);
  xpp::to_upper(col_name.data());
  s.data_store->add_column(col_index); /* max_rows zeros */
  if(const Result<> computed=compute_added_column(s,std::string(formula),col_index,b->maxrow);!computed)return computed;
  s.data->added_columns.push_back({std::move(col_name),std::string(formula)});
  b->maxcol=m.neq+1+static_cast<int>(s.data->added_columns.size());
  ui.data_redraw(1);
  return {};
}

namespace {
enum class Sequence { none, endpoints, increment };
/* a:b (endpoints) or a;b (increment), split at the last ':' or ';' (none
   when neither); each side is a number read through f, whose fault is the
   caller's */
Sequence chk_seq(xpp::TypedFields &f, std::string_view text, double &a1, double &a2)
{
  a1=0.0;
  a2=0.0;
  const size_t j=text.find_last_of(":;");
  if(j==std::string_view::npos)return Sequence::none;
  a1=f.real("sequence",std::string(text.substr(0,j)));
  a2=f.real("sequence",std::string(text.substr(j+1)));
  return text[j]==':'?Sequence::endpoints:Sequence::increment;
}
} // namespace

Result<> replace_data_column(xpp::Session &s, std::string_view column, std::string_view formula)
{
 const ColumnTable dat=s.data->view.data;
 const int n=s.data->view.maxrow;
 std::string form(formula);
 Program com;
 int i;
 int intflag=0;
 int dif_var=-1;
 Sequence seq=Sequence::none;
 double a1,a2,da=0.0;
 double old=0.0,dt,derv=0.0;
 double sum=0.0;
 const Place here=command_place();
 if(n<2)return fail("data","Replace needs at least two rows of data",here);

 dt=s.numerics->store_every*s.numerics->delta_t;
/* first check for derivative or integral symbol */
 i=0;
 while(i<static_cast<int>(form.size())){
  if(!isspace(static_cast<unsigned char>(form[static_cast<std::size_t>(i)])))break;
  i++;
  }
 const std::size_t at=static_cast<std::size_t>(i);
 if(at<form.size()&&form[at]=='&'){ intflag=1; form[at]=' ';}
 if(at<form.size()&&form[at]=='@'){
   form[at]=' ';
   find_variable(s,form,&dif_var);
   if(dif_var<0)return fail("data",xpp::format("No such variable: \"{}\"",form),here);
 }

if(dif_var<0){
  xpp::TypedFields fields;
  seq=chk_seq(fields,form,a1,a2);
  if(fields.error())return fail("data",*fields.error(),here);
 }
 if(seq==Sequence::endpoints){
   if(a1==a2)return fail("data","Illegal sequence: its ends are the same",here);
   da=(a2-a1)/static_cast<double>(n-1);
 }
 if(seq==Sequence::increment)
   da=a2;

/*  first compile formula ... */

 if(dif_var<0&&seq==Sequence::none){
   if(xpp::add_expr(s,form,com.rpn,&i)){
     s.parser.ncon=s.model().ncon_start;
     s.parser.nsym=s.model().nsym_start;
     return fail("data",xpp::format("Illegal formula: \"{}\"",form),here);
   }
 }
/* next check to see if column is known ... */

 find_variable(s,column,&i);
 if(i<0){
   s.parser.ncon=s.model().ncon_start;
   s.parser.nsym=s.model().nsym_start;
   return fail("data",xpp::format("No such column: \"{}\"",column),here);
 }
 s.data->replaced_col=i;

 /* Okay the formula is cool so lets allocate and replace  */

 wipe_rep(*s.data);
 s.data->old_column.assign(static_cast<std::size_t>(n),0.0);
 s.data->replaced=1;
 RowValues row(s);
 for(i=0;i<n;i++)
 {
   s.data->old_column[static_cast<std::size_t>(i)]=dat[s.data->replaced_col][i];
   if(dif_var<0)
     {
       if(seq==Sequence::none)
	 {
	   row.set(dat,i);
	   if(intflag)
	     {
	       sum+=xpp::evaluate(s,com);
	       dat[s.data->replaced_col][i]=sum*dt;
	     }
	   else 
	     dat[s.data->replaced_col][i]=xpp::evaluate(s,com);
	 }
       else 
	 {
	   dat[s.data->replaced_col][i]=a1+i*da;
	 }
     }
   else 
     {
       if(i==0)derv=(dat[dif_var][1]-dat[dif_var][0])/dt;
       if(i==(n-1))derv=(dat[dif_var][i]-old)/dt;
       if(i>0&&i<(n-1))derv=(dat[dif_var][i+1]-dat[dif_var][i])/dt;
       old=dat[dif_var][i];
       dat[s.data->replaced_col][i]=derv;
     }
 }
 s.parser.ncon=s.model().ncon_start;
 s.parser.nsym=s.model().nsym_start;
 ui.data_redraw(0);
 return {};
}

Result<> unreplace_column(xpp::Session &s)
{
 int i,n=s.data->view.maxrow;
 if(!s.data->replaced)return fail("data","No replacement to undo",command_place());
 for(i=0;i<n;i++)s.data->view.data[s.data->replaced_col][i]=s.data->old_column[static_cast<std::size_t>(i)];
 wipe_rep(*s.data);
 ui.data_redraw(0);
 return {};
}

Result<std::size_t> tabulate_column(xpp::Session &s, std::string_view column, double xlo, double xhi, std::string_view filename)
{
  const DATA_VIEW &b=s.data->view;
  int col=-1;
  find_variable(s,column,&col);
  if(col<0)return fail("Tabulate",xpp::format("The data has no column \"{}\"",column),command_place());
  if(!(xlo<xhi))return fail("Tabulate",xpp::format("The table's x range must have its low end below its high end: {:g} and {:g}",xlo,xhi),command_place());
  const int npts=b.iend-b.istart;
  if(npts<=0){
    ui.save_result("",false);
    return fail("Tabulate","Nothing to save: no rows are kept between First and Last",command_place());
  }
  Result<> opened;
  xpp::Writer w=open_writer_asking(filename,false,&opened);
  if(!opened)return std::unexpected<Error>(opened.error());
  if(!w)return 0;
  w.print("{}\n",npts);
  w.print("{:g}\n{:g}\n",xlo,xhi);
  for(int i=0;i<npts;i++)
    w.print("{:10.10g}\n",static_cast<double>(b.data[col][i+b.istart]));
  if(Result<> saved=commit_save(w);!saved)return std::unexpected<Error>(saved.error());
  ping();
  return static_cast<std::size_t>(npts);
}


namespace {

/* the row, from the selected one on, whose value in column col is nearest val */
void find_value(int col, double val, int *row, const DATA_VIEW &b)
{
 int n=b.maxrow;
 int i;
 int ihot=0;
 double err,errm;
 errm=fabs(b.data[col][0]-val);
 for(i=b.row0;i<n;i++){
 err=fabs(b.data[col][i]-val);
 if(err<errm){
	ihot=i;
	errm=err;
        }
  }
 *row=ihot;
}

} // namespace

Result<int> find_in_column(xpp::Session &s, std::string_view column, double value)
{
 DATA_VIEW &b=s.data->view;
 int col=-1,row=0;
 find_variable(s,column,&col);
 if(col<0)return fail("Find Data",xpp::format("The data has no column \"{}\"",column),command_place());
 if(b.maxrow<=0)return fail("Find Data","The data table holds no rows",command_place());
 find_value(col,value,&row,b);
 b.row0=row;
 ui.data_redraw(0);
 return row;
}

Result<DataLoaded> load_data_file(xpp::Session &s, std::string_view format, std::string_view name)
{
 DATA_VIEW *b=&s.data->view;
 const xpp::DataFormat *f=nullptr;
 if(!format.empty()&&!(f=xpp::data_format_named(format)))
   return fail("save data",xpp::format("No data format {}",format),command_place());
 std::string fil(name);
 if(fil.empty()){
   fil="test.dat";
   if(!file_selector("Load data",fil,"*"))return DataLoaded{};
 }
 if(!f)f=xpp::data_format_of_file(fil);
 if(!f||!f->read)f=xpp::data_format_named("dat"); /* any other name: XPP's own */
 xpp::DataTable t;
 if(!f->read(fil.c_str(),t))
   return fail("load data",xpp::format("Cannot read {} as {}{}",fil,f->title,t.error.empty()?"":": "+t.error),command_place());
 /*  The file's columns fill the stored ones in order: more columns than
     there are are left out, and every row is read, the store growing to fit. This data
     can be plotted etc like anything else */
 if(t.rows()>static_cast<std::size_t>(std::numeric_limits<int>::max()))
   return fail("load data",xpp::format("{} has {} rows: more than the data table can hold",fil,t.rows()),command_place());
 const int len=static_cast<int>(t.rows());
 if(const xpp::Result<> fitted=s.data_store->fit(s.model().neq+1,len);!fitted)return std::unexpected(fitted.error());
 for(std::size_t k=0;k<t.columns.size()&&k<static_cast<std::size_t>(b->maxcol);k++)
   b->data[k].store(0,std::span(t.columns[k]).first(static_cast<std::size_t>(len)));
 refresh_data(s,len);
 s.data_store->rows=len;
 return DataLoaded{fil,static_cast<std::size_t>(len)};
}

namespace {

/* the Save data menu of the data formats' registry; nullptr when none is
   chosen */
const xpp::DataFormat *choose_data_format()
{
 std::vector<const char *> items;
 std::string keys;
 for(const xpp::DataFormat &f : xpp::data_formats()){
   items.push_back(f.title);
   keys+=f.key;
 }
 const XppMenu m={"save_format","Save data as",static_cast<int>(items.size()),items.data(),keys.c_str()};
 const int k=menu_choose(&m,0);
 for(const xpp::DataFormat &f : xpp::data_formats())
   if(k==f.key)return &f;
 return nullptr;
}

/* the table t written to file `fil` in format f, the replace question asked as a save asks it: false when the
   user declined to replace it */
Result<bool> write_table(const xpp::Session &s, xpp::DataTable &t, const xpp::DataFormat &f, std::string_view fil, const Place &at)
{
 t.seed=s.numerics->last_seed;
 Result<> opened;
 xpp::Writer w=open_writer_asking(fil,f.binary,&opened,false,at);
 if(!opened)return std::unexpected<Error>(opened.error());
 if(!w)return false;
 if(!f.write(t,w)){
   abort_save(w);
   return fail("save data",xpp::format("Cannot write {}",fil),command_place());
 }
 if(Result<> saved=commit_save(w);!saved)return std::unexpected<Error>(saved.error());
 return true;
}

} // namespace

Result<DataSaved> save_data_file(const xpp::Session &s, std::optional<DataTarget> target, std::string_view format, std::string_view name)
{
 const DATA_VIEW *b=&s.data->view;
 if(!target){
   const int k=menu_choose(&menu_save_what,0);
   if(k!='t'&&k!='p')return DataSaved{};
   target=k=='p'?DataTarget::plot_curves:DataTarget::table;
 }
 const bool plot=*target==DataTarget::plot_curves;
 const xpp::DataFormat *f=nullptr;
 if(!format.empty()&&!(f=xpp::data_format_named(format)))
   return fail("save data",xpp::format("No data format {}",format),command_place());
 xpp::DataTable t=plot?plot_curves_table(s):data_table(s,*b,*target==DataTarget::output?output_columns(s,*b):all_columns(*b));
 if(t.rows()==0){
   ui.save_result("",false);
   return fail("save data","Nothing to save",command_place());
 }
 if(!f&&!name.empty())f=xpp::data_format_of_file(name);
 if(!f&&!(f=choose_data_format()))return DataSaved{};
 std::string fil(name);
 if(fil.empty()){
   fil=xpp::files::output_name(s.model().this_file,f->extension,plot?"curves":"");
   if(!file_selector("Save data",fil,xpp::format("*{}",f->extension)))return DataSaved{};
 }
 const Result<bool> written=write_table(s,t,*f,fil,*target==DataTarget::output ? s.output_place : Place{});
 if(!written)return std::unexpected<Error>(written.error());
 return DataSaved{fil,*written?t.rows():0};
}

void fit_data_block(xpp::Session &s)
{
 const DATA_VIEW &b=s.data->view;
 DataBlock &k=s.data->block;
 if(k.from>b.maxrow-1)k.from=b.maxrow>0?b.maxrow-1:0;
 if(k.from<0)k.from=0;
 if(k.col>b.maxcol-1)k.col=b.maxcol-1;
 if(k.col<1)k.col=1;
}

std::string data_block_rows(const xpp::Session &s)
{
 const DATA_VIEW &b=s.data->view;
 const DataBlock &k=s.data->block;
 std::string rows="[";
 const int last=b.dataflag?std::min(k.from+k.count,b.maxrow):k.from;
 const bool single=s.data_store->precision()==StoragePrecision::single;
 const auto number=[&rows,single](double z){
   if(!xpp::json::json_finite(z))rows+="null"; /* not JSON numbers */
   else xpp::append_stored(rows,z,single);
 };
 for(int i=k.from;i<last;i++){
   if(i>k.from)rows+=',';
   rows+='[';
   number(b.data[0][i]);
   for(int j=k.col;j<k.col+k.ncol&&j<b.maxcol;j++){
     rows+=',';
     number(b.data[j][i]);
   }
   rows+=']';
 }
 return rows+']';
}

Result<int> set_first_row(xpp::Session &s, int row)
{
 DATA_VIEW &b=s.data->view;
 if(row<0||row>=b.maxrow)
   return fail("data",xpp::format("Row {} is not in the data: it has {} rows (the first is 0)",row,b.maxrow),command_place());
 if(row>=b.iend)
   return fail("data",xpp::format("The first row {} must come before the last row kept, {}",row,b.iend-1),command_place());
 b.row0=row;
 b.istart=row;
 ui.data_redraw(0);
 return row;
}

Result<int> set_last_row(xpp::Session &s, int row)
{
 DATA_VIEW &b=s.data->view;
 if(row<0||row>=b.maxrow)
   return fail("data",xpp::format("Row {} is not in the data: it has {} rows (the first is 0)",row,b.maxrow),command_place());
 if(row<b.istart)
   return fail("data",xpp::format("The last row {} must not come before the first row kept, {}",row,b.istart),command_place());
 b.row0=row;
 b.iend=row+1;
 ui.data_redraw(0);
 return row;
}

Result<int> restore_whole_data(xpp::Session &s)
{
 DATA_VIEW &b=s.data->view;
 if(b.maxrow<=0)return fail("data","The data table holds no rows",command_place());
 b.istart=0;
 b.iend=b.maxrow;
 xpp::restore(s,b.istart,b.iend);
 ui.data_redraw(0);
 return b.maxrow-1;
}

} // namespace xpp
