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

float **get_browser_data()
{
  return xpp::session().browser.view.data;
}

/* show another data set in the browser: its columns from new_dat[1] on,
   dat_len rows (the adjoint, the Fourier modes, a histogram, ...) */
void new_browse_dat(float **new_dat, int dat_len)
{
  xpp::session().browser.view.data=new_dat;
  refresh_browser(dat_len);
}

float *get_data_col(int c)
{
  return xpp::session().browser.view.data[c];
}

void waitasec(int msec)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}

std::string browse_column_name(int j)
{
  if(j==0)return "T";
  const xpp::Model &m=xpp::model();
  if(j>0&&j<=m.neq)return m.uvar_names[j-1];
  const std::vector<std::string> &added=xpp::session().browser.added_columns;
  if(j>m.neq){
    const std::size_t k=static_cast<std::size_t>(j-m.neq-1);
    if(k<added.size())return added[k];
  }
  return "";
}

namespace {

/* the rows First..Last of b's columns cols */
xpp::DataTable browser_table(const BROWSER &b, std::span<const int> cols)
{
  xpp::DataTable t;
  for(int j : cols){
    t.names.push_back(browse_column_name(j));
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

void write_mybrowser_data(xpp::Writer &w)
{
  const BROWSER &b=xpp::session().browser.view;
  const std::vector<int> cols=N_plist>0?std::vector<int>(plotlist,plotlist+N_plist):all_columns(b);
  xpp::data_format_named("dat")->write(browser_table(b,cols),w);
}

void find_variable(std::string_view s, int *col)
{
 *col=-1;
  if(xpp::equal_ignoring_case("T",s)){
   *col=0;
    return;
   }
  *col=find_user_name(2,s);
  if(*col>-1){
    *col=*col+1;
    return;
  }
  const std::vector<std::string> &added=xpp::session().browser.added_columns;
  for(std::size_t k=0;k<added.size();k++){
    if(xpp::equal_ignoring_case(added[k],s)){
      *col=xpp::model().neq+1+static_cast<int>(k);
      return;
    }
  }
 }

void  refresh_browser(int length)
{
 xpp::Session &s=xpp::session();
 s.browser.view.dataflag=1;
 s.browser.view.maxrow=length;
 s.browser.view.iend=length;
 if(s.browser.view.data==s.data_store.col){
   /* a fresh run's own data: an earlier data_add_col column was not
      recomputed for it and the Model's own columns are unchanged, so
      drop it rather than show it stale (docs/roadmap.md W77) */
   s.browser.added_columns.clear();
   s.browser.view.maxcol=xpp::model().neq+1;
 }
 xpp_ui.data_changed(length);
}

void reset_browser()
{
  xpp::session().browser.view.maxrow=0;
  xpp::session().browser.view.dataflag=0;
}

void init_browser()
{
 
 xpp::session().browser.view.dataflag=0;
 xpp::session().browser.view.data=xpp::session().data_store.col;
 xpp::session().browser.view.maxcol=xpp::model().neq+1;
 xpp::session().browser.view.maxrow=0;
 xpp::session().browser.view.row0=0;
 xpp::session().browser.view.istart=0;
 xpp::session().browser.view.iend=0;
 xpp::session().browser.added_columns.clear();

}

namespace {

/* 1 when fil does not exist yet or may be overwritten */
bool may_write_file(const char *fil)
{
 if(!xpp_files_exists(fil))return true;
 return static_cast<char>(TwoChoice("Yes","No",
		"File Exists! Overwrite?","yn"))=='y';
}

} // namespace

xpp::Writer open_writer_asking(const char *fil, bool binary)
{
 if(!may_write_file(fil))return xpp::Writer();
 xpp::Writer w=binary?xpp::Writer::binary(fil):xpp::Writer(fil);
 if(!w)err_msg("Cannot open file");
 return w;
}

void  wipe_rep()
 {
    if(!xpp::session().browser.replaced)return;
    std::vector<float>().swap(xpp::session().browser.old_column);
    xpp::session().browser.replaced=0;
  }

void data_get(BROWSER *b)
{
 int i,in=b->row0;
 set_ivar(0,static_cast<double>(xpp::session().data_store.col[0][in]));
 for(i=0;i<xpp::model().node;i++)
 {
  xpp::session().last_ic[i]=static_cast<double>(xpp::session().data_store.col[i+1][in]);
  set_ivar(i+1,xpp::session().last_ic[i]);
 } 
 for(i=0;i<xpp::model().nmarkov;i++){
   xpp::session().last_ic[i+xpp::model().node]=static_cast<double>(xpp::session().data_store.col[i+xpp::model().node+1][in]);
   set_ivar(i+1+xpp::model().node+xpp::model().fix_var,xpp::session().last_ic[i+xpp::model().node]);
 }
 for(i=xpp::model().node+xpp::model().nmarkov;i<xpp::model().neq;i++)
   set_val(xpp::model().uvar_names[i],xpp::session().data_store.col[i+1][in]);

 redraw_ics();
}

extern "C" void data_get_mybrowser(int row)
{
  xpp::session().browser.view.row0=row;
  data_get(&xpp::session().browser.view);
}

void get_data_xyz(float *x, float *y, float *z, int i1, int i2, int i3, int off)
{
  int in=xpp::session().browser.view.row0+off;
  *x=xpp::session().browser.view.data[i1][in];
  *y=xpp::session().browser.view.data[i2][in];
  *z=xpp::session().browser.view.data[i3][in];
}

/* ---- the browser's commands (were in browse.c); the widget calls them ---- */

int check_for_stor(float **data)
{
 if(data!=xpp::session().data_store.col){
   err_msg("Only data can be in browser");
   return(0);
 }
   else return(1);

}

void data_del_col(BROWSER *b)  /*  this only works with storage  */
{
    if(check_for_stor(b->data)==0)return;
  err_msg("Sorry - not working very well yet...");
}

void data_add_col(BROWSER *b)
{
  int status;
  std::string var,form;
   if(check_for_stor(b->data)==0)return;
  status=get_dialog("Add Column","Name",var,"Ok","Cancel",XPP_NAME_MAX);
  if(status!=0){
    status=get_dialog_of("Add Column","Formula:",form,"Add it","Cancel",80,XPP_FIELD_EXPRESSION);
     if(status!=0)
      add_stor_col(var.c_str(),form.c_str(),b);
  }
}

int add_stor_col(const char *name, const char *formula, BROWSER *b)
{
  int com[4000],i,j;
  xpp::Session &s=xpp::session();
  const xpp::Model &m=xpp::model();

  if(strlen(name)>XPP_NAME_MAX){
    err_msg("Name too long");
    return(0);
  }
  /* the added column's data_store index: right after the model's own
     columns and every column data_add_col has added so far (the Model
     stays as the load left it -- docs/roadmap.md W77 -- so this count
     never advances neq) */
  const int col_index=m.neq+1+static_cast<int>(s.browser.added_columns.size());
  if(col_index>MAXODE){
    err_msg("Too many columns");
    return(0);
  }
  if(add_expr(formula,com,&i)){
    err_msg("Bad Formula .... ");
    return(0);
  }
  s.data_store.add_column(col_index); /* max_rows zeros */
  for(i=0;i<b->maxrow;i++){
    for(j=0;j<m.node+1;j++)set_ivar(j,static_cast<double>(s.data_store.col[j][i]));
    for(j=m.node;j<m.neq;j++)set_val(m.uvar_names[j],static_cast<double>(s.data_store.col[j+1][i]));
    s.data_store.col[col_index][i]=static_cast<float>(evaluate(com));
  }
  /* add_expr may have added constants to the parser's working symbol
     table (ParserState::ncon/nsym, session.h): roll it back to the
     Model's own end, like a histogram condition (histogram.cpp) -- the
     added column is not a symbol a later formula can name */
  s.parser.ncon=m.ncon_start;
  s.parser.nsym=m.nsym_start;
  std::string col_name(name);
  xpp::to_upper(col_name.data());
  s.browser.added_columns.push_back(std::move(col_name));
  b->maxcol=m.neq+1+static_cast<int>(s.browser.added_columns.size());
  xpp_ui.browser_redraw(1);
  return(1);
}

/* a:b (seq 1) or a;b (seq 2), split at the last ':' or ';' */
void chk_seq(const char *f,int *seq, double *a1, double *a2)
{
  const std::string_view s(f);
  *seq=0;
  *a1=0.0;
  *a2=0.0;
  const size_t j=s.find_last_of(":;");
  if(j==std::string_view::npos)return;
  *seq=s[j]==':'?1:2;
  *a1=std::atof(std::string(s.substr(0,j)).c_str());
  *a2=std::atof(std::string(s.substr(j+1)).c_str());
}

void replace_column(const char *var, char *form, float **dat, int n)
{
 xpp::Session &s=xpp::session();
 int com[200],i,j;
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
   find_variable(form,&dif_var);
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
   if(add_expr(form,com,&i)){
     s.parser.ncon=xpp::model().ncon_start;
     s.parser.nsym=xpp::model().nsym_start;
     err_msg("Illegal formula...");
     return;
   }
 }
/* next check to see if column is known ... */

 find_variable(var,&i);
 if(i<0){
   err_msg("No such column...");
   s.parser.ncon=xpp::model().ncon_start;
   s.parser.nsym=xpp::model().nsym_start;
   return;
 }
 s.browser.replaced_col=i;

 /* Okay the formula is cool so lets allocate and replace  */

 wipe_rep();
 s.browser.old_column.assign(n,0.0f);
 s.browser.replaced=1;
 for(i=0;i<n;i++)
 {
   s.browser.old_column[i]=dat[s.browser.replaced_col][i];
   if(dif_var<0)
     {
       if(seq==0)
	 {
	   for(j=0;j<xpp::model().node+1;j++)set_ivar(j,static_cast<double>(dat[j][i]));
	   for(j=xpp::model().node;j<xpp::model().neq;j++)set_val(xpp::model().uvar_names[j],static_cast<double>(dat[j+1][i]));
	   if(intflag)
	     {
	       sum+=static_cast<float>(evaluate(com));
	       dat[s.browser.replaced_col][i]=sum*dt;
	     }
	   else 
	     dat[s.browser.replaced_col][i]=static_cast<float>(evaluate(com));
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
 s.parser.ncon=xpp::model().ncon_start;
 s.parser.nsym=xpp::model().nsym_start;

}

void unreplace_column()

{
 int i,n=xpp::session().browser.view.maxrow;
 if(!xpp::session().browser.replaced)return;
 for(i=0;i<n;i++)xpp::session().browser.view.data[xpp::session().browser.replaced_col][i]=xpp::session().browser.old_column[i];
 wipe_rep();
 
 }

void make_d_table(double xlo, double xhi, int col, const char *filename, BROWSER b)
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

void data_replace(BROWSER *b)
{
 int status;
 std::string var=xpp::model().uvar_names[0],form=xpp::model().uvar_names[0];
status=get_dialog_of("Replace","Variable:",var,"Ok","Cancel",XPP_NAME_MAX,XPP_FIELD_NAME_IN(0));
if(status!=0){
 status=get_dialog_of("Replace","Formula:",form,"Replace","Cancel",80,XPP_FIELD_EXPRESSION);
 if(status!=0)replace_column(var.data(),form.data(),b->data,b->maxrow);
 xpp_ui.browser_redraw(0);
}

 }

void data_unreplace(BROWSER *b)
{
 unreplace_column();
 xpp_ui.browser_redraw(0);
}

void data_table(BROWSER *b)
{
 int status;

 static const char *name[]={"Variable","Xlo","Xhi","File"};
 std::array<std::string, 4> value;

 double xlo=0,xhi=1;
 int col;
 value[0] = xpp::model().uvar_names[0];
 value[1] = "0.00";
 value[2] = "1.00";
 value[3] = value[0].substr(0, XPP_NAME_MAX) + ".tab";
 static const int kinds[]={XPP_FIELD_NAME_IN(0),XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_FILE};
 status=do_string_box_of(4,1,"Tabulate",name,value,40,kinds);
 if(status==0)return;
 xlo=atof(value[1].c_str());
 xhi=atof(value[2].c_str());
 find_variable(value[0].c_str(),&col);
  if(col>=0)
   make_d_table(xlo,xhi,col,value[3].c_str(),*b);
}

void data_find(BROWSER *b)
{
 int status;

 static const char *name[]={"*0Variable","Value"};
 std::array<std::string, 2> value;
 int col,row=-1;

 double val;

 value[0] = xpp::model().uvar_names[0];
 value[1] = "0.00";
 static const int kinds[]={XPP_FIELD_TEXT,XPP_FIELD_NUMBER};
 status=do_string_box_of(2,1,"Find Data",name,value,40,kinds);

 if(status==0)return;
 val=atof(value[1].c_str());
 find_variable(value[0].c_str(),&col);
 if(col>=0)find_value(col,val,&row,*b);
 if(row>=0){
	    b->row0=row;
	    xpp_ui.browser_redraw(0);
	   }

}

void data_read(BROWSER *b, std::string_view format, std::string_view name)
{
 const xpp::DataFormat *f=nullptr;
 if(!format.empty()&&!(f=xpp::data_format_named(format))){
   err_msg(xpp::format("No data format {}",format).c_str());
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
   respond_box("Ok",xpp::format("Cannot read {} as {}",fil,f->title).c_str());
   return;
 }
 /*  The file's columns fill the stored ones in order: more columns than
     there are are left out, and at most max_rows rows are read. This data
     can be plotted etc like anything else */
 const int len=static_cast<int>(std::min<std::size_t>(t.rows(),static_cast<std::size_t>(xpp::session().data_store.max_rows)));
 for(std::size_t k=0;k<t.columns.size()&&k<static_cast<std::size_t>(b->maxcol);k++)
   std::copy(t.columns[k].begin(),t.columns[k].begin()+len,b->data[k]);
 refresh_browser(len);
 xpp::session().data_store.rows=len;
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

void data_write(BROWSER *b, std::string_view what, std::string_view format, std::string_view name)
{
 bool plot;
 if(what.empty()){
   const int k=menu_choose(&menu_save_what,0);
   if(k!='t'&&k!='p')return;
   plot=k=='p';
 }
 else if(what=="table"||what=="plot")plot=what=="plot";
 else {
   err_msg(xpp::format("Save data writes the table or the plot, not {}",what).c_str());
   return;
 }
 const xpp::DataFormat *f=nullptr;
 if(!format.empty()&&!(f=xpp::data_format_named(format))){
   err_msg(xpp::format("No data format {}",format).c_str());
   return;
 }
 if(!f&&!name.empty())f=xpp::data_format_of_file(name);
 if(!f&&!(f=choose_data_format()))return;
 std::string fil(name);
 if(fil.empty()){
   fil=std::string(plot?"curves":"data")+f->extension;
   if(!file_selector("Save data",fil,xpp::format("*{}",f->extension).c_str()))return;
 }
 xpp::DataTable t=plot?plot_curves_table():browser_table(*b,all_columns(*b));
 t.seed=xpp::session().numerics.last_seed;
 xpp::Writer w=open_writer_asking(fil.c_str(),f->binary);
 if(!w)return;
 if(!f->write(t,w)){
   err_msg(xpp::format("Cannot write {}",fil).c_str());
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

void  data_restore(BROWSER *b)
 {
  restore(b->istart,b->iend);

  }

