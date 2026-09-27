#include "tabular.h"
#include "session.h"
#include "storage.h"
#include "xpp_files.h"
#include "load_eqn.h"
#include "xpp_ui.h"

#include "browse.h"
#include "many_pops.h"
#include "simplenet.h"

#include "parserslow.h"

#include <stdlib.h>
#include <string.h>
#include <array>
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

    If the first line of the file says "xyvals" then the table is of the
    form: x1 < x2 < .... < xn
    npts
    x1 y1
    x2 y2 
     ...
    xn yn

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
#include "model.h"


namespace {
/* table index's values y to length doubles, keeping what it holds (what
   is added is zero); every caller then fills [0,length) itself, so a
   regrow after a shrink leaving stale rather than zeroed values (unlike
   xpp_realloc, std::vector::resize does not re-zero capacity it already
   had) never shows. y stays a raw double*: see TABULAR (tabular.h). */
void resize_values(int index, int length)
{
  xpp::Session &s=xpp::session();
  s.tables[index].y_storage.resize(static_cast<size_t>(length));
  s.tables[index].y=s.tables[index].y_storage.data();
}
}

void set_auto_eval_flags(int f)
{
 int i;
  for(i=0;i<MAX_TAB;i++) 
    xpp::session().tables[i].autoeval=f;
}
void set_table_name(const char *name, int index)
{
  xpp::session().tables[index].name=name;
}

void view_table(int index)
{
  xpp::Session &s=xpp::session();
  int i;
  int n=s.tables[index].n,len;
  double *y=s.tables[index].y;
  double xlo=s.tables[index].xlo,dx=s.tables[index].dx;
  len=n;
  if(len>=xpp::session().data_store.max_rows)len=xpp::session().data_store.max_rows-1;
  for(i=0;i<len;i++){
    xpp::session().data_store.col[0][i]=xlo+i*dx;
    xpp::session().data_store.col[1][i]=y[i];
  }
  refresh_browser(len);
}

void new_lookup_com(int i)
{
  xpp::Session &s=xpp::session();
 int index,ok,status;
 double xlo,xhi;
 int npts;

  index=select_table();
  if(index==-1)return;
  if(i==1){
    view_table(index);
    return;
  }
   if(s.tables[index].flag==1){
     std::string file=s.tables[index].filename;
     status=file_selector("Load table",file,"*.tab");
     if(status==0)return;
     ok=load_table(file.c_str(),index);
     if(ok==1)s.tables[index].filename=file;

   }
   if(s.tables[index].flag==2){
     npts=s.tables[index].n;

     xlo=s.tables[index].xlo;
       xhi=s.tables[index].xhi;
       std::string newform=s.tables[index].filename;
       new_int("Auto-evaluate? (1/0)",&s.tables[index].autoeval);
       new_int("NPts: ",&npts);
       new_float("Xlo: ",&xlo);
       new_float("Xhi: ",&xhi);
       new_string_of("Formula :",newform,XPP_FIELD_EXPRESSION);
       create_fun_table(npts,xlo,xhi,newform.c_str(),index);

   }

}

double lookupxy(double x, int n, double *xv, double *yv)
{
  double dx,dy,x1,y1,x2,y2;
  int i;
  if(x<=xv[0])
    return(yv[0]+(yv[1]-yv[0])*(x-xv[0])/(xv[1]-xv[0]));
  if(x>=xv[n-1])
    return(yv[n-1]+(yv[n-2]-yv[n-1])*(x-xv[n-1])/(xv[n-1]-xv[n-2]));
  x1=xv[0];
  y1=yv[0];
  for(i=1;i<n;i++){
    if(x<=xv[i]){
      x2=xv[i];
      y2=yv[i];
      dx=x2-x1;
      dy=y2-y1;
      return(y1+dy*(x-x1)/dx);
    }
    x1=xv[i];
    y1=yv[i];
  }
  return(yv[n-1]);
}

double tab_interp(double xlo, double h, double x, double *y, int n, int i)
{
  double a,b,c,d;
  double ym,y0,y1,y2;
  double tt;
  ym=y[i-1];
  y0=y[i];
  y1=y[i+1];
  y2=y[i+2];
  d=y0;
  b=.5*(y1+ym-2*y0);
  a=(3*(y0-y1)+y2-ym)/6;
  c=(6*y1-y2-3*y0-2*ym)/6;
  tt=(x-xlo)/h-i;
  return d+tt*(c+tt*(b + tt*a));
}
double lookup(double x, int index)
{
  xpp::Session &s=xpp::session();
  double xlo=s.tables[index].xlo,xhi=s.tables[index].xhi,dx=s.tables[index].dx;
  double *y;
  double x1,y1,y2;
  int i1,i2,n=s.tables[index].n;
  y=s.tables[index].y;
 
  if(s.tables[index].flag==0)return(0.0); /* Not defined   */
   if(s.tables[index].xyvals==1)
    return(lookupxy(x,n,s.tables[index].x,y));
  
  i1=static_cast<int>((x-xlo)/dx);   /* (int)floor(x) instead of (int)x ??? */
  if(s.tables[index].interp==2&&i1>0&&i1<(n-2))
    return tab_interp(xlo,dx,x,y,n,i1); /* if it is on the edge - use linear */
  i2=i1+1;
    if(i1>-1&&i2<n){
    x1=dx*i1+xlo;
    y1=y[i1];
    y2=y[i2];
    if (s.tables[index].interp==0||s.tables[index].interp==2)
      return(y1+(y2-y1)*(x-x1)/dx);
    else
      {
	    return(y1);
	};
  }
  if(i1<0)return(y[0]+(y[1]-y[0])*(x-xlo)/dx);
  if(i2>=n)return(y[n-1]+(y[n-1]-y[n-2])*(x-xhi)/dx);

  return(0.0);
}

void init_table()
{
  xpp::Session &s=xpp::session();
  int i;
  for(i=0;i<MAX_TAB;i++) {
    s.tables[i].flag=0;
    s.tables[i].autoeval=1;
    s.tables[i].interp=0;
  }
}

void redo_all_fun_tables()
{
  xpp::Session &s=xpp::session();
  int i;
  for(i=0;i<s.ntable;i++){
    if(s.tables[i].flag==2&&s.tables[i].autoeval==1)
      eval_fun_table(s.tables[i].n,s.tables[i].xlo,
		     s.tables[i].xhi,s.tables[i].filename.c_str(),s.tables[i].y);
  }
  update_all_ffts();
}

int eval_fun_table(int n, double xlo, double xhi, const char *formula, double *y)
{
  int i;
  
  double dx;
  double oldt;
  int command[200],ncold=xpp::session().parser.ncon,nsym=xpp::session().parser.nsym;
  if(add_expr(formula,command,&i)){
    err_msg("Illegal formula...");
    xpp::session().parser.ncon=ncold;
    xpp::session().parser.nsym=nsym;
    return(0);
  }
  oldt=get_ivar(0);
  dx=(xhi-xlo)/(static_cast<double>(n-1));
  for(i=0;i<n;i++){
    set_ivar(0,dx*i+xlo);
    y[i]=evaluate(command);
  }
  set_ivar(0,oldt);
  xpp::session().parser.ncon=ncold;
  xpp::session().parser.nsym=nsym;
  return(1);
}

int create_fun_table(int npts, double xlo, double xhi, const char *formula, int index)
{
  xpp::Session &s=xpp::session();
  int length=npts;

   if(s.tables[index].flag==1){
    err_msg("Not a function table...");
    return(0);
  }
  if(xlo>xhi){
    err_msg("Xlo > Xhi ???");
    return(0);
  }
  if(npts<2){
    err_msg("Too few points...");
    return(0);
  }
  resize_values(index,length);
  s.tables[index].flag=2;
  if(eval_fun_table(npts,xlo,xhi,formula,s.tables[index].y)){
    s.tables[index].xlo=xlo;
    s.tables[index].xhi=xhi;
    s.tables[index].n=npts;
    s.tables[index].dx=(xhi-xlo)/(static_cast<double>(npts-1));
    s.tables[index].filename=formula;
    return(1);
  }
   return(0);
}

int load_table(const char *filename, int index)
{
  xpp::Session &s=xpp::session();
  int i;
  int length;
  double xlo,xhi;
  /* the name without its quotes, up to a closing one */
  std::string filename2;
  bool quoted=false;
  for(const char *p=filename;*p;p++){
    if(*p=='"'){
      if(quoted)break;
      quoted=true;
    }
    else
      filename2+=*p;
  }

  if(s.tables[index].flag==2){
    err_msg("Not a file table...");
    return(0);
  }

  xpp::LineReader reader(filename2.c_str());
  if(!reader){
    xpp_files_refresh_cur_dir();
    err_msg(xpp::format("File<{:.245}> not found in {:.245}",filename2,xpp_files_cur_dir()).c_str());
    return(0);
  }
  auto next_line=[&reader]() -> std::optional<std::string> {
    auto line=reader.next();
    if(!line) return std::nullopt;
    return std::string(*line);
  };

 s.tables[index].interp=0;
  auto line0=next_line();
  if(!line0){
    err_msg("Table file too short");
    return(0);
  }
  {
    const char *bob=line0->c_str();
    if (bob[0]=='i') /* closest step value */
      {
        s.tables[index].interp=1;
        bob++;  /* skip past initial "i" to length */
      };
    if (bob[0]=='s') /* cubic spline  */
      {
        s.tables[index].interp=2;
        bob++;  /* skip past initial "i" to length */
      };
    length=atoi(bob);
  }
  if(length<2){
    err_msg("Length too small");
    return(0);
  }
  auto line1=next_line();
  if(!line1){
    err_msg("Table file too short");
    return(0);
  }
  xlo=atof(line1->c_str());
  auto line2=next_line();
  if(!line2){
    err_msg("Table file too short");
    return(0);
  }
  xhi=atof(line2->c_str());
  if(xlo>=xhi){
    err_msg("xlo >= xhi ??? ");
    return(0);
  }
  bool fresh=(s.tables[index].flag==0);
  resize_values(index,length);
  for(i=0;i<length;i++){
    auto line=next_line();
    if(!line){
       err_msg("Table file too short");
       s.tables[index].y_storage=std::vector<double>();
       s.tables[index].y=NULL;
       s.tables[index].flag=0;
       return(0);
     }
     s.tables[index].y[i]=atof(line->c_str());
   }
  s.tables[index].xlo=xlo;
  s.tables[index].xhi=xhi;
  s.tables[index].n=length;
  s.tables[index].dx=(xhi-xlo)/(length-1);
  s.tables[index].flag=1;
  if(fresh) s.tables[index].filename=filename2;
  return(1);
}
   
int get_lookup_len(int i)
{
  return xpp::session().tables[i].n;
}

/*   network stuff  
     
table name <type> ... arguments ...
           conv   npts  weight variable_name klo khi end_cond
           sparse npts  variable filename
           
      name(0 ... npts-1)

conv:
        name(i) = sum(k=klo,khi) weight(k-klo)*variable(i+k) 
        with end_cond = zero means skip if off end
                      = periodic means wrap around
        
sparse:
       need a file with the structure:
       ncon i_1 w_1 ... i_ncon w_ncon
for npts lines
 name(i) = sum(j=1,ncon_i) w_j name(i_j)

*/

#include "menus.h"
int select_table(void)
{
 int j;
 char ch;
 std::string key;
 std::vector<std::string> names;
 std::vector<const char *> n;
 for(int i=0;i<xpp::session().ntable;i++){
   key+=static_cast<char>('a'+i);
   names.push_back(xpp::format("{}: {}",key[i],xpp::session().tables[i].name));
 }
 for(const std::string &s : names)n.push_back(s.c_str());
 {
   XppMenu m={"table","Table",0,NULL,NULL,NULL,-1};
   m.n=xpp::session().ntable; m.items=n.data(); m.keys=key.c_str(); m.hints=no_hint;
   ch=static_cast<char>(menu_choose(&m,0));
 }
 j=static_cast<int>(ch-'a');
 if(j<0||j>=xpp::session().ntable){
   err_msg("Not a valid table");
   return -1;
 }
 return j;
}
