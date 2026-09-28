#include "histogram.h"
#include "session.h"
#include "model.h"
#include "storage.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <array>
#include <vector>

#include "adj2.h"
#include "browse.h"

#include "expr.h"
#include "xpp_log.h"
#include "xpp_math.h"
#include "xpp_ui.h"
#include "form_ode.h"
#include "load_eqn.h"

static int spec_type=0;
/* type =0 for PSD
   type =1 for crossspectrum
   type =2 for coherence

*/

namespace {
/* the plot list post_process_stuff sets when the model has none of its own
   (plotlist is form_ode.cpp's pointer) */
std::array<int, 10> post_process_plotlist{};
}



int two_d_hist(int col1,int col2,int ndat,int n1,int n2,double xlo,double xhi,double ylo,double yhi)
     /*
       col1,2 are the data you want to histogram
       ndat - number of points in the data
       n1,2 number of bins for two data streams
       xlo,xhi - range of first column
       ylo,yhi - range of second column
       val[0] = value of first data
       val[1] = value of second data
       val[3] = number of points - which will be normalized by ndat
 EXAMPLE of binning
       if xl0 = 0 and xhi=1 and nbin=10
       dx=1/10
       then bins are [0,1/10), [1/10,2/10), ....,[9/10,1)
       thus  bin j = int ((x-xlo)/dx)
             bin k = int ((y-ylo)/dy)
	     if j<0 or j>=nxbin then skip etc
     */
{
  int i,j,k;
  double dx,dy,norm;
  double x,y;
  dx=(xhi-xlo)/static_cast<double>(n1);
  dy=(yhi-ylo)/static_cast<double>(n2);
  norm=1./static_cast<double>(ndat);
  /* now fill the data with the bin values - take the midpoints of 
     each bin
  */
  for(i=0;i<n1;i++)
    for(j=0;j<n2;j++){
      xpp::session().histogram.hist()[0][i+j*n1]=xlo + (i+.5)*dx;
      xpp::session().histogram.hist()[1][i+j*n1]=ylo + (j+.5)*dy;
      xpp::session().histogram.hist()[2][i+j*n1]=0.0;
    }
  for(k=0;k<ndat;k++){
    x=(xpp::session().data_store.col[col1][k]-xlo)/dx;
    y=(xpp::session().data_store.col[col2][k]-ylo)/dy;
    i=static_cast<int>(x);
    j=static_cast<int>(y);
    if((i>=0)&&(i<n1)&&(j>=0)&&(j<n2))
      xpp::session().histogram.hist()[2][i+j*n1]+=norm;
   }  
  return 0;
}

void four_back()
{
 if(xpp::session().histogram.four_here)new_browse_dat(xpp::session().histogram.four(),xpp::session().histogram.four_len);
}

void hist_back()
{
 if(xpp::session().histogram.hist_here)new_browse_dat(xpp::session().histogram.hist(),xpp::session().histogram.hist_len);
}

void new_four(int nmodes, int col)
{
  xpp::Session &s=xpp::session();
  int i;
  int length=nmodes+1;
  float total=s.data_store.col[0][s.data_store.rows-1]-s.data_store.col[0][0];
  float *bob;
  if(s.histogram.four_here){
   data_back();
   s.histogram.four_columns.release();
   s.histogram.four_here=0;
 }
  s.histogram.four_len=nmodes;
 s.histogram.four_columns.make(3,length,xpp::model().neq);
 s.histogram.four_here=1;
for(i=0;i<length;i++)s.histogram.four()[0][i]=static_cast<float>(i)/total; 
 bob=get_data_col(col);
    fft(bob,s.histogram.four()[1],s.histogram.four()[2],nmodes,s.data_store.rows);
 four_back();
  ping();
}

void post_process_stuff()
{
  xpp::Session &s=xpp::session();

  if(s.histogram.post_process==0)return;
    if(N_plist<1)plotlist=post_process_plotlist.data();
    N_plist=2;
    plotlist[0]=0;
    plotlist[1]=1;
    if(s.histogram.post_process==7){ /* two-d histogram stuff */
      twod_hist();
      return;
    }
    if(s.histogram.post_process==1){
      new_hist(s.histogram.info.nbins,s.histogram.info.xlo,s.histogram.info.xhi,s.histogram.info.col,0,"",0);
      return;
    }
    if(s.histogram.post_process==2){
      just_fourier(0);
      return;
    }
    if(s.histogram.post_process==3){
      just_fourier(1);
      return;
    }
    if(s.histogram.post_process>3&&s.histogram.post_process<7){
      just_sd(s.histogram.post_process-4);
      return;
    }

}

int twod_hist()

{
  xpp::Session &s=xpp::session();
  int length;
 length=s.histogram.info.nbins*s.histogram.info.nbins2;
   if(length>=s.data_store.max_rows)
    length=s.data_store.max_rows-1;

  if(s.histogram.hist_here){
    data_back();
    s.histogram.hist_columns.release();
    s.histogram.hist_here=0;
  }

   s.histogram.hist_len=length;
  s.histogram.hist_columns.make(3,length,xpp::model().neq);
  s.histogram.hist_here=2;
  s.histogram.hist_len=length;
  two_d_hist(s.histogram.info.col,s.histogram.info.col2,s.data_store.rows,
	     s.histogram.info.nbins,s.histogram.info.nbins2,
	     s.histogram.info.xlo,s.histogram.info.xhi,s.histogram.info.ylo,s.histogram.info.yhi);

  hist_back();

      ping();
      
  return(1);

}  
int new_2d_hist()
{
  xpp::Session &s=xpp::session();

  if((xpp::model().neq<2)||(s.data_store.rows<3)){
    err_msg("Need more data and at least 3 columns");
    return 0;
  }
  if(get_col_info(&s.histogram.info.col,"Variable 1 ")==0)return(-1);  
  new_int("Number of bins ",&s.histogram.info.nbins);
  new_float("Low ",&s.histogram.info.xlo);
  new_float("Hi ",&s.histogram.info.xhi);
  if(s.histogram.info.nbins<2){
    err_msg("At least 2 bins\n");
    return(0);
  }
  if(s.histogram.info.xlo>=s.histogram.info.xhi){
    err_msg("Low must be less than hi");
    return(0);
  }
  
  if(get_col_info(&s.histogram.info.col2,"Variable 2 ")==0)return(-1);  
  new_int("Number of bins ",&s.histogram.info.nbins2);
  new_float("Low ",&s.histogram.info.ylo);
  new_float("Hi ",&s.histogram.info.yhi);

if(s.histogram.info.nbins2<2){
    err_msg("At least 2 bins\n");
    return(0);
  }
  if(s.histogram.info.ylo>=s.histogram.info.yhi){
    err_msg("Low must be less than hi");
    return(0);
  }

  return(twod_hist());
}
  
void new_hist(int nbins, double zlo, double zhi, int col, int col2, const char *condition, int which)
{
  xpp::Session &s=xpp::session();
  int i,j,index;
  int command[256];
  int cond=0,flag=1;
  double z,y;
  double dz;
  int length=nbins+1;
  if(length>=s.data_store.max_rows)
    length=s.data_store.max_rows-1;
  dz=(zhi-zlo)/static_cast<double>((length-1));
  if(s.histogram.hist_here){
    data_back();
    s.histogram.hist_columns.release();
    s.histogram.hist_here=0;
  }
  s.histogram.hist_len=length;
  s.histogram.hist_columns.make(2,length,xpp::model().neq);
  s.histogram.hist_here=1;
  for(i=0;i<length;i++){
    s.histogram.hist()[0][i]=static_cast<float>((zlo+dz*i));
    s.histogram.hist()[1][i]=0.0;
  }
  if(which==0){
    if(strlen(condition)==0)cond=0;
    else
      {
	if(add_expr(condition,command,&i)){
	  err_msg("Bad condition. Ignoring...");
	  
	}
	else {
	  cond=1;
	}
      }
    for(i=0;i<s.data_store.rows;i++)
      {
	flag=1;
	if(cond){
	  for(j=0;j<xpp::model().node+1;j++)set_ivar(j,static_cast<double>(s.data_store.col[j][i]));
	  for(j=0;j<xpp::model().nmarkov;j++)
	    set_ivar(j+xpp::model().node+1+xpp::model().fix_var,static_cast<double>(s.data_store.col[j+xpp::model().node+1][i]));
	  z=evaluate(command);
	  if(fabs(z)>0.0)flag=1;
	  else flag=0;
	}
	z=(s.data_store.col[col][i]-zlo)/dz;
	index=static_cast<int>(z);
	if(index>=0&&index<length&&flag==1){
	  s.histogram.hist()[1][index]+=1.0;
	}
      }
    s.parser.ncon=xpp::model().ncon_start;
    s.parser.nsym=xpp::model().nsym_start;
    hist_back();
    ping();
    return;
  }
  if(which==1){
    for(i=0;i<s.data_store.rows;i++){
      for(j=0;j<s.data_store.rows;j++){
	y=s.data_store.col[col][i]-s.data_store.col[col][j];
	z=(y-zlo)/dz;
	index=static_cast<int>(z);
	if(index>=0&&index<length)
	  s.histogram.hist()[1][index]+=1.0;
      }
    }
    hist_back();
    ping();
    return;
  }
  if(which==2){
    mycor2(s.data_store.col[col],s.data_store.col[col2],s.data_store.rows,nbins,s.histogram.hist()[1],1);
    hist_back();
    ping();
    return;
  }
  if(which==3){
    fftxcorr(s.data_store.col[col],s.data_store.col[col2],s.data_store.rows,(nbins-1)/2,s.histogram.hist()[1],1);
    hist_back();
    ping();
    return;
  }

}

void column_mean()
{
 int i;
 double sum,sum2,ss;
 double mean,sdev;
 if(xpp::session().data_store.rows<=1){
   err_msg("Need at least 2 data points!");
   return;
 }
 if(get_col_info(&xpp::session().histogram.info.col,"Variable ")==0)return;
 sum=0.0;
 sum2=0.0;
 for(i=0;i<xpp::session().data_store.rows;i++){
   ss=xpp::session().data_store.col[xpp::session().histogram.info.col][i];
   sum+=ss;
   sum2+=(ss*ss);
 }
 mean=sum/static_cast<double>(xpp::session().data_store.rows);
 sdev=sqrt(sum2/static_cast<double>(xpp::session().data_store.rows)-mean*mean);
 err_msg(xpp::format("Mean={:g} Std. Dev. = {:g} ",mean,sdev).c_str());
}

int get_col_info(int *col, const char *prompt)
{
 std::string variable=*col==0?"t":xpp::model().uvar_names[*col-1];
 new_string_of(prompt,variable,XPP_FIELD_NAME_IN(0));
 find_variable(variable.c_str(),col);
 if(*col<0){
   err_msg("No such variable...");
   return(0);
 }
 return(1);
}

void compute_power()
{
  int i;
  double s,c;
  float *datx,*daty,ptot=0;
  compute_fourier();
  if((xpp::model().neq<2)||(xpp::session().data_store.rows<=1))return;
  datx=get_data_col(1);
  daty=get_data_col(2);

  for(i=0;i<xpp::session().histogram.four_len;i++){
    c=datx[i];
    s=daty[i];
    datx[i]=sqrt(s*s+c*c);
    daty[i]=atan2(s,c);
    ptot+=(datx[i]*datx[i]);
  }
  xpp::log(XPP_LOG_INFO, "a0={:g} L2norm= {:g}  \n",datx[0],sqrt(ptot));
}
/* short-term fft 
   first apply a window
   give data, window size, increment size,
   window type - 0-square, 1=par 2=hamming,4-hanning,3- bartlet
   returns a two vectors, real and imaginary
   which have each of the data appended to them
  data is data (not destroyed)
  nr=number of points in data
  win=window size
  w_type=windowing
  pow returns the power 
      size = win/2
*/

int spectrum(float *data,int nr,int win,int w_type,float *pow)
{
  /* assumes 50% overlap */
  int shift=win/2;
  int kwin=(nr-win+1)/shift;
 int i,j,kk;
 float x,nrmf;
 if(nr<2)return(0);
 if(kwin<1)return(0);
 std::vector<float> ct_v(win), d_v(win), st_v(win), f_v(win);
 float *ct=ct_v.data(), *d=d_v.data(), *st=st_v.data(), *f=f_v.data();
 nrmf=0.0;
 for(i=0;i<win;i++){
   x=static_cast<float>(i)/(static_cast<float>(win));
   switch(w_type){
   case 0: f[i]=1; break;
   case 1: f[i]=x*(1-x)*4.0; break;
   case 2: f[i]=.54-.46*cos(2*M_PI*x);break;
   case 4: f[i]=.5*(1-cos(2*M_PI*x));break;
   case 3: f[i]=1-2*fabs(x-.5);break;
   }
   nrmf+=(f[i]*f[i]/win);
 }
 for(i=0;i<shift;i++)
   pow[i]=0.0;
  
 for(j=0;j<kwin;j++){
   for(i=0;i<win;i++){
     kk=(j*shift+i+nr)%nr;
     d[i]=f[i]*data[kk];
   }
   fft(d,ct,st,shift,win);
   for(i=0;i<shift;i++){
     x=ct[i]*ct[i]+st[i]*st[i];
     pow[i]=pow[i]+sqrt(x);
   }
 }
 for(i=0;i<shift;i++)
   pow[i]=pow[i]/((kwin)*sqrt(nrmf));

 return(1);
}

/*  here is what we do - I think it is what MatLab does as well 

    psd(x) breaks data into chunks, takes FFT of each chunk, 
    power of each chunk, and averages this.

   csd(x,y) 
   break into chunks 
   compute for each frequency fft(y)*fft(x)^*
   now average these - note that this will be complex
   what I call the cross spectrum is |Pxy|
  the coherence is
   |Pxy|^2/|Pxx||Pyy|

*/

int cross_spectrum(float *data,float *data2,int nr,int win,int w_type,float *pow,int type)
{
  int shift=win/2;
  int kwin=(nr-win+1)/shift; 
  /*  int kwin=nr/shift; */
  int i,j,kk;
 float x,nrmwin;
 /*float sum; Not used anywhere*/
 if(nr<2)return(0);
 if(kwin<1)return(0);
 std::vector<float> ct_v(win), d_v(win), st_v(win), f_v(win);
 std::vector<float> ct2_v(win), d2_v(win), st2_v(win);
 std::vector<float> pxx_v(win), pyy_v(win), pxyr_v(win), pxym_v(win);
 float *ct=ct_v.data(), *st=st_v.data(), *f=f_v.data(), *d=d_v.data();
 float *ct2=ct2_v.data(), *st2=st2_v.data(), *d2=d2_v.data();
 float *pxx=pxx_v.data(), *pyy=pyy_v.data();
 float *pxyr=pxyr_v.data(), *pxym=pxym_v.data();
 nrmwin=0.0;
 for(i=0;i<win;i++){
   x=static_cast<float>(i)/(static_cast<float>(win));
   switch(w_type){
   case 0: f[i]=1; break;
   case 1: f[i]=x*(1-x)*4.0; break;
   case 4: f[i]=.5*(1-cos(2*M_PI*x));break;
   case 2: f[i]=.54-.46*cos(2*M_PI*x);break;
   case 3: f[i]=1-2*fabs(x-.5);break;
   }
   nrmwin+=f[i]*f[i];
 }
 for(i=0;i<shift;i++){
   pxx[i]=0.0;
   pyy[i]=0.0;
   pxyr[i]=0.0;
   pxym[i]=0.0;
 }
   
 for(j=0;j<=kwin;j++){
   for(i=0;i<win;i++){
     kk=(i+j*shift)%nr;
     d[i]=f[i]*data[kk];
     d2[i]=f[i]*data2[kk];
   }
   fft(d,ct,st,shift,win);
   fft(d2,ct2,st2,shift,win);
   for(i=0;i<shift;i++){
     pxyr[i]+=(ct[i]*ct2[i]+st[i]*st2[i]);
     pxym[i]+=(ct[i]*st2[i]-ct2[i]*st[i]);
     pxx[i]+=(ct[i]*ct[i]+st[i]*st[i]);
     pyy[i]+=(ct2[i]*ct2[i]+st2[i]*st2[i]);

   }
 }
 for(i=0;i<shift;i++){
   pxx[i]=pxx[i]/((kwin)*nrmwin);
   pyy[i]=pyy[i]/((kwin)*nrmwin);
   pxyr[i]=pxyr[i]/((kwin)*nrmwin);
   pxym[i]=pxym[i]/((kwin)*nrmwin);
   pxyr[i]=pxyr[i]*pxyr[i]+pxym[i]*pxym[i];
   if(type==1)
     pow[i]=log(pxyr[i]);
   else
     pow[i]=pxyr[i]/(pxx[i]*pyy[i]);
 }

 return(1);
}

void just_sd(int flag)
{
 xpp::Session &s=xpp::session();
 int length,j;
  float total=s.data_store.col[0][s.data_store.rows-1]-s.data_store.col[0][0];
  spec_type=flag;
  if(s.histogram.hist_here){
    data_back();
    s.histogram.hist_columns.release();
    s.histogram.hist_here=0;
  }  
   s.histogram.hist_len=s.histogram.spec_wid/2;
   length=s.histogram.hist_len+2;
  s.histogram.hist_columns.make(2,length,xpp::model().neq);
  s.histogram.hist_here=1;
  for(j=0;j<s.histogram.hist_len;j++)s.histogram.hist()[0][j]=(static_cast<float>(j)*s.data_store.rows/s.histogram.spec_wid)/total;
  if(spec_type==0)
    spectrum(s.data_store.col[s.histogram.spec_col],s.data_store.rows,s.histogram.spec_wid,s.histogram.spec_win,s.histogram.hist()[1]);
  else
    cross_spectrum(s.data_store.col[s.histogram.spec_col],s.data_store.col[s.histogram.spec_col2],s.data_store.rows,s.histogram.spec_wid,s.histogram.spec_win,s.histogram.hist()[1],spec_type);
  hist_back();
  ping();
}
void compute_sd()
{
  new_int("(0) PSDx, (1) PSDxy, (2) COHxy:",&spec_type);
  
  if(get_col_info(&xpp::session().histogram.spec_col,"Variable ")==0)return;
  if(spec_type>0)
      if(get_col_info(&xpp::session().histogram.spec_col2,"Variable 2 ")==0)return;
  new_int("Window length ",&xpp::session().histogram.spec_wid);
  new_int("0:sqr 1:par 2:ham 3:bart 4:han ",&xpp::session().histogram.spec_win);
  just_sd(spec_type);
}
 
void just_fourier(int flag)
{
  int i;
  double s,c;
  float *datx,*daty;
  int nmodes=xpp::session().data_store.rows/2-1;
  if(xpp::model().neq<2||xpp::session().data_store.rows<=1)return;
   new_four(nmodes,xpp::session().histogram.spec_col);
   if(flag)
     {
       datx=get_data_col(1);
       daty=get_data_col(2);
       
       for(i=0;i<xpp::session().histogram.four_len;i++){
	 c=datx[i];
	 s=daty[i];
	 datx[i]=sqrt(s*s+c*c);
	 daty[i]=atan2(s,c);
	 
       }

     }
}
  
void compute_fourier()
{
  int nmodes=10;
  if(xpp::model().neq<2){
    err_msg("Need at least three data columns");
    return;
  }
  if(xpp::session().data_store.rows<=1){
    err_msg("No data!");
    return;
  }
  if(get_col_info(&xpp::session().histogram.spec_col,"Variable ")==1){
    nmodes=xpp::session().data_store.rows/2-1;
    new_four(nmodes,xpp::session().histogram.spec_col);
  }
}

void compute_correl()
{
  xpp::Session &s=xpp::session();
  int lag;
  float total=s.data_store.col[0][s.data_store.rows-1]-s.data_store.col[0][0],dta;
  dta=total/static_cast<float>((s.data_store.rows-1));
  
  new_int("Number of bins ",&s.histogram.info.nbins);
  new_int("(0)Direct or (1) FFT ", &s.histogram.info.fftc);
  if(s.histogram.info.nbins>(s.data_store.rows/2-1))
    s.histogram.info.nbins=s.data_store.rows/2-2;
  
  s.histogram.info.nbins=2*(s.histogram.info.nbins/2)+1;
  lag=s.histogram.info.nbins/2;

  /* lets try to get the lags correct for plotting */
  s.histogram.info.xlo=-lag*dta;
  s.histogram.info.xhi=lag*dta;
  
  if(get_col_info(&s.histogram.info.col,"Variable 1 ")==0)return;
  if(get_col_info(&s.histogram.info.col2,"Variable 2 ")==0)return;
  new_hist(s.histogram.info.nbins,s.histogram.info.xlo,
	   s.histogram.info.xhi,s.histogram.info.col,s.histogram.info.col2,s.histogram.info.cond.c_str(),2+s.histogram.info.fftc);
}
void compute_stacor()
{
  new_int("Number of bins ",&xpp::session().histogram.info.nbins);
  new_float("Low ",&xpp::session().histogram.info.xlo);
  new_float("Hi ",&xpp::session().histogram.info.xhi);
  if(get_col_info(&xpp::session().histogram.info.col,"Variable ")==0)return;
   new_hist(xpp::session().histogram.info.nbins,xpp::session().histogram.info.xlo,
	   xpp::session().histogram.info.xhi,xpp::session().histogram.info.col,0,xpp::session().histogram.info.cond.c_str(),1);
}

void mycor2(float *x,float *y, int n, int nbins, float *z, int flag)
{
  int i,j;
  int k,count=0,lag=nbins/2;
  float sum,avx=0.0,avy=0.0;
  if(flag){
    for(i=0;i<n;i++){
      avx+=x[i];
      avy+=y[i];
    }
    avx=avx/static_cast<float>(n);
    avy=avy/static_cast<float>(n);
  }
  for(j=0;j<=nbins;j++){
    sum=0.0;
    count=0;
    for(i=0;i<n;i++){
      k=i+j-lag;
      k=(k+n)%n;
      if((k>=0)&&(k<n)){
	count++;
	sum+=(x[i]-avx)*(y[k]-avy);
      }
    }
    if(count>0)
      sum=sum/count; 
    z[j]=sum;
  }
}

void compute_hist()
{
  xpp::Session &s=xpp::session();
  
  new_int("Number of bins ",&s.histogram.info.nbins);
  new_float("Low ",&s.histogram.info.xlo);
  new_float("Hi ",&s.histogram.info.xhi);
  if(get_col_info(&s.histogram.info.col,"Variable ")==0)return;
  new_string_of("Condition ",s.histogram.info.cond,XPP_FIELD_EXPRESSION);
  new_hist(s.histogram.info.nbins,s.histogram.info.xlo,
	   s.histogram.info.xhi,s.histogram.info.col,0,s.histogram.info.cond.c_str(),0);
}

/* experimental -- does it work */
/* nlag should be less than length/2 */
void fftxcorr(float *data1,float *data2,int length,int nlag,float *cr,int flag)
{
  double x,y,sum;
  float av1=0.0,av2=0.0;
  int i;
  if(flag){
    for(i=0;i<length;i++){
      av1+=data1[i];
      av2+=data2[i];
    }
    av1=av1/static_cast<float>(length);
    av2=av2/static_cast<float>(length);
  }

  std::vector<double> re1_v(length), im1_v(length), re2_v(length), im2_v(length);
  double *re1=re1_v.data(), *im1=im1_v.data(), *re2=re2_v.data(), *im2=im2_v.data();

  for(i=0;i<length;i++){
    re1[i]=(data1[i]-av1);
    re2[i]=(data2[i]-av2);
  }

   /* both transforms and the inverse divided by the length */
   xpp_fft(length,re1,im1,1,1.0/length);
   xpp_fft(length,re2,im2,1,1.0/length);
   for(i=0;i<length;i++){
     x=re1[i]*re2[i]+im1[i]*im2[i];
     y=im1[i]*re2[i]-im2[i]*re1[i];
     re1[i]=x;
     im1[i]=-y;
   }
   xpp_fft(length,re1,im1,-1,1.0/length);
   /* now lets order these
      I think!  */
   sum=0.0;
   for(i=0;i<nlag;i++){
     sum+=fabs(im1[i]);
     cr[nlag+i]=static_cast<float>(re1[i])*length; /* positive part of the correlation */
   }
   for(i=0;i<nlag;i++){
     sum+=fabs(im1[length-nlag+i]);
     cr[i]=static_cast<float>(re1[length-nlag+i])*length;}
   xpp::log(XPP_LOG_INFO, "residual = {:g}\n",sum);
   
}

/* the Fourier modes 0..nmodes-1 of data[0..length-1]: ct[i] and st[i]
   are twice the real and imaginary parts of the transform
   (1/length) sum_j data[j] exp(+2 pi i j k/length), ct[0] once */
void fft(float *data, float *ct, float *st, int nmodes, int length)
{
  if(length<=0)return;
  std::vector<double> in(data,data+length);
  const int half=length/2+1;
  std::vector<double> re(half), im(half);
  xpp_fft_real(length,in.data(),re.data(),im.data(),1,1.0/length);
  ct[0]=static_cast<float>(re[0]);
  st[0]=0.0;
  for(int i=1;i<nmodes;i++){
    /* past the half the real transform stores, X[n-k] = conj(X[k]) */
    const bool mirror=i>=half;
    const int k=mirror?length-i:i;
    ct[i]=static_cast<float>(re[k]*2.0);
    st[i]=static_cast<float>((mirror?-im[k]:im[k])*2.0);
  }
}

