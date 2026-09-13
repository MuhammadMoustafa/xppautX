/* The data side of the browser: the one BROWSER instance, its storage
   pointer, row/column bookkeeping and the file writer. No X11 here; the
   widget code that displays it stays in browse.c. */
#include <stdlib.h>
#include "parserslow.h"
#include "browse.h"
#include "xpp_ui.h"
#include "xpplim.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>

extern int *plotlist, N_plist;
extern int NEQ;
extern float **storage;

int find_user_name(int type, char *oname); /* init_conds.c (pure) */

/*  The one and only primitive data browser   */
BROWSER my_browser;
float *old_rep;
int REPLACE=0,R_COL=0;
extern int NODE,NMarkov,FIX_VAR;
extern char uvar_names[MAXODE][12];
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

int gettimenow()
{
  struct timeval now;
  /*struct timezone tz;
  gettimeofday(&now,&tz);
  */
  gettimeofday(&now,NULL);
  return now.tv_usec;
} 

void waitasec(msec)
     int msec;
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

void write_browser_data(fp,b)
     FILE *fp;
     BROWSER *b;
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

void find_variable(s,col)
char *s;
int *col;
{
 *col=-1;
  if(strcasecmp("T",s)==0){
   *col=0;
    return;
   }
  *col=find_user_name(2,s);
  if(*col>-1)*col=*col+1; 
 } 

void  refresh_browser(length)
 int length;
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
 strcpy(my_browser.hinttxt,"hint");

}

void open_write_file(fp,fil,ok)
 FILE **fp;
  char *fil;
  int *ok;
{
 char ans;
 *ok=0;
 *fp=fopen(fil,"r");
	if(*fp!=NULL){
		fclose(*fp); 
		ans=(char)TwoChoice("Yes","No",
		"File Exists! Overwrite?","yn");
		if(ans!='y')return;
		}	 

			*fp=fopen(fil,"w");
			if(*fp==NULL){
				      err_msg("Cannot open file");
				      *ok=0;
				     }
		         else *ok=1;
			 return;
		    
  }

void  wipe_rep()
 {
    if(!REPLACE)return;
    free(old_rep);
    REPLACE=0;
  }

void data_get(b)
BROWSER *b;
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

void data_get_mybrowser(int row)
{
  my_browser.row0=row;
  data_get(&my_browser);
}
