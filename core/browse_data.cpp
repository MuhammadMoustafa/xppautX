/* The data side of the browser: the one BROWSER instance, its storage
   pointer, row/column bookkeeping and the file writer. No X11 here; the
   widget code that displays it stays in browse.c. */
#include <stdlib.h>
#include "xpp_mem.h"
#include "parserslow.h"
#include "browse.h"
#include "xpp_ui.h"
#include "xpp_globals.h"
#include "integrate.h"
#include "grobs.h"
#include <math.h>
#include <ctype.h>
#include "xpplim.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include "xpp_io.h"

namespace {
/* err_msg/TwoChoice/respond_box/file_selector (xpp_ui.h) take char * and
   do not write through it, the historical C dialog API shared far beyond
   this file; str() (grobs.cpp-precedented) casts a literal for one of
   these calls. */
char *str(const char *s) { return const_cast<char *>(s); }
} // namespace

extern int *plotlist, N_plist;
extern int NEQ;
extern float **storage;

extern "C" int find_user_name(int type, char *oname); /* init_conds.c (pure) */

/*  The one and only primitive data browser   */
BROWSER my_browser;
float *old_rep;
int REPLACE=0,R_COL=0;
extern int NODE,NMarkov,FIX_VAR;
extern char uvar_names[MAXODE][XPP_NAME_MAX+1];
extern double last_ic[MAXODE];

float **get_browser_data()
{
  return my_browser.data;
}

void set_browser_data(float **data,int col0)
{
  my_browser.data=data;
  my_browser.col0=col0;
}

float *get_data_col(int c)
{
  return my_browser.data[c];
}

void waitasec(int msec)
{
  struct timeval tim;
  /*struct timezone tz;*/
  double sec=(double)msec/1000;
  double t1,t2;
  gettimeofday(&tim,NULL);
  t1=tim.tv_sec+(tim.tv_usec/1000000.0);

   while(1)
    {
       gettimeofday(&tim,NULL);
       t2=tim.tv_sec+(tim.tv_usec/1000000.0);


       if((t2-t1)>sec)
	
       return;
    }
}

int get_maxrow_browser()
{
  return my_browser.maxrow;
}

void write_mybrowser_data(FILE *fp)
{
  write_browser_data(fp,&my_browser);
}

void write_browser_data(FILE *fp, BROWSER *b)
{
  int i,j,l;
  
  for(i=b->istart;i<b->iend;i++){
    if(N_plist>0){
      for(l=0;l<N_plist;l++){
	j=plotlist[l];
	fprintf(fp,"%.8g ",b->data[j][i]);
      }
    }
    else {
	for(j=0;j<b->maxcol;j++)fprintf(fp,"%.8g ",b->data[j][i]);
    }
    fprintf(fp,"\n");
  }
 
}

void find_variable(char *s, int *col)
{
 *col=-1;
  if(strcasecmp("T",s)==0){
   *col=0;
    return;
   }
  *col=find_user_name(2,s);
  if(*col>-1)*col=*col+1; 
 } 

void  refresh_browser(int length)
{
 my_browser.dataflag=1;
 my_browser.maxrow=length;
 my_browser.iend=length;
 xpp_ui.data_changed(length);
}

void reset_browser()
{
  my_browser.maxrow=0;
  my_browser.dataflag=0;
}

void init_browser()
{
 
 my_browser.dataflag=0;
 my_browser.data=storage;
 my_browser.maxcol=NEQ+1;
 my_browser.maxrow=0;
 my_browser.col0=1;
 my_browser.row0=0;
 my_browser.istart=0;
 my_browser.iend=0;
 XPP_STRCPY(my_browser.hinttxt,"hint");

}

int may_write_file(const char *fil)
{
 FILE *fp=fopen(fil,"r");
 if(fp==NULL)return 1;
 fclose(fp);
 return (char)TwoChoice(str("Yes"),str("No"),
		str("File Exists! Overwrite?"),str("yn"))=='y';
}

void open_write_file(FILE **fp, char *fil, int *ok)
{
 *ok=0;
 *fp=NULL;
 if(!may_write_file(fil))return;

			*fp=fopen(fil,"w");
			if(*fp==NULL){
				      err_msg(str("Cannot open file"));
				      *ok=0;
				     }
		         else *ok=1;
			 return;

  }

void  wipe_rep()
 {
    if(!REPLACE)return;
    xpp_free(old_rep);
    REPLACE=0;
  }

void data_get(BROWSER *b)
{
 int i,in=b->row0;
 set_ivar(0,(double)storage[0][in]);
 for(i=0;i<NODE;i++)
 {
  last_ic[i]=(double)storage[i+1][in];
  set_ivar(i+1,last_ic[i]);
 } 
 for(i=0;i<NMarkov;i++){
   last_ic[i+NODE]=(double)storage[i+NODE+1][in];
   set_ivar(i+1+NODE+FIX_VAR,last_ic[i+NODE]);
 }
 for(i=NODE+NMarkov;i<NEQ;i++)
   set_val(uvar_names[i],storage[i+1][in]);
 

 redraw_ics();
}

extern "C" void data_get_mybrowser(int row)
{
  my_browser.row0=row;
  data_get(&my_browser);
}

void get_data_xyz(float *x, float *y, float *z, int i1, int i2, int i3, int off)
{
  int in=my_browser.row0+off;
  *x=my_browser.data[i1][in];
  *y=my_browser.data[i2][in];
  *z=my_browser.data[i3][in];
}

/* ---- the browser's commands (were in browse.c); the widget calls them ---- */

extern int *my_ode[];
extern char *ode_names[MAXODE];
extern int MAXSTOR,NEQ_MIN,NJMP,storind;
extern int NSYM,NSYM_START,NCON,NCON_START;
extern double DELTA_T;

int check_for_stor(float **data)
{
 if(data!=storage){
   err_msg(str("Only data can be in browser"));
   return(0);
 }
   else return(1);

}

void data_del_col(BROWSER *b)  /*  this only works with storage  */
{
    if(check_for_stor(b->data)==0)return;
  err_msg(str("Sorry - not working very well yet..."));
}

void data_add_col(BROWSER *b)
{
  int status;
  char var[XPP_NAME_MAX+1],form[80];
   if(check_for_stor(b->data)==0)return;
  XPP_STRCPY(var,"");
  XPP_STRCPY(form,"");
  status=get_dialog(str("Add Column"),str("Name"),var,str("Ok"),str("Cancel"),XPP_NAME_MAX);
  if(status!=0){
    status=get_dialog_of(str("Add Column"),str("Formula:"),form,str("Add it"),str("Cancel"),80,XPP_FIELD_EXPRESSION);
     if(status!=0)
      add_stor_col(var,form,b);
  }
}

int add_stor_col(char *name, char *formula, BROWSER *b)
{
  int com[4000],i,j;

  if(strlen(name)>XPP_NAME_MAX){
    err_msg(str("Name too long"));
    return(0);
  }
  if(add_expr(formula,com,&i)){
    err_msg(str("Bad Formula .... "));
    return(0);
  }
  if((my_ode[NEQ+FIX_VAR]=(int *)xpp_malloc((i+2)*sizeof(int)))==NULL){
     err_msg(str("Cant allocate formula space"));
     return(0);
   }
  if((storage[NEQ+1]=(float *)xpp_malloc(MAXSTOR * sizeof(float)))==NULL){
    err_msg(str("Cant allocate space ...."));
    xpp_free(my_ode[NEQ]);
    return(0);
  }
  if((ode_names[NEQ]=(char *)xpp_malloc(80))==NULL){
    err_msg(str("Cannot allocate space ..."));
    xpp_free(my_ode[NEQ]);
    xpp_free(storage[NEQ+1]);
    return(0);
  }
  /* ode_names[NEQ] is a pointer, allocated 80 bytes just above. */
  xpp_strlcpy(ode_names[NEQ],formula,80);
  strupr(ode_names[NEQ]);
  for(j=0;j<=i;j++)
    my_ode[NEQ+FIX_VAR][j]=com[j];
  XPP_STRCPY(uvar_names[NEQ],name);
  strupr(uvar_names[NEQ]);
  for(i=0;i<b->maxrow;i++)
    storage[NEQ+1][i]=0.0;   /*  zero it all   */
  for(i=0;i<b->maxrow;i++){
    for(j=0;j<NODE+1;j++)set_ivar(j,(double)storage[j][i]);
    for(j=NODE;j<NEQ;j++)set_val(uvar_names[j],(double)storage[j+1][i]); 
    storage[NEQ+1][i]=(float)evaluate(com);
  }
  add_var(uvar_names[NEQ],0.0);  /*  this could be trouble .... */
  NEQ++;
  b->maxcol=NEQ+1;
  xpp_ui.browser_redraw(1);  
  return(1);
}

void chk_seq(char *f,int *seq, double *a1, double *a2)
{
  int i,j=-1;
  char n1[256],n2[256];
  int n=strlen(f);
  *seq=0;
  *a1=0.0;
  *a2=0.0;
  for(i=0;i<n;i++)
    {
      if(f[i]==':'){
	*seq=1;
	j=i;
      }
      
      if(f[i]==';'){
	*seq=2;
	j=i;
      }
    }
  if(j>-1){
    for(i=0;i<j;i++)
      n1[i]=f[i];
    n1[j]=0;
    for(i=j+1;i<n;i++)
      n2[i-j-1]=f[i];
    n2[n-j-1]=0;
    *a1=atof(n1);
    *a2=atof(n2);
  }
  /*      plintf("seq=%d a1=%g a2=%g\n",*seq,*a1,*a2); */
}

void replace_column(char *var, char *form, float **dat, int n)
{
 int com[200],i,j;
 int intflag=0;
 int dif_var=-1;
 int seq=0;
 double a1,a2,da=0.0;
 float old=0.0,dt,derv=0.0;
 float sum=0.0;
 if(n<2)return;

 dt=NJMP*DELTA_T;
/* first check for derivative or integral symbol */
i=0;
while(i<(int)strlen(form)){
  if(!isspace(form[i]))break;
  i++;
  }
 if(form[i]=='&'){ intflag=1; form[i]=' ';}
 if(form[i]=='@'){
   form[i]=' ';
   find_variable(form,&dif_var);
   if(dif_var<0){
     err_msg(str("No such variable"));
     return;
   }

 }

if(dif_var<0)
  chk_seq(form,&seq,&a1,&a2);
 if(seq==1){
   if(a1==a2)
     seq=3;
   else
     da=(a2-a1)/((double)(n-1));
 }
 if(seq==2)
   da=a2;
 if(seq==3){
   err_msg(str("Illegal sequence"));
   return;
 }


/*  first compile formula ... */


 if(dif_var<0&&seq==0){
   if(add_expr(form,com,&i)){
     NCON=NCON_START;
     NSYM=NSYM_START;
     err_msg(str("Illegal formula..."));
     return;
   }
 }
/* next check to see if column is known ... */

 find_variable(var,&i);
 if(i<0){
   err_msg(str("No such column..."));
   NCON=NCON_START;
   NSYM=NSYM_START;
   return;
 }
 R_COL=i;

 /* Okay the formula is cool so lets allocate and replace  */

 wipe_rep();
 old_rep=(float *)xpp_malloc(sizeof(float)*n);
 REPLACE=1;
 for(i=0;i<n;i++)
 {
   old_rep[i]=dat[R_COL][i];
   if(dif_var<0)
     {
       if(seq==0)
	 {
	   for(j=0;j<NODE+1;j++)set_ivar(j,(double)dat[j][i]);
	   for(j=NODE;j<NEQ;j++)set_val(uvar_names[j],(double)dat[j+1][i]);
	   if(intflag)
	     {
	       sum+=(float)evaluate(com);
	       dat[R_COL][i]=sum*dt;
	     }
	   else 
	     dat[R_COL][i]=(float)evaluate(com);
	 }
       else 
	 {
	   dat[R_COL][i]=(float)(a1+i*da);
	 }
     }
   else 
     {
       if(i==0)derv=(dat[dif_var][1]-dat[dif_var][0])/dt;
       if(i==(n-1))derv=(dat[dif_var][i]-old)/dt;
       /* if(i>0&&i<(n-1))derv=(dat[dif_var][i+1]-old)/(2*dt); */
       if(i>0&&i<(n-1))derv=(dat[dif_var][i+1]-dat[dif_var][i])/dt;
       old=dat[dif_var][i];
       dat[R_COL][i]=derv;
     }
 }
 NCON=NCON_START;
 NSYM=NSYM_START;

}

void unreplace_column()


{
 int i,n=my_browser.maxrow;
 if(!REPLACE)return;
 for(i=0;i<n;i++)my_browser.data[R_COL][i]=old_rep[i];
 wipe_rep();
 
 }

void make_d_table(double xlo, double xhi, int col, char *filename, BROWSER b)
{
  int i,npts,ok;
  FILE *fp;
  open_write_file(&fp,filename,&ok);
  if(!ok)return;
    npts=b.iend-b.istart;
 

  fprintf(fp,"%d\n",npts);
  fprintf(fp,"%g\n%g\n",xlo,xhi);
  for(i=0;i<npts;i++)
    fprintf(fp,"%10.10g\n",b.data[col][i+b.istart]);
  fclose(fp);
  ping();
}

void find_value(int col, double val, int *row, BROWSER b)
{
 int n=b.maxrow;
 int i;
 int ihot=0;
 float err,errm;
 errm=(float)fabs(b.data[col][0]-val);
 for(i=b.row0;i<n;i++){
 err=(float)fabs(b.data[col][i]-val);
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
 char var[XPP_NAME_MAX+1],form[80];
XPP_STRCPY(var,uvar_names[0]);
XPP_STRCPY(form,uvar_names[0]);
status=get_dialog_of(str("Replace"),str("Variable:"),var,str("Ok"),str("Cancel"),XPP_NAME_MAX,XPP_FIELD_NAME_IN(0));
if(status!=0){
 status=get_dialog_of(str("Replace"),str("Formula:"),form,str("Replace"),str("Cancel"),80,XPP_FIELD_EXPRESSION);
 if(status!=0)replace_column(var,form,b->data,b->maxrow);
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

 static char *name[]={str("Variable"),str("Xlo"),str("Xhi"),str("File")};
 char value[4][MAX_LEN_SBOX];

 double xlo=0,xhi=1;
 int col;
 XPP_SPRINTF(value[0],"%s",uvar_names[0]);
 XPP_SPRINTF(value[1],"0.00");
 XPP_SPRINTF(value[2],"1.00");
 snprintf(value[3],sizeof(value[3]),"%.*s.tab",XPP_NAME_MAX,value[0]);
 static const int kinds[]={XPP_FIELD_NAME_IN(0),XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,XPP_FIELD_FILE};
 status=do_string_box_of(4,4,1,str("Tabulate"),name,value,40,kinds);
 if(status==0)return;
 xlo=atof(value[1]);
 xhi=atof(value[2]);
 find_variable(value[0],&col);
  if(col>=0)
   make_d_table(xlo,xhi,col,value[3],*b);
}

void data_find(BROWSER *b)
{
 int status;

 static char *name[]={str("*0Variable"),str("Value")};
 char value[2][MAX_LEN_SBOX];
 int col,row=-1;

 double val;

 XPP_SPRINTF(value[0],"%s",uvar_names[0]);
 XPP_SPRINTF(value[1],"0.00");
 static const int kinds[]={XPP_FIELD_TEXT,XPP_FIELD_NUMBER};
 status=do_string_box_of(2,2,1,str("Find Data"),name,value,40,kinds);
 
  

 if(status==0)return;
 val=atof(value[1]);
 find_variable(value[0],&col);
 if(col>=0)find_value(col,val,&row,*b);
 if(row>=0){
	    b->row0=row;
	    xpp_ui.browser_redraw(0);
	   }

   
  
}

void data_read(BROWSER *b)
{

 int status;
 char fil[256];
 FILE *fp;
 int k;
 int len,count=0;
 float z;

 XPP_STRCPY(fil,"test.dat");
 /*  XGetInputFocus(display,&w,&rev);
 status=get_dialog("Load","Filename:",fil,"Ok","Cancel",40);
 */
 status=file_selector(str("Load data"),fil,str("*.dat"));
if(status==0)return;
 fp=fopen(fil,"r");
 	if(fp==NULL){
				      respond_box(str("Ok"),
					str("Cannot open file"));
				     return;
				     }
 /*  Now we establish the width of the file (the whitespace-separated
     fields of its first line -- xpp_line_reader reads that line whole,
     however long, instead of the raw fscanf "%c" char-at-a-time loop
     this replaced) and read it.
      If there are more columns than available we
      ignore them.

     if there are fewer rows we read whats necessary
     if there are more rows then read until we
     are done or MAX_STOR_ROW.
     This data can be plotted etc like anything else
    */
 {
   xpp::LineReader lr = xpp::LineReader::attach(fp);
   std::optional<std::string_view> line = lr.next();
   if(line){
     int white=1;
     for(unsigned char c : *line){
       if(!isspace(c)&&white){white=0;++count;}
       if(isspace(c)&&!white)white=1;
     }
   }
 }
 rewind(fp);
 len=0;
 {
   XppTokenReader *tr=xpp_token_reader_attach(fp);
   for(;;)
   {
    int gotrow=1;
    for(k=0;k<count;k++)
    {
     if(xpp_token_reader_float(tr,&z)!=1){gotrow=0;break;}
     if(k<b->maxcol)b->data[k][len]=z;
     }
     if(!gotrow)break;
     ++len;
     if(len>=MAXSTOR)break;
    }
   xpp_token_reader_close(tr);
  }
  fclose(fp);
  refresh_browser(len);
  storind=len;
 /*  b->maxrow=len;
 xpp_ui.browser_redraw(0); */
}

void data_write(BROWSER *b)
{

 int status;
 char fil[256];
 FILE *fp;
 int i,j;
 int ok;

 XPP_STRCPY(fil,"test.dat");

/*
 XSetInputFocus(display,command_pop,RevertToParent,CurrentTime);
 strcpy(fil,"test.dat");
 new_string("Write to:",fil);
*/
 /* status=get_dialog("Write","Filename:",fil,"Ok","Cancel",40);

    XSetInputFocus(display,w,rev,CurrentTime); */
  status=file_selector(str("Write data"),fil,str("*.dat"));
if(status==0)return;
 open_write_file(&fp,fil,&ok);
 if(!ok)return;
 for(i=b->istart;i<b->iend;i++){
	for(j=0;j<b->maxcol;j++)fprintf(fp,"%.8g ",b->data[j][i]);
 	fprintf(fp,"\n");
        }
 fclose(fp);
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

