#ifndef _xAuto_h_
#define _xAuto_h_

#include "auto_codes.h"

#include <vector>

/* a user-defined output (a Mark of the AUTO window): the parameter it is at
   (AUTO's slot; the period for T) and the value that parameter has there */
struct AutoMark {
  int parameter;
  double value;
};
/* a weight of a parameter in the pseudo-arclength norm (AUTO's THL) */
struct AutoRunWeight {
  int index;
  double value;
};

 typedef struct {
    int ndim;
    xpp::AutoProblem ips;
    int irs;
    int ilp;
    int nicp;
    std::vector<int> icp;
    int ntst;
    int ncol;
    int iad;
    int isp;
    int isw;
    int iplt;
    int nbc;
    int nint;
    int nmx;
    double rl0;
    double rl1;
    double a0;
    double a1;
    int npr;
    int mxbf;
    int iid;
    int itmx;
    int itnw;
    int nwtn;
    int jac;
    double epsl;
    double epss;
    double epsu;
    double ds;
    double dsmax;
    double dsmin;
    int iads;
    std::vector<AutoRunWeight> weights;
    std::vector<AutoMark> marks;
   /*  these are for the homoclinic stuff */
    int nunstab;
   int nstab;
   int iequib; /* +1 homoclinic   -2 heteroclinic */



  } XAUTO;

/*  for homcont  - itwist=0, istart=2, nrev=0,nfixed=0,npsi=0 */

#endif

