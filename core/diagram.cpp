/* xpp_io.h first: it pulls in <optional>/<string_view>/<format>, which
   auto_f2c.h's own min/max macros (included transitively below, through
   auto_nox.h) break if they are already defined first. */
#include "xpp_io.h"
#include "diagram.h"
#include "xpp_mem.h"
#include "autevd.h"
#include "init_conds.h"
#include "ggets.h"

#include "my_svg.h"
#include "my_ps.h"
#include "my_ps.h"
#include "graphics.h"
#include "auto_nox.h"
#include <stdlib.h>
#include <stdio.h>
#include "autlim.h"
#include "load_eqn.h"
#include "browse.h"
#include "graf_par.h"
#define DALLOC(a) (double *)xpp_malloc((a)*sizeof(double))
namespace {
/* err_msg/file_selector (xpp_ui.h) take char * and do not write through
   it, the historical C dialog API shared far beyond this file; str()
   (grobs.cpp-precedented) casts a literal for one of these calls. */
char *str(const char *s) { return const_cast<char *>(s); }
} // namespace
extern int TypeOfCalc;
extern ROTCHK blrtn;

extern float **storage;
extern int storind;
#define PACK_AUTO 0
#define PACK_LBF 1
extern int AutoTwoParam;
extern int NODE;
extern int DiagFlag;
int NBifs=0;
extern int NAutoPar;
DIAGRAM *bifd;

void start_diagram(int n)
{
  NBifs=1;
  bifd=(DIAGRAM *)xpp_malloc(sizeof(DIAGRAM));
  bifd->prev=NULL;
  bifd->next=NULL;
  bifd->index=0;
  bifd->uhi=DALLOC(n);
  bifd->ulo=DALLOC(n);
  bifd->u0=DALLOC(n);
  bifd->ubar=DALLOC(n);
  bifd->evr=DALLOC(n);
  bifd->evi=DALLOC(n);
  bifd->norm=0;
  bifd->lab=0;
  bifd->from=0;
   
  DiagFlag=0;
}

int find_diagram(int irs, int n, int *index, int *ibr, int *ntot, int *itp, int *nfpar, double *a, double *uhi, double *ulo, double *u0, double *par, double *per, int *icp1, int *icp2, int *icp3, int *icp4)
{
  int i,found=0;
  DIAGRAM *d;
  d=bifd;

  while(d->next!=NULL){
    if(d->lab==irs){
      found=1;
      break;
    }
    d=d->next;
  }
  if(found){
    *ibr=d->ibr;
    *ntot=d->ntot;
    *index=d->index;
    *itp=d->itp;
    *nfpar=d->nfpar;
    *a=d->norm;
    par=d->par;
    *icp1=d->icp1;
    *icp2=d->icp2;
    *icp3=d->icp3;
    *icp4=d->icp4;
    *per=d->per;
    for(i=0;i<n;i++){
      u0[i]=d->u0[i];
      ulo[i]=d->ulo[i];
      uhi[i]=d->uhi[i];
    }
    return(1);
  }
  return(0);
}
    
void edit_start(int ibr, int ntot, int itp, int lab, int nfpar, double a, double *uhi, double *ulo, double *u0, double *ubar, double *par, double per, int n, int icp1, int icp2, int icp3, int icp4, double *evr, double *evi)
{
  edit_diagram(bifd,ibr,ntot,itp,lab,nfpar,a,uhi,ulo,u0,ubar,
	       par,per,n,icp1,icp2,icp3,icp4,AutoTwoParam,evr,evi,blrtn.torper);
}

void edit_diagram(DIAGRAM *d, int ibr, int ntot, int itp, int lab, int nfpar, double a, double *uhi, double *ulo, double *u0, double *ubar, double *par, double per, int n, int icp1, int icp2, int icp3, int icp4, int flag2, double *evr, double *evi, double tp)
{
  int i;
  d->calc=TypeOfCalc;
  d->ibr=ibr;
  d->ntot=ntot;
  d->itp=itp;
  d->lab=lab;
  d->nfpar=nfpar;
  d->norm=a;
  for(i=0;i<8;i++){
    d->par[i]=par[i];
    /*  printf("%d %g\n",i,par[i]); */
  }

  d->per=per;
 
  d->icp1=icp1;
  d->icp2=icp2;
  d->icp3=icp3;
  d->icp4=icp4;
  d->flag2=flag2;
  for(i=0;i<n;i++){
    d->ulo[i]=ulo[i];
    d->uhi[i]=uhi[i];
    d->ubar[i]=ubar[i];
    d->u0[i]=u0[i];
    d->evr[i]=evr[i];
    d->evi[i]=evi[i];
   }
  d->torper=tp;
}
  
void add_diagram(int ibr, int ntot, int itp, int lab, int nfpar, double a, double *uhi, double *ulo, double *u0, double *ubar, double *par, double per, int n, int icp1, int icp2, int icp3, int icp4, int flag2, double *evr, double *evi)
{
 DIAGRAM *d,*dnew;

 d=bifd;
 while(d->next != NULL){
   d=(d->next);
 }
 d->next=(DIAGRAM *)xpp_malloc(sizeof(DIAGRAM));
 dnew=d->next;
 dnew->next=NULL;
 dnew->prev=d;
 dnew->uhi=DALLOC(n);
 dnew->ulo=DALLOC(n);
 dnew->u0=DALLOC(n);
 dnew->ubar=DALLOC(n);
 dnew->evr=DALLOC(n);
 dnew->evi=DALLOC(n);
 dnew->index=NBifs;
 dnew->from=0;
 NBifs++;
 edit_diagram(dnew,ibr,ntot,itp,lab,nfpar,a,uhi,ulo,u0,ubar,par,per,n,
	      icp1,icp2,icp3,icp4,flag2,evr,evi,blrtn.torper);
 
}

DIAGRAM *last_diagram(void)
{
  DIAGRAM *d=bifd;
  while(d->next!=NULL)d=d->next;
  return d;
}

void set_last_diagram_from(int from)
{
  last_diagram()->from=from;
}

const DIAGRAM *diagram_of_label(int lab)
{
  if(lab<=0||DiagFlag==0)return NULL; /* DiagFlag 0: bifd holds no point yet */
  for(const DIAGRAM *d=bifd;d!=NULL;d=d->next)
    if(d->lab==lab)return d;
  return NULL;
}

int diagram_has(int index,int ibr,int ntot)
{
  DIAGRAM *d=bifd;
  while(d!=NULL&&d->index!=index)d=d->next;
  return d!=NULL&&d->ibr==ibr&&abs(d->ntot)==abs(ntot);
}

void kill_diagrams()
{
  DIAGRAM *d,*dnew;
  d=bifd;
  while(d->next != NULL){  /*  Move to the end of the tree  */
    d=d->next;
  }
  while(d->prev != NULL ){
   dnew=d->prev;
   d->next=NULL;
   d->prev=NULL;
   xpp_free(d->uhi);
   xpp_free(d->ulo);
   xpp_free(d->u0);
   xpp_free(d->ubar);
   xpp_free(d->evr);
   xpp_free(d->evi);
   xpp_free(d);
   d=dnew;
 }
/*  NBifs=1;
  bifd->prev=NULL;
  bifd->next=NULL;
  bifd->index=0;
  */
  xpp_free(bifd->uhi);
  xpp_free(bifd->ulo);
  xpp_free(bifd->u0);
  xpp_free(bifd->ubar);
  xpp_free(bifd->evr);
  xpp_free(bifd->evi);
  xpp_free(bifd);
  start_diagram(NODE);
}

void redraw_diagram()
{
  DIAGRAM *d;
  int type,flag=0;
  draw_bif_axes();
  d=bifd;
  if(d->next==NULL)return;
  while(1){
    type=get_bif_type(d->ibr,d->ntot,d->lab);
 
    if(d->ntot==1)flag=0;
    else flag=1;
    auto_point_id(d->ibr,d->ntot,d->itp,d->index,d->from);
    add_point(d->par,d->per,d->uhi,d->ulo,d->ubar,d->norm,type,flag,
	      d->lab,d->nfpar,d->icp1,d->icp2,d->icp3,d->icp4,d->flag2,d->evr,d->evi);
    d=d->next;
    if(d==NULL)break;
  }
}

void write_info_out()
{
  /*char filename[256];*/
  char filename[XPP_MAX_NAME];
  DIAGRAM *d;
  int type,i;
  /*int flag=0
  */
  int status;
  int icp1,icp2;
  double *par;
  double par1,par2=0,*uhigh,*ulow,per;
  /*double a,*ubar,*u0;*/
  FILE *fp;
  XppWriter *w;
  XPP_SPRINTF(filename,"allinfo.dat");
  /* status=get_dialog("Write all info","Filename",filename,"Ok","Cancel",60);
   */
  status=file_selector(str("Write all info"),filename,str("*.dat"));

  if(status==0)return;
  d=bifd;
  if(d->next==NULL)return; /* nothing recorded: leave any existing file alone */
  w=xpp_writer_open(filename);
  if(w==NULL){
    err_msg(str("Can't open file"));
    return;
  }
  fp=xpp_writer_file(w);
 while(1){
    type=get_bif_type(d->ibr,d->ntot,d->lab);
    
    /*if(d->ntot==1)flag=0;
    else flag=1;
    */
    icp1=d->icp1;
    icp2=d->icp2;
    par=d->par;
    per=d->per;
    uhigh=d->uhi;
    ulow=d->ulo;
    /*ubar=d->ubar; Not used*/
   /* u0=d->u0; Not used*/
    /* a=d->norm; Not used*/
    par1=par[icp1];
    if(icp2<NAutoPar)
      par2=par[icp2];
    else 
      par2=par1;
     
    fprintf(fp,"%d %d %d %g %g %g ",
	    type,d->ibr,d->flag2,par1,par2,per);
    for(i=0;i<NODE;i++)
      fprintf(fp,"%g ",uhigh[i]);
    for(i=0;i<NODE;i++)
      fprintf(fp,"%g ",ulow[i]);
    for(i=0;i<NODE;i++)
      fprintf(fp,"%g %g ",d->evr[i],d->evi[i]);
    fprintf(fp,"\n");
    d=d->next;
    if(d==NULL)break;
  }
  xpp_writer_commit(w);

}

extern "C" void load_browser_with_branch(int ibr,int pts,int pte)
{
   DIAGRAM *d;
   int i,j,pt;
  /*int flag=0;
  */
  int icp1;
  double *par;
  double par1,*u0;
  int first,last,nrows;
  first=abs(pts);
  last=abs(pte);
  if(last<first){ /* reorder the points so that we will store in right range*/
    i=first;
    first=last;
    last=i;
  }
  nrows=last-first+1;
   d=bifd;
  if(d->next==NULL)return;
  j=0;
 while(1){
    pt=abs(d->ntot);
    if((d->ibr==ibr) && (pt>=first) && (pt<=last)){
      icp1=d->icp1;
      par=d->par;
      u0=d->u0;

      par1=par[icp1];
      storage[0][j]=par1;
      for(i=0;i<NODE;i++)
	storage[i+1][j]=u0[i];
      j++;
    }
    d=d->next;
    if(d==NULL)break;
        
 }
 storind=nrows;
 refresh_browser(nrows);
}
void write_init_data_file()
{
  /*char filename[256];*/
  char filename[XPP_MAX_NAME];
  DIAGRAM *d;
  int i;
  /*int flag=0;
  */
  int status;
  int icp1;
  double *par;
  double par1,*u0;
  /*double a,*uhigh,*ulow,*ubar;*/
  FILE *fp;
  XppWriter *w;
  XPP_SPRINTF(filename,"initdata.dat");
  /* status=get_dialog("Write all info","Filename",filename,"Ok","Cancel",60);
   */
  status=file_selector(str("Write init data file"),filename,str("*.dat"));

  if(status==0)return;
  d=bifd;
  if(d->next==NULL)return; /* nothing recorded: leave any existing file alone */
  w=xpp_writer_open(filename);
  if(w==NULL){
    err_msg(str("Can't open file"));
    return;
  }
  fp=xpp_writer_file(w);
 while(1){
    /*if(d->ntot==1)flag=0;
    else flag=1;
    Unused here?
    */
    icp1=d->icp1;
    par=d->par;
    /*
    uhigh=d->uhi;
    ulow=d->ulo;
    ubar=d->ubar;
    Unused here??
    */
    u0=d->u0;

    /*
    a=d->norm;

    Unused here??
    */
    par1=par[icp1];

    /* fprintf(fp,"%d %d %g %g %g ",
       type,d->ibr,par1,par2,per); */
    fprintf(fp,"%g ",par1);
    for(i=0;i<NODE;i++)
      fprintf(fp,"%g ",u0[i]);
    fprintf(fp,"\n");
    d=d->next;
    if(d==NULL)break;
  }
  xpp_writer_commit(w);

}


void write_pts()
{
  /*char filename[256];*/
  char filename[XPP_MAX_NAME];
  DIAGRAM *d;
  int type;
  /*int flag=0;
  */
  int status;
  int icp1,icp2;
  double *par;
  double x,y1,y2,par1,par2=0,a,*uhigh,*ulow,*ubar,per;
  FILE *fp;
  XppWriter *w;
  XPP_SPRINTF(filename,"diagram.dat");
  status=file_selector(str("Write points"),filename,str("*.dat"));
  /* get_dialog("Write points","Filename",filename,"Ok","Cancel",60); */
  if(status==0)return;
  d=bifd;
  if(d->next==NULL)return; /* nothing recorded: leave any existing file alone */
  w=xpp_writer_open(filename);
  if(w==NULL){
    err_msg(str("Can't open file"));
    return;
  }
  fp=xpp_writer_file(w);
  while(1){
    type=get_bif_type(d->ibr,d->ntot,d->lab);
    
    /*if(d->ntot==1)flag=0;
    else flag=1;
    
    Unused here??
    */
    icp1=d->icp1;
    icp2=d->icp2;
    par=d->par;
    per=d->per;
    uhigh=d->uhi;
    ulow=d->ulo;
    ubar=d->ubar;
    a=d->norm;
    par1=par[icp1];
    if(icp2<NAutoPar)
      par2=par[icp2];

    /* now we have to check is the diagram parameters correspond to the 
       current view 
    */
    if(check_plot_type(d->flag2,icp1,icp2)==1){
      auto_xy_plot(&x,&y1,&y2,par1,par2,per,uhigh,ulow,ubar,a); 
      fprintf(fp,"%g %g %g %d %d %d\n",
	      x,y1,y2,type,abs(d->ibr),d->flag2);
    }
      d=d->next;
      if(d==NULL)break;
  }
  xpp_writer_commit(w);
}

void post_auto()
{
  /*char filename[256];*/
  char filename[XPP_MAX_NAME];
  DIAGRAM *d;
  int type,flag=0;
  int status;
  XPP_SPRINTF(filename,"auto.ps");
  /* status=get_dialog("Postscript","Filename",filename,"Ok","Cancel",60); */
  status=file_selector(str("Postscript"),filename,str("*.ps"));
  if(status==0)return;
  if(!ps_init(filename,plot_export.color))
    return;
   draw_ps_axes();
  d=bifd;
  if(d->next==NULL)return;
  while(1){
    type=get_bif_type(d->ibr,d->ntot,d->lab);
    if (type < 0)
    {	
    	plintf("Unable to get bifurcation type.\n");
    }
    if(d->ntot==1)flag=0;
    else flag=1;
    add_ps_point(d->par,d->per,d->uhi,d->ulo,d->ubar,d->norm,type,flag,
	      d->lab,d->nfpar,d->icp1,d->icp2,d->flag2,d->evr,d->evi);
    d=d->next;
    if(d==NULL)break;
  }
  ps_end();
  set_normal_scale();
}


void svg_auto()
{
  /*char filename[256];*/
  char filename[XPP_MAX_NAME];
  DIAGRAM *d;
  int type,flag=0;
  int status;
  XPP_SPRINTF(filename,"auto.svg");
  /* status=get_dialog("Postscript","Filename",filename,"Ok","Cancel",60); */
  status=file_selector(str("SVG"),filename,str("*.svg"));
  if(status==0)return;
  if(!svg_init(filename,plot_export.color))
    return;
   draw_svg_axes();
  d=bifd;
  if(d->next==NULL)return;
  while(1){
    type=get_bif_type(d->ibr,d->ntot,d->lab);
    if (type < 0)
    {	
    	plintf("Unable to get bifurcation type.\n");
    }
    if(d->ntot==1)flag=0;
    else flag=1;
    add_ps_point(d->par,d->per,d->uhi,d->ulo,d->ubar,d->norm,type,flag,
	      d->lab,d->nfpar,d->icp1,d->icp2,d->flag2,d->evr,d->evi);
    d=d->next;
    if(d==NULL)break;
  }
  svg_end();
  
  set_normal_scale();
}




void bound_diagram(double *xlo, double *xhi, double *ylo, double *yhi)
{
  DIAGRAM *d;
  int type;
  
  /*int flag=0;
  */
  double x,y1,y2,par1,par2=0.0;
  d=bifd;
  if(d->next==NULL)return;
  *xlo=1.e16;
  *ylo=*xlo;
  *xhi=-*xlo;
  *yhi=-*ylo;
  while(1){
    type=get_bif_type(d->ibr,d->ntot,d->lab);
    if (type <1)
    {
        plintf("Unable to get bifurcation type.\n");
    }
    /*if(d->ntot==1)flag=0;
    else flag=1;
    Unused here?
    */
    par1=d->par[d->icp1];
    if(d->icp2<NAutoPar)par2=d->par[d->icp2];
    auto_xy_plot(&x,&y1,&y2,par1,par2,d->per,d->uhi,d->ulo,d->ubar,d->norm);
    if(x<*xlo)*xlo=x;
    if(x>*xhi)*xhi=x;
    if(y2<*ylo)*ylo=y2;
    if(y1>*yhi)*yhi=y1;
    d=d->next;
    if(d==NULL)break;
  }
}



int save_diagram(FILE *fp, int n)
{
  int i;
  DIAGRAM *d;
  fprintf(fp,"%d\n",NBifs-1);
  if(NBifs==1)
    return(-1);
  d=bifd;
  while(1){
    fprintf(fp,"%d %d %d %d %d %d %d %d %d %d %d %d\n", 
	    d->calc,d->ibr,d->ntot,d->itp,d->lab,d->index,d->nfpar,
	    d->icp1,d->icp2,d->icp3,d->icp4,d->flag2);
    for(i=0;i<8;i++)fprintf(fp,"%g ",d->par[i]);
    fprintf(fp,"%g %g \n",d->norm,d->per);
    
    for(i=0;i<n;i++)fprintf(fp,"%f %f %f %f %f %f\n",d->u0[i],d->uhi[i],d->ulo[i],
			    d->ubar[i],d->evr[i],d->evi[i]);
    d=d->next;
    if(d==NULL)break;
  }
  return(1);
}
 




 
int load_diagram(FILE *fp, int node)
{
  double u0[NAUTO],uhi[NAUTO],ulo[NAUTO],ubar[NAUTO],evr[NAUTO],evi[NAUTO],norm,par[8],per;
  int i,flag=0;
  int n;
  int calc,ibr,ntot,itp,lab,index,nfpar,icp1,icp2,icp3,icp4,flag2;
  XppTokenReader *tr=xpp_token_reader_attach(fp);
  if (xpp_token_reader_int(tr,&n) != 1) { xpp_token_reader_close(tr); return -1; }
  if(n==0){
/*    start_diagram(NODE); */
    xpp_token_reader_close(tr);
    return(-1);
  }

  while(1){
    if (xpp_token_reader_int(tr,&calc) != 1 || xpp_token_reader_int(tr,&ibr) != 1
	|| xpp_token_reader_int(tr,&ntot) != 1 || xpp_token_reader_int(tr,&itp) != 1
	|| xpp_token_reader_int(tr,&lab) != 1 || xpp_token_reader_int(tr,&index) != 1
	|| xpp_token_reader_int(tr,&nfpar) != 1 || xpp_token_reader_int(tr,&icp1) != 1
	|| xpp_token_reader_int(tr,&icp2) != 1 || xpp_token_reader_int(tr,&icp3) != 1
	|| xpp_token_reader_int(tr,&icp4) != 1 || xpp_token_reader_int(tr,&flag2) != 1) break;
    for(i=0;i<8;i++) if (xpp_token_reader_double(tr,&par[i]) != 1) break;
    if (i<8) break;
    if (xpp_token_reader_double(tr,&norm) != 1 || xpp_token_reader_double(tr,&per) != 1) break;
    for(i=0;i<node;i++) if (xpp_token_reader_double(tr,&u0[i]) != 1 || xpp_token_reader_double(tr,&uhi[i]) != 1
			      || xpp_token_reader_double(tr,&ulo[i]) != 1 || xpp_token_reader_double(tr,&ubar[i]) != 1
			      || xpp_token_reader_double(tr,&evr[i]) != 1 || xpp_token_reader_double(tr,&evi[i]) != 1) break;
    if (i<node) break;
    if(flag==0){
      edit_start(ibr,ntot,itp,lab,nfpar,norm,uhi,ulo,u0,ubar,par,per,node,
		 icp1,icp2,icp3,icp4,evr,evi);
      flag=1;
      DiagFlag=1;
    }
    else
      add_diagram(ibr,ntot,itp,lab,nfpar,norm,uhi,ulo,u0,ubar,par,per,node,
		  icp1,icp2,icp3,icp4,flag2,evr,evi);
    if(index>=n)break;
  }
    xpp_token_reader_close(tr);
    return(1);

}
  










