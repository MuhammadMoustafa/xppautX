#include "histogram.h"
#include "model_options.h"
#include "session.h"
#include "mykeydef.h"
#include "getvar.h"
#include "model.h"
#include "storage.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <algorithm>
#include <array>
#include <vector>

#include "adj2.h"
#include "browse.h"

#include "expr.h"
#include "xpp_log.h"
#include "xpp_math.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "form_ode.h"
#include "load_eqn.h"

namespace xpp {





int two_d_hist(xpp::Session &s, int col1,int col2,int ndat,int n1,int n2,double xlo,double xhi,double ylo,double yhi)
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
      s.histogram.hist()[0][i+j*n1]=xlo + (i+.5)*dx;
      s.histogram.hist()[1][i+j*n1]=ylo + (j+.5)*dy;
      s.histogram.hist()[2][i+j*n1]=0.0;
    }
  for(k=0;k<ndat;k++){
    x=(s.data_store.col[col1][k]-xlo)/dx;
    y=(s.data_store.col[col2][k]-ylo)/dy;
    i=static_cast<int>(x);
    j=static_cast<int>(y);
    if((i>=0)&&(i<n1)&&(j>=0)&&(j<n2))
      s.histogram.hist()[2][i+j*n1]+=norm;
   }  
  return 0;
}

void four_back(xpp::Session &s)
{
 if(s.histogram.four_here)new_browse_dat(s,s.histogram.four_columns.view(s.data_store),s.histogram.four_len);
}

void hist_back(xpp::Session &s)
{
 if(s.histogram.hist_here)new_browse_dat(s,s.histogram.hist_columns.view(s.data_store),s.histogram.hist_len);
}

Result<> reload_histogram(xpp::Session &s)
{
 return show_derived(s,s.histogram.hist_here!=0,"histogram","No histogram yet: compute one first",hist_back);
}

void new_four(xpp::Session &s, int nmodes, int col)
{
  int i;
  int length=nmodes+1;
  float total=s.data_store.col[0][s.data_store.rows-1]-s.data_store.col[0][0];
  float *bob;
  if(s.histogram.four_here){
   data_back(s);
   s.histogram.four_columns.release();
   s.histogram.four_here=0;
 }
  s.histogram.four_len=nmodes;
 s.histogram.four_columns.make(s.data_store,3,length,s.model().neq);
 s.histogram.four_here=1;
for(i=0;i<length;i++)s.histogram.four()[0][i]=static_cast<float>(i)/total; 
 bob=get_data_col(s,col);
    fourier_modes(std::span(bob,s.data_store.rows),s.histogram.four()[1],s.histogram.four()[2],nmodes);
 four_back(s);
  ping();
}

namespace {
/* whether the run stored at least `rows` rows, or why it did not */
Result<> need_rows(const xpp::Session &s, const std::string &command, int rows)
{
  if(s.data_store.rows<rows)
    return fail(command,xpp::format("The run stored {} row(s): nothing to analyse (at least {} are needed)",s.data_store.rows,rows),command_place());
  return {};
}

/* whether the run stored what an analysis of a column's data needs (at least 2 rows, and 2 variables), or why not */
Result<> need_data(const xpp::Session &s, const std::string &command)
{
  if(auto rows=need_rows(s,command,2);!rows)return rows;
  if(s.model().neq<2)
    return fail(command,xpp::format("The model has {} variable(s): the analysis needs at least 2",s.model().neq),command_place());
  return {};
}

/* whether a histogram's bins and range are ones the options take (HISTBINS, HISTLO, HISTHI): the same
   rules, or the command's error naming them */
Result<> histogram_range_ok(const std::string &command,int bins,double lo,double hi)
{
  if(const char *no=xpp::rule_problem(xpp::OptionRule::whole_at_least_two,bins,xpp::option_limit::histogram_bins))
    return fail(command,xpp::format("Number of bins ({}) {}",bins,no),command_place());
  if(const auto bad=xpp::order_problem("Low",lo,"Hi",hi,true))
    return fail(command,*bad,command_place());
  return {};
}

/* the histogram's samples: what its bins hold */
long long counted(xpp::Session &s)
{
  double total=0;
  for(int i=0;i<s.histogram.hist_len;i++)total+=static_cast<double>(s.histogram.hist()[1][i]);
  return static_cast<long long>(total);
}
}

void post_process_stuff(xpp::Session &s)
{

  if(s.histogram.post_process==0)return;
    s.plot_list.assign({0,1});
    if(s.histogram.post_process==7){ /* two-d histogram stuff */
      xpp::ok_or_show(new_2d_hist(s));
      return;
    }
    if(s.histogram.post_process==1){
      xpp::ok_or_show(new_hist(s,s.histogram.info.nbins,s.histogram.info.xlo,s.histogram.info.xhi,s.histogram.info.col,0,"",0));
      return;
    }
    if(s.histogram.post_process==2){
      xpp::ok_or_show(just_fourier(s,0));
      return;
    }
    if(s.histogram.post_process==3){
      xpp::ok_or_show(just_fourier(s,1));
      return;
    }
    if(s.histogram.post_process>3&&s.histogram.post_process<7){
      xpp::ok_or_show(just_sd(s,s.histogram.post_process-4));
      return;
    }

}

int twod_hist(xpp::Session &s)

{
  int length;
 length=s.histogram.info.nbins*s.histogram.info.nbins2;
   if(length>=s.data_store.max_rows)
    length=s.data_store.max_rows-1;

  if(s.histogram.hist_here){
    data_back(s);
    s.histogram.hist_columns.release();
    s.histogram.hist_here=0;
  }

   s.histogram.hist_len=length;
  s.histogram.hist_columns.make(s.data_store,3,length,s.model().neq);
  s.histogram.hist_here=2;
  s.histogram.hist_len=length;
  two_d_hist(s,s.histogram.info.col,s.histogram.info.col2,s.data_store.rows,
	     s.histogram.info.nbins,s.histogram.info.nbins2,
	     s.histogram.info.xlo,s.histogram.info.xhi,s.histogram.info.ylo,s.histogram.info.yhi);

  hist_back(s);

      ping();
      
  return(1);

}  

namespace {
Result<> need_data_for_2d(const xpp::Session &s)
{
  if((s.model().neq<2)||(s.data_store.rows<3))
    return fail("histogram","Need more data and at least 3 columns",command_place());
  return {};
}
}

Result<> new_2d_hist(xpp::Session &s)
{
  if(auto data=need_data_for_2d(s);!data)return data;
  HIST_INFO &info=s.histogram.info;
  if(auto first=histogram_range_ok("histogram",info.nbins,info.xlo,info.xhi);!first)return first;
  if(auto second=histogram_range_ok("histogram",info.nbins2,info.ylo,info.yhi);!second)return second;
  /* the bins are rows of the data table, which has room for max_rows less one */
  if(static_cast<long long>(info.nbins)*info.nbins2>=s.data_store.max_rows)
    return fail("histogram",xpp::format("{} by {} bins is {} rows: the data table has room for {}; use fewer bins",info.nbins,info.nbins2,static_cast<long long>(info.nbins)*info.nbins2,s.data_store.max_rows-1),command_place());
  twod_hist(s);
  return {};
}

bool ask_histogram_2d(xpp::Session &s)
{
  HIST_INFO &info=s.histogram.info;
  if(!xpp::ok_or_show(need_data_for_2d(s)))return false;
  if(!ask_column(s,info.col,"Variable 1 "))return false;
  new_int("Number of bins ",&info.nbins);
  new_float(s,"Low ",&info.xlo);
  new_float(s,"Hi ",&info.xhi);
  if(!xpp::ok_or_show(histogram_range_ok("histogram",info.nbins,info.xlo,info.xhi)))return false;
  if(!ask_column(s,info.col2,"Variable 2 "))return false;
  new_int("Number of bins ",&info.nbins2);
  new_float(s,"Low ",&info.ylo);
  new_float(s,"Hi ",&info.yhi);
  return true;
}
  
xpp::Result<> new_hist(xpp::Session &s, int nbins, double zlo, double zhi, int col, int col2, const char *condition, int which)
{
  int i,j,index;
  std::vector<int> command;
  int cond=0,flag=1;
  double z,y;
  double dz;
  int length=nbins+1;
  if(length>=s.data_store.max_rows)
    length=s.data_store.max_rows-1;
  dz=(zhi-zlo)/static_cast<double>((length-1));
  if(which==0&&strlen(condition)>0){
    /* a condition that does not compile is the command's error, nothing is made */
    if(add_expr(s,condition,command,&i)){
      s.parser.ncon=s.model().ncon_start;
      s.parser.nsym=s.model().nsym_start;
      return xpp::fail("histogram",xpp::format("The condition \"{}\" is not a formula",condition),command_place());
    }
    cond=1;
  }
  if(s.histogram.hist_here){
    data_back(s);
    s.histogram.hist_columns.release();
    s.histogram.hist_here=0;
  }
  s.histogram.hist_len=length;
  s.histogram.hist_columns.make(s.data_store,2,length,s.model().neq);
  s.histogram.hist_here=1;
  for(i=0;i<length;i++){
    s.histogram.hist()[0][i]=static_cast<float>((zlo+dz*i));
    s.histogram.hist()[1][i]=0.0;
  }
  if(which==0){
    for(i=0;i<s.data_store.rows;i++)
      {
	if(my_abort() == ESC) break;
	flag=1;
	if(cond){
	  for(j=0;j<s.model().node+1;j++)setvar(s,j,static_cast<double>(s.data_store.col[j][i]));
	  for(j=0;j<s.model().nmarkov;j++)
	    setvar(s,j+s.model().node+1+s.model().fix_var,static_cast<double>(s.data_store.col[j+s.model().node+1][i]));
	  z=evaluate(s,command.data());
	  if(fabs(z)>0.0)flag=1;
	  else flag=0;
	}
	z=(s.data_store.col[col][i]-zlo)/dz;
	index=static_cast<int>(z);
	if(index>=0&&index<length&&flag==1){
	  s.histogram.hist()[1][index]+=1.0;
	}
      }
    s.parser.ncon=s.model().ncon_start;
    s.parser.nsym=s.model().nsym_start;
    hist_back(s);
    ping();
    return {};
  }
  if(which==1){
    for(i=0;i<s.data_store.rows;i++){
      if(my_abort() == ESC) break;
      for(j=0;j<s.data_store.rows;j++){
	y=s.data_store.col[col][i]-s.data_store.col[col][j];
	z=(y-zlo)/dz;
	index=static_cast<int>(z);
	if(index>=0&&index<length)
	  s.histogram.hist()[1][index]+=1.0;
      }
    }
    hist_back(s);
    ping();
    return {};
  }
  if(which==2){
    mycor2(std::span(s.data_store.col[col],s.data_store.rows),std::span(s.data_store.col[col2],s.data_store.rows),nbins,s.histogram.hist()[1],1);
    hist_back(s);
    ping();
    return {};
  }
  if(which==3){
    fftxcorr(std::span(s.data_store.col[col],s.data_store.rows),std::span(s.data_store.col[col2],s.data_store.rows),(nbins-1)/2,s.histogram.hist()[1],1);
    hist_back(s);
    ping();
    return {};
  }
  return {};
}

Result<ColumnStatistics> column_mean(xpp::Session &s)
{
 if(s.data_store.rows<=1)
   return fail("mean","Need at least 2 data points!",command_place());
 double sum=0.0,sum2=0.0;
 for(int i=0;i<s.data_store.rows;i++){
   const double ss=s.data_store.col[s.histogram.info.col][i];
   sum+=ss;
   sum2+=(ss*ss);
 }
 const double mean=sum/static_cast<double>(s.data_store.rows);
 /* rounding may take a variance of constant data just below 0 */
 const double variance=std::max(0.0,sum2/static_cast<double>(s.data_store.rows)-mean*mean);
 return ColumnStatistics{mean,variance};
}

bool ask_column(const xpp::Session &s, int &col, const char *prompt)
{
 std::string variable=ind_to_sym(s,col);
 new_string_of(prompt,variable,XPP_FIELD_NAME_IN(0));
 int found=-1;
 find_variable(s,variable.c_str(),&found);
 if(found<0){
   command_error("mean", "No such variable...");
   return false;
 }
 col=found;
 return true;
}

Result<int> compute_power(xpp::Session &s)
{
  int i;
  double sv,c;
  float *datx,*daty,ptot=0;
  if(auto fourier=compute_fourier(s);!fourier)return std::unexpected(fourier.error());
  datx=get_data_col(s,1);
  daty=get_data_col(s,2);

  for(i=0;i<s.histogram.four_len;i++){
    c=datx[i];
    sv=daty[i];
    datx[i]=sqrt(sv*sv+c*c);
    daty[i]=xpp::math::atan2(sv,c);
    ptot+=(datx[i]*datx[i]);
  }
  xpp::log(XPP_LOG_INFO, "a0={:g} L2norm= {:g}  \n",datx[0],sqrt(ptot));
  return s.histogram.four_len;
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

namespace {
/* the data window of win points, w_type 0 square, 1 parzen, 2 hamming, 3 bartlett, 4 hanning */
std::vector<float> data_window(int win, int w_type)
{
  std::vector<float> f(win);
  for(int i=0;i<win;i++){
    const float x=static_cast<float>(i)/(static_cast<float>(win));
    switch(w_type){
    case 0: f[i]=1; break;
    case 1: f[i]=x*(1-x)*4.0; break;
    case 2: f[i]=.54-.46*xpp::math::cos(2*M_PI*x);break;
    case 3: f[i]=1-2*fabs(x-.5);break;
    case 4: f[i]=.5*(1-xpp::math::cos(2*M_PI*x));break;
    }
  }
  return f;
}
}

int spectrum(std::span<const float> data,int win,int w_type,float *pow)
{
  const int nr=static_cast<int>(data.size());
  /* assumes 50% overlap */
  int shift=win/2;
  int kwin=(nr-win+1)/shift;
 int i,j,kk;
 float x,nrmf;
 if(nr<2)return(0);
 if(kwin<1)return(0);
 std::vector<float> ct_v(win), d_v(win), st_v(win);
 float *ct=ct_v.data(), *d=d_v.data(), *st=st_v.data();
 const std::vector<float> window=data_window(win,w_type);
 const float *f=window.data();
 nrmf=0.0;
 for(i=0;i<win;i++)nrmf+=(f[i]*f[i]/win);
 for(i=0;i<shift;i++)
   pow[i]=0.0;
  
 for(j=0;j<kwin;j++){
   for(i=0;i<win;i++){
     kk=(j*shift+i+nr)%nr;
     d[i]=f[i]*data[kk];
   }
   fourier_modes(d_v,ct,st,shift);
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

int cross_spectrum(std::span<const float> data,std::span<const float> data2,int win,int w_type,float *pow,int type)
{
  const int nr=static_cast<int>(data.size());
  int shift=win/2;
  int kwin=(nr-win+1)/shift; 
  /*  int kwin=nr/shift; */
  int i,j,kk;
 float nrmwin;
 /*float sum; Not used anywhere*/
 if(nr<2)return(0);
 if(kwin<1)return(0);
 std::vector<float> ct_v(win), d_v(win), st_v(win);
 std::vector<float> ct2_v(win), d2_v(win), st2_v(win);
 std::vector<float> pxx_v(win), pyy_v(win), pxyr_v(win), pxym_v(win);
 const std::vector<float> window=data_window(win,w_type);
 const float *f=window.data();
 float *ct=ct_v.data(), *st=st_v.data(), *d=d_v.data();
 float *ct2=ct2_v.data(), *st2=st2_v.data(), *d2=d2_v.data();
 float *pxx=pxx_v.data(), *pyy=pyy_v.data();
 float *pxyr=pxyr_v.data(), *pxym=pxym_v.data();
 nrmwin=0.0;
 for(i=0;i<win;i++)nrmwin+=f[i]*f[i];
 for(i=0;i<shift;i++){
   pxx[i]=0.0;
   pyy[i]=0.0;
   pxyr[i]=0.0;
   pxym[i]=0.0;
 }
   
 for(j=0;j<kwin;j++){
   for(i=0;i<win;i++){
     kk=(i+j*shift)%nr;
     d[i]=f[i]*data[kk];
     d2[i]=f[i]*data2[kk];
   }
   fourier_modes(d_v,ct,st,shift);
   fourier_modes(d2_v,ct2,st2,shift);
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
     pow[i]=xpp::math::log(pxyr[i]);
   else
     pow[i]=pxyr[i]/(pxx[i]*pyy[i]);
 }

 return(1);
}

Result<int> just_sd(xpp::Session &s, int flag)
{
  HistogramState &h=s.histogram;
  if(auto data=need_data(s,"spectrum");!data)return std::unexpected(data.error());
  if(flag<0||flag>2)
    return fail("spectrum",xpp::format("The spectrum type {} is not 0 (PSDx), 1 (PSDxy) or 2 (COHxy)",flag),command_place());
  /* a window is at least 2 points (it is moved by half its length) and no more than the run stored */
  if(h.spec_wid<2||h.spec_wid>s.data_store.rows)
    return fail("spectrum",xpp::format("No window of {} points fits in the {} rows the run stored (a window is 2 points or more, and moves by half its length)",h.spec_wid,s.data_store.rows),command_place());
  if(h.spec_win<0||h.spec_win>4)
    return fail("spectrum",xpp::format("The window type {} is not one of 0 to 4 (square, parzen, hamming, bartlett, hann)",h.spec_win),command_place());
  float total=s.data_store.col[0][s.data_store.rows-1]-s.data_store.col[0][0];
  h.spec_type=flag;
  if(h.hist_here){
    data_back(s);
    h.hist_columns.release();
    h.hist_here=0;
  }  
   h.hist_len=h.spec_wid/2;
   const int length=h.hist_len+2;
  h.hist_columns.make(s.data_store,2,length,s.model().neq);
  h.hist_here=1;
  for(int j=0;j<h.hist_len;j++)h.hist()[0][j]=(static_cast<float>(j)*s.data_store.rows/h.spec_wid)/total;
  const int computed=h.spec_type==0
    ? spectrum(std::span(s.data_store.col[h.spec_col],s.data_store.rows),h.spec_wid,h.spec_win,h.hist()[1])
    : cross_spectrum(std::span(s.data_store.col[h.spec_col],s.data_store.rows),std::span(s.data_store.col[h.spec_col2],s.data_store.rows),h.spec_wid,h.spec_win,h.hist()[1],h.spec_type);
  hist_back(s);
  ping();
  if(!computed)
    return fail("spectrum",xpp::format("No window of {} points fits in the {} rows the run stored (windows overlap by half)",h.spec_wid,s.data_store.rows),command_place());
  return h.hist_len;
}
Result<int> compute_sd(xpp::Session &s)
{
  return just_sd(s,s.histogram.spec_type);
}

bool ask_spectral_density(xpp::Session &s)
{
  HistogramState &h=s.histogram;
  new_int("(0) PSDx, (1) PSDxy, (2) COHxy:",&h.spec_type);
  
  if(!ask_column(s,h.spec_col,"Variable "))return false;
  if(h.spec_type>0)
      if(!ask_column(s,h.spec_col2,"Variable 2 "))return false;
  new_int("Window length ",&h.spec_wid);
  new_int("0:sqr 1:par 2:ham 3:bart 4:han ",&h.spec_win);
  return true;
}
 
Result<int> just_fourier(xpp::Session &s, int flag)
{
  int i;
  double sv,c;
  float *datx,*daty;
  int nmodes=s.data_store.rows/2-1;
  if(auto data=need_data(s,"fourier");!data)return std::unexpected(data.error());
   new_four(s,nmodes,s.histogram.spec_col);
   if(flag)
     {
       datx=get_data_col(s,1);
       daty=get_data_col(s,2);
       
       for(i=0;i<s.histogram.four_len;i++){
	 c=datx[i];
	 sv=daty[i];
	 datx[i]=sqrt(sv*sv+c*c);
	 daty[i]=xpp::math::atan2(sv,c);
	 
       }

     }
   return s.histogram.four_len;
}
  
Result<int> compute_fourier(xpp::Session &s)
{
  return just_fourier(s,0);
}

Result<int> compute_correl(xpp::Session &s)
{
  HIST_INFO &info=s.histogram.info;
  if(auto data=need_data(s,"correlation");!data)return std::unexpected(data.error());
  /* the lags are bins either side of 0, as many as half the data less 2 allows (the data are taken as periodic) */
  const int most=s.data_store.rows/2-2;
  if(info.nbins<2||info.nbins>most)
    return fail("correlation",xpp::format("Number of lags ({}) is not between 2 and {}, half the {} rows the run stored less 2",info.nbins,most,s.data_store.rows),command_place());
  if(info.fftc!=0&&info.fftc!=1)
    return fail("correlation",xpp::format("The method {} is not 0 (direct) or 1 (FFT)",info.fftc),command_place());
  const float total=s.data_store.col[0][s.data_store.rows-1]-s.data_store.col[0][0];
  const float dta=total/static_cast<float>((s.data_store.rows-1));
  
  info.nbins=2*(info.nbins/2)+1;
  const int lag=info.nbins/2;

  /* lets try to get the lags correct for plotting */
  info.xlo=-lag*dta;
  info.xhi=lag*dta;
  
  if(auto made=new_hist(s,info.nbins,info.xlo,info.xhi,info.col,info.col2,info.cond.c_str(),2+info.fftc);!made)return std::unexpected(made.error());
  return s.histogram.hist_len;
}

bool ask_cross_correlation(xpp::Session &s)
{
  HIST_INFO &info=s.histogram.info;
  new_int("Number of bins ",&info.nbins);
  new_int("(0)Direct or (1) FFT ", &info.fftc);
  return ask_column(s,info.col,"Variable 1 ")&&ask_column(s,info.col2,"Variable 2 ");
}

Result<int> compute_stacor(xpp::Session &s)
{
  HIST_INFO &info=s.histogram.info;
  if(auto rows=need_rows(s,"correlation",1);!rows)return std::unexpected(rows.error());
  if(auto range=histogram_range_ok("correlation",info.nbins,info.xlo,info.xhi);!range)return std::unexpected(range.error());
  if(auto made=new_hist(s,info.nbins,info.xlo,info.xhi,info.col,0,info.cond.c_str(),1);!made)return std::unexpected(made.error());
  return s.histogram.hist_len;
}

bool ask_autocorrelation(xpp::Session &s)
{
  HIST_INFO &info=s.histogram.info;
  new_int("Number of bins ",&info.nbins);
  new_float(s,"Low ",&info.xlo);
  new_float(s,"Hi ",&info.xhi);
  if(!xpp::ok_or_show(histogram_range_ok("correlation",info.nbins,info.xlo,info.xhi)))return false;
  return ask_column(s,info.col,"Variable ");
}

void mycor2(std::span<const float> x,std::span<const float> y, int nbins, float *z, int flag)
{
  const int n=static_cast<int>(x.size());
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

Result<HistogramCounts> compute_hist(xpp::Session &s)
{
  HIST_INFO &info=s.histogram.info;
  if(auto rows=need_rows(s,"histogram",1);!rows)return std::unexpected(rows.error());
  if(auto range=histogram_range_ok("histogram",info.nbins,info.xlo,info.xhi);!range)return std::unexpected(range.error());
  if(auto made=new_hist(s,info.nbins,info.xlo,info.xhi,info.col,0,info.cond.c_str(),0);!made)return std::unexpected(made.error());
  return HistogramCounts{info.nbins,counted(s)};
}

bool ask_histogram(xpp::Session &s)
{
  HIST_INFO &info=s.histogram.info;
  new_int("Number of bins ",&info.nbins);
  new_float(s,"Low ",&info.xlo);
  new_float(s,"Hi ",&info.xhi);
  if(!xpp::ok_or_show(histogram_range_ok("histogram",info.nbins,info.xlo,info.xhi)))return false;
  if(!ask_column(s,info.col,"Variable "))return false;
  new_string_of("Condition ",info.cond,XPP_FIELD_EXPRESSION);
  return true;
}

/* experimental -- does it work */
/* nlag should be less than length/2 */
void fftxcorr(std::span<const float> data1,std::span<const float> data2,int nlag,float *cr,int flag)
{
  const int length=static_cast<int>(data1.size());
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
   xpp::fft(re1_v,im1_v,1,1.0/length);
   xpp::fft(re2_v,im2_v,1,1.0/length);
   for(i=0;i<length;i++){
     x=re1[i]*re2[i]+im1[i]*im2[i];
     y=im1[i]*re2[i]-im2[i]*re1[i];
     re1[i]=x;
     im1[i]=-y;
   }
   xpp::fft(re1_v,im1_v,-1,1.0/length);
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

/* the Fourier modes 0..nmodes-1 of data (length points): ct[i] and st[i]
   are twice the real and imaginary parts of the transform
   (1/length) sum_j data[j] exp(+2 pi i j k/length), ct[0] once */
void fourier_modes(std::span<const float> data, float *ct, float *st, int nmodes)
{
  const int length=static_cast<int>(data.size());
  if(length<=0)return;
  std::vector<double> in(data.begin(),data.end());
  const int half=length/2+1;
  std::vector<double> re(half), im(half);
  xpp::fft_real(in,re,im,1,1.0/length);
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

} // namespace xpp
