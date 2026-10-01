/* The data side of the browser: the one BROWSER instance, its storage
   pointer, row/column bookkeeping and the file writer. No X11 here; the
   widget code that displays it stays in browse.c. */
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "model.h"
#include "session.h"
#include "getvar.h"
#include "xpp_util.h"
#include "storage.h"
#include "form_ode.h"
#include "expr.h"
#include "xpp_io.h"
#include "browse.h"
#include "xpp_ui.h"
#include "integrate.h"
#include <cctype>
#include <cstring>
#include <strings.h>
#include "load_eqn.h"
#include "data_formats.h"
#include "graf_par.h"
#include "menus.h"
#include <algorithm>
#include <span>

namespace xpp {

float **get_browser_data(xpp::Session &s)
{
  return s.browser.view.data;
}

/* show another data set in the browser: its columns from new_dat[1] on,
   dat_len rows (the adjoint, the Fourier modes, a histogram, ...) */
void new_browse_dat(xpp::Session &s, float **new_dat, int dat_len)
{
  s.browser.view.data=new_dat;
  refresh_browser(s,dat_len);
}

float *get_data_col(const xpp::Session &s, int c)
{
  return s.browser.view.data[c];
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
  const std::vector<AddedColumn> &added=s.browser.added_columns;
  if(j>m.neq){
    const std::size_t k=static_cast<std::size_t>(j-m.neq-1);
    if(k<added.size())return added[k].name;
  }
  return "";
}

namespace {

/* the rows First..Last of b's columns cols */
xpp::DataTable browser_table(const xpp::Session &s, const BROWSER &b, std::span<const int> cols)
{
  xpp::DataTable t;
  for(int j : cols){
    t.names.push_back(browse_column_name(s,j));
    const float *c=b.data[j];
    if(b.iend>b.istart)t.columns.emplace_back(c+b.istart,c+b.iend);
    else t.columns.emplace_back();
  }
  return t;
}

/* every column b has */
std::vector<int> all_columns(const BROWSER &b)
{
  std::vector<int> cols(b.maxcol>0?b.maxcol:0);
  for(std::size_t j=0;j<cols.size();j++)cols[j]=static_cast<int>(j);
  return cols;
}

} // namespace

namespace {

/* the columns a batch run writes: the "only" list, else every column */
std::vector<int> output_columns(const xpp::Session &s, const BROWSER &b)
{
  return !s.plot_list.empty()?s.plot_list:all_columns(b);
}

} // namespace

void write_mybrowser_data(xpp::Session &s, xpp::Writer &w)
{
  const BROWSER &b=s.browser.view;
  xpp::data_format_named("dat")->write(browser_table(s,b,output_columns(s,b)),w);
}

xpp::DataTable stored_data_table(const xpp::Session &s)
{
  const int ncol=s.model().neq+1;
  xpp::DataTable t;
  for(int j=0;j<ncol;j++){
    t.names.push_back(browse_column_name(s,j));
    const float *c=s.data_store.col[j];
    t.columns.emplace_back(c,c+s.data_store.rows);
  }
  t.seed=s.numerics.last_seed;
  return t;
}

int put_stored_data(xpp::Session &s, const xpp::DataTable &t)
{
  const int neq=s.model().neq;
  const int rows=static_cast<int>(t.rows());
  if(rows>s.data_store.max_rows){
    if(const xpp::Result<> r=s.data_store.grow(neq+1,rows);!r){
      xpp::show_error(r.error());
      return 0;
    }
    s.data_store.max_rows=rows;
  }
  for(std::size_t k=0;k<t.columns.size();k++){
    int col;
    find_variable(s,xpp::data_column_name(t,k),&col);
    if(col<0||col>neq||t.columns[k].size()<static_cast<std::size_t>(rows))continue;
    std::copy(t.columns[k].begin(),t.columns[k].begin()+rows,s.data_store.col[col]);
  }
  s.data_store.rows=rows;
  /* the store's own columns, which may just have grown (moved): not a
     derived data set shown before, whose lent columns would be stale */
  s.browser.view.data=s.data_store.col;
  refresh_browser(s,rows);
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
  const std::vector<AddedColumn> &added=s.browser.added_columns;
  for(std::size_t k=0;k<added.size();k++){
    if(xpp::equal_ignoring_case(added[k].name,name)){
      *col=s.model().neq+1+static_cast<int>(k);
      return;
    }
  }
 }

void  refresh_browser(xpp::Session &s, int length)
{
 s.browser.view.dataflag=1;
 s.browser.view.maxrow=length;
 s.browser.view.iend=length;
 if(s.browser.view.data==s.data_store.col){
   /* a fresh run's own data, the Model's own columns unchanged: recompute
      every added column over it (docs/manual/07-data-browser.md: it
      stays computed "as though ... another auxiliary variable") rather
      than dropping it (docs/roadmap.md W77) */
   const xpp::Model &m=s.model();
   for(std::size_t k=0;k<s.browser.added_columns.size();k++){
     const int col_index=m.neq+1+static_cast<int>(k);
     s.data_store.add_column(col_index); /* fresh max_rows zeros */
     compute_added_column(s,s.browser.added_columns[k].formula,col_index,length);
   }
   s.browser.view.maxcol=m.neq+1+static_cast<int>(s.browser.added_columns.size());
 }
 ui.data_changed(length);
}

void reset_browser(xpp::Session &s)
{
  s.browser.view.maxrow=0;
  s.browser.view.dataflag=0;
}

void init_browser(xpp::Session &s)
{
 
 s.browser.view.dataflag=0;
 s.browser.view.data=s.data_store.col;
 s.browser.view.maxcol=s.model().neq+1;
 s.browser.view.maxrow=0;
 s.browser.view.row0=0;
 s.browser.view.istart=0;
 s.browser.view.iend=0;
 s.browser.added_columns.clear();

}

namespace {

/* 1 when fil does not exist yet or may be overwritten */
bool may_write_file(std::string_view fil)
{
 if(!xpp::files::exists(fil))return true;
 return static_cast<char>(TwoChoice("Yes","No",
		"File Exists! Overwrite?","yn"))=='y';
}

} // namespace

namespace {

/* fil opened for a write that replaces it at commit, whether or not it
   exists: an empty Writer when it cannot be written (err_msg says so) */
xpp::Writer open_writer(std::string_view fil, bool binary)
{
 xpp::Writer w=binary?xpp::Writer::binary(fil):xpp::Writer(fil);
 if(!w)err_msg("Cannot open file");
 return w;
}

} // namespace

xpp::Writer open_writer_asking(std::string_view fil, bool binary)
{
 if(!may_write_file(fil))return xpp::Writer();
 return open_writer(fil,binary);
}

void  wipe_rep(BrowserState &b)
 {
    if(!b.replaced)return;
    std::vector<float>().swap(b.old_column);
    b.replaced=0;
  }

void data_get(xpp::Session &s, BROWSER *b)
{
 int i,in=b->row0;
 setvar(s,0,static_cast<double>(s.data_store.col[0][in]));
 for(i=0;i<s.model().node;i++)
 {
  s.last_ic[i]=static_cast<double>(s.data_store.col[i+1][in]);
  setvar(s,i+1,s.last_ic[i]);
 } 
 for(i=0;i<s.model().nmarkov;i++){
   s.last_ic[i+s.model().node]=static_cast<double>(s.data_store.col[i+s.model().node+1][in]);
   setvar(s,i+1+s.model().node+s.model().fix_var,s.last_ic[i+s.model().node]);
 }
 for(i=s.model().node+s.model().nmarkov;i<s.model().neq;i++)
   xpp::set_val(s,s.model().uvar_names[i],s.data_store.col[i+1][in]);

 redraw_ics();
}

void data_get_mybrowser(xpp::Session &s, int row)
{
  s.browser.view.row0=row;
  data_get(s,&s.browser.view);
}

void get_data_xyz(const xpp::Session &s, float *x, float *y, float *z, int i1, int i2, int i3, int off)
{
  int in=s.browser.view.row0+off;
  *x=s.browser.view.data[i1][in];
  *y=s.browser.view.data[i2][in];
  *z=s.browser.view.data[i3][in];
}

/* ---- the browser's commands (were in browse.c); the widget calls them ---- */

int check_for_stor(const xpp::Session &s, float **data)
{
 if(data!=s.data_store.col){
   err_msg("Only data can be in browser");
   return(0);
 }
   else return(1);

}

void data_del_col(const xpp::Session &s, BROWSER *b)  /*  this only works with storage  */
{
    if(check_for_stor(s,b->data)==0)return;
  err_msg("Sorry - not working very well yet...");
}

void data_add_col(xpp::Session &s, BROWSER *b)
{
  int status;
  std::string var,form;
   if(check_for_stor(s,b->data)==0)return;
  status=get_dialog("Add Column","Name",var,"Ok","Cancel");
  if(status!=0){
    status=get_dialog_of("Add Column","Formula:",form,"Add it","Cancel",XPP_FIELD_EXPRESSION);
     if(status!=0)
      add_stor_col(s,var,form,b);
  }
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
  void set(float *const *data, int row)
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

bool compute_added_column(xpp::Session &s, const std::string &formula, int col_index, int nrows)
{
  int com[4000],i;
  const xpp::Model &m=s.model();
  if(xpp::add_expr(s,formula,com,&i)){
    err_msg("Bad Formula .... ");
    return false;
  }
  RowValues row(s);
  for(i=0;i<nrows;i++){
    row.set(s.data_store.col,i);
    s.data_store.col[col_index][i]=static_cast<float>(xpp::evaluate(s,com));
  }
  /* add_expr may have added constants to the parser's working symbol
     table (ParserState::ncon/nsym, session.h): roll it back to the
     Model's own end, like a histogram condition (histogram.cpp) -- the
     added column is not a symbol a later formula can name */
  s.parser.ncon=m.ncon_start;
  s.parser.nsym=m.nsym_start;
  return true;
}

int add_stor_col(xpp::Session &s, std::string_view name, const std::string &formula, BROWSER *b)
{
  const xpp::Model &m=s.model();

  /* the added column's data_store index: right after the model's own
     columns and every column data_add_col has added so far (the Model
     stays as the load left it -- docs/roadmap.md W77 -- so this count
     never advances neq) */
  const int col_index=m.neq+1+static_cast<int>(s.browser.added_columns.size());
  if(col_index>MAXODE){
    err_msg("Too many columns");
    return(0);
  }
  s.data_store.add_column(col_index); /* max_rows zeros */
  if(!compute_added_column(s,formula,col_index,b->maxrow))return(0);
  std::string col_name(name);
  xpp::to_upper(col_name.data());
  s.browser.added_columns.push_back({std::move(col_name),formula});
  b->maxcol=m.neq+1+static_cast<int>(s.browser.added_columns.size());
  ui.browser_redraw(1);
  return(1);
}

/* a:b (seq 1) or a;b (seq 2), split at the last ':' or ';' */
void chk_seq(std::string_view s,int *seq, double *a1, double *a2)
{
  *seq=0;
  *a1=0.0;
  *a2=0.0;
  const size_t j=s.find_last_of(":;");
  if(j==std::string_view::npos)return;
  *seq=s[j]==':'?1:2;
  *a1=std::atof(std::string(s.substr(0,j)).c_str());
  *a2=std::atof(std::string(s.substr(j+1)).c_str());
}

void replace_column(xpp::Session &s, const char *var, char *form, float **dat, int n)
{
 int com[200],i;
 int intflag=0;
 int dif_var=-1;
 int seq=0;
 double a1,a2,da=0.0;
 float old=0.0,dt,derv=0.0;
 float sum=0.0;
 if(n<2)return;

 dt=s.numerics.njmp*s.numerics.delta_t;
/* first check for derivative or integral symbol */
i=0;
while(i<static_cast<int>(strlen(form))){
  if(!isspace(form[i]))break;
  i++;
  }
 if(form[i]=='&'){ intflag=1; form[i]=' ';}
 if(form[i]=='@'){
   form[i]=' ';
   find_variable(s,form,&dif_var);
   if(dif_var<0){
     err_msg("No such variable");
     return;
   }

 }

if(dif_var<0)
  chk_seq(form,&seq,&a1,&a2);
 if(seq==1){
   if(a1==a2)
     seq=3;
   else
     da=(a2-a1)/static_cast<double>(n-1);
 }
 if(seq==2)
   da=a2;
 if(seq==3){
   err_msg("Illegal sequence");
   return;
 }

/*  first compile formula ... */

 if(dif_var<0&&seq==0){
   if(xpp::add_expr(s,form,com,&i)){
     s.parser.ncon=s.model().ncon_start;
     s.parser.nsym=s.model().nsym_start;
     err_msg("Illegal formula...");
     return;
   }
 }
/* next check to see if column is known ... */

 find_variable(s,var,&i);
 if(i<0){
   err_msg("No such column...");
   s.parser.ncon=s.model().ncon_start;
   s.parser.nsym=s.model().nsym_start;
   return;
 }
 s.browser.replaced_col=i;

 /* Okay the formula is cool so lets allocate and replace  */

 wipe_rep(s.browser);
 s.browser.old_column.assign(n,0.0f);
 s.browser.replaced=1;
 RowValues row(s);
 for(i=0;i<n;i++)
 {
   s.browser.old_column[i]=dat[s.browser.replaced_col][i];
   if(dif_var<0)
     {
       if(seq==0)
	 {
	   row.set(dat,i);
	   if(intflag)
	     {
	       sum+=static_cast<float>(xpp::evaluate(s,com));
	       dat[s.browser.replaced_col][i]=sum*dt;
	     }
	   else 
	     dat[s.browser.replaced_col][i]=static_cast<float>(xpp::evaluate(s,com));
	 }
       else 
	 {
	   dat[s.browser.replaced_col][i]=static_cast<float>(a1+i*da);
	 }
     }
   else 
     {
       if(i==0)derv=(dat[dif_var][1]-dat[dif_var][0])/dt;
       if(i==(n-1))derv=(dat[dif_var][i]-old)/dt;
       if(i>0&&i<(n-1))derv=(dat[dif_var][i+1]-dat[dif_var][i])/dt;
       old=dat[dif_var][i];
       dat[s.browser.replaced_col][i]=derv;
     }
 }
 s.parser.ncon=s.model().ncon_start;
 s.parser.nsym=s.model().nsym_start;

}

void unreplace_column(xpp::Session &s)

{
 int i,n=s.browser.view.maxrow;
 if(!s.browser.replaced)return;
 for(i=0;i<n;i++)s.browser.view.data[s.browser.replaced_col][i]=s.browser.old_column[i];
 wipe_rep(s.browser);
 
 }

void make_d_table(double xlo, double xhi, int col, std::string_view filename, BROWSER b)
{
  int i,npts;
  xpp::Writer w=open_writer_asking(filename);
  if(!w)return;
  npts=b.iend-b.istart;
  w.print("{}\n",npts);
  w.print("{:g}\n{:g}\n",xlo,xhi);
  for(i=0;i<npts;i++)
    w.print("{:10.10g}\n",static_cast<double>(b.data[col][i+b.istart]));
  w.commit();
  ping();
}

void find_value(int col, double val, int *row, BROWSER b)
{
 int n=b.maxrow;
 int i;
 int ihot=0;
 float err,errm;
 errm=static_cast<float>(fabs(b.data[col][0]-val));
 for(i=b.row0;i<n;i++){
 err=static_cast<float>(fabs(b.data[col][i]-val));
 if(err<errm){
	ihot=i;
	errm=err;
        }
  }
 *row=ihot;
}

void data_replace(xpp::Session &s, BROWSER *b)
{
 int status;
 std::string var=s.model().uvar_names[0],form=s.model().uvar_names[0];
status=get_dialog_of("Replace","Variable:",var,"Ok","Cancel",XPP_FIELD_NAME_IN(0));
if(status!=0){
 status=get_dialog_of("Replace","Formula:",form,"Replace","Cancel",XPP_FIELD_EXPRESSION);
 if(status!=0)replace_column(s,var.data(),form.data(),b->data,b->maxrow);
 ui.browser_redraw(0);
}

 }

void data_unreplace(xpp::Session &s)
{
 unreplace_column(s);
 ui.browser_redraw(0);
}

void data_table(const xpp::Session &s, BROWSER *b)
{
 int status;

 static const char *const name[]={"Variable","Xlo","Xhi","File"};
 std::array<std::string, 4> value;

 double xlo=0,xhi=1;
 int col;
 value[0] = s.model().uvar_names[0];
 value[1] = "0.00";
 value[2] = "1.00";
 value[3] = value[0] + ".tab";
 static const int kinds[]={XPP_FIELD_NAME_IN(0),XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_FILE};
 status=do_string_box_of(4,1,"Tabulate",name,value,kinds);
 if(status==0)return;
 xlo=atof(value[1].c_str());
 xhi=atof(value[2].c_str());
 find_variable(s,value[0].c_str(),&col);
  if(col>=0)
   make_d_table(xlo,xhi,col,value[3],*b);
}

void data_find(const xpp::Session &s, BROWSER *b)
{
 int status;

 static const char *const name[]={"*0Variable","Value"};
 std::array<std::string, 2> value;
 int col,row=-1;

 double val;

 value[0] = s.model().uvar_names[0];
 value[1] = "0.00";
 static const int kinds[]={XPP_FIELD_TEXT,XPP_FIELD_NUMBER};
 status=do_string_box_of(2,1,"Find Data",name,value,kinds);

 if(status==0)return;
 val=atof(value[1].c_str());
 find_variable(s,value[0].c_str(),&col);
 if(col>=0)find_value(col,val,&row,*b);
 if(row>=0){
	    b->row0=row;
	    ui.browser_redraw(0);
	   }

}

void data_read(xpp::Session &s, BROWSER *b, std::string_view format, std::string_view name)
{
 const xpp::DataFormat *f=nullptr;
 if(!format.empty()&&!(f=xpp::data_format_named(format))){
   err_msg(xpp::format("No data format {}",format));
   return;
 }
 std::string fil(name);
 if(fil.empty()){
   fil="test.dat";
   if(!file_selector("Load data",fil,"*"))return;
 }
 if(!f)f=xpp::data_format_of_file(fil);
 if(!f||!f->read)f=xpp::data_format_named("dat"); /* any other name: XPP's own */
 xpp::DataTable t;
 if(!f->read(fil.c_str(),t)){
   respond_box("Ok",xpp::format("Cannot read {} as {}",fil,f->title));
   return;
 }
 /*  The file's columns fill the stored ones in order: more columns than
     there are are left out, and at most max_rows rows are read. This data
     can be plotted etc like anything else */
 const int len=static_cast<int>(std::min<std::size_t>(t.rows(),static_cast<std::size_t>(s.data_store.max_rows)));
 for(std::size_t k=0;k<t.columns.size()&&k<static_cast<std::size_t>(b->maxcol);k++)
   std::copy(t.columns[k].begin(),t.columns[k].begin()+len,b->data[k]);
 refresh_browser(s,len);
 s.data_store.rows=len;
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
 const XppMenu m={"save_format","Save data as",static_cast<int>(items.size()),items.data(),keys.c_str(),nullptr,-1};
 const int k=menu_choose(&m,0);
 for(const xpp::DataFormat &f : xpp::data_formats())
   if(k==f.key)return &f;
 return nullptr;
}

} // namespace

void data_write(const xpp::Session &s, BROWSER *b, std::string_view what, std::string_view format, std::string_view name, bool replace)
{
 bool plot;
 if(what.empty()){
   const int k=menu_choose(&menu_save_what,0);
   if(k!='t'&&k!='p')return;
   plot=k=='p';
 }
 else if(what=="table"||what=="plot"||what=="output")plot=what=="plot";
 else {
   err_msg(xpp::format("Save data writes the table, the output or the plot, not {}",what));
   return;
 }
 const xpp::DataFormat *f=nullptr;
 if(!format.empty()&&!(f=xpp::data_format_named(format))){
   err_msg(xpp::format("No data format {}",format));
   return;
 }
 if(!f&&!name.empty())f=xpp::data_format_of_file(name);
 if(!f&&!(f=choose_data_format()))return;
 std::string fil(name);
 if(fil.empty()){
   fil=std::string(plot?"curves":"data")+f->extension;
   if(!file_selector("Save data",fil,xpp::format("*{}",f->extension)))return;
 }
 xpp::DataTable t=plot?plot_curves_table(s):browser_table(s,*b,what=="output"?output_columns(s,*b):all_columns(*b));
 t.seed=s.numerics.last_seed;
 xpp::Writer w=replace?open_writer(fil.c_str(),f->binary):open_writer_asking(fil.c_str(),f->binary);
 if(!w)return;
 if(!f->write(t,w)){
   err_msg(xpp::format("Cannot write {}",fil));
   return;
 }
 w.commit();
}

void  data_first(BROWSER *b)
{
 b->istart=b->row0;
}

void  data_last(BROWSER *b)
{
 b->iend=b->row0+1;
}

void  data_restore(xpp::Session &s, BROWSER *b)
 {
  xpp::restore(s,b->istart,b->iend);

  }

} // namespace xpp
