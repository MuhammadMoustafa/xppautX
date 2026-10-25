#include "storage.h"
#include "session.h"
#include "xpp_ui.h"
#include "xpp_math.h"
#include <math.h>
#include <cstring>
#include <vector>
#include "dormpri.h"
#include "flags.h"
#include "xpp_log.h"
#include "model.h"
#include "my_rhs.h"
#include <algorithm>
#include <cmath>
#include "xpp_job.h"

/* Hairer's DOP853 and DOPRI5, with what XPP's one caller (the Solver
   below) always chose built in: one scalar relative and absolute
   tolerance, dense output of every component, no solution callback, and
   Hairer's defaults for the rounding unit, the safety factor and the step
   ratio bounds. A failure is returned as its code, with the time and step
   it happened at in the run; the Solver reports it, once. */

namespace {
/* One continuous run: attempts, FSAL stages, controller and dense coefficients. */
struct DpRun {
  long nstep=0, naccpt=0, nrejct=0;
  bool resume=false;
  double next_h=0, facold=1e-4, hlamb=0;
  int iasti=0, nonsti=0;
  double xold=0, xout=0;
  double failed_h=0; /* the step size an attempt failed with */
  double *yy1=nullptr, *k1=nullptr, *k2=nullptr, *k3=nullptr, *k4=nullptr, *k5=nullptr, *k6=nullptr,
         *k7=nullptr, *k8=nullptr, *k9=nullptr, *k10=nullptr, *ysti=nullptr;
  double *rcont1=nullptr, *rcont2=nullptr, *rcont3=nullptr, *rcont4=nullptr;
  double *rcont5=nullptr, *rcont6=nullptr, *rcont7=nullptr, *rcont8=nullptr;
};
}

/* Hairer's rounding unit (the smallest number with 1+uround > 1, for his
   step-size-too-small test) and safety factor of the step controller */
constexpr double DOPRI_UROUND = 2.3E-16;
constexpr double DOPRI_SAFE = 0.9;
/* the run's attempts (DOPRI_RUN_ATTEMPTS, dormpri.h), and the accepted
   steps between stiffness tests: every one, as XPP's dormprin always did */
constexpr long DOPRI_NMAX = xpp::DOPRI_RUN_ATTEMPTS;
constexpr long DOPRI_NSTIFF = 1;
/* Hairer's defaults for each method: the bounds fac1 <= hnew/h <= fac2
   and beta, the Lund stabilization of the step controller */
constexpr double DOP853_FAC1 = 0.333, DOP853_FAC2 = 6.0, DOP853_BETA = 0.0;
constexpr double DOPRI5_FAC1 = 0.2, DOPRI5_FAC2 = 10.0, DOPRI5_BETA = 0.04;

static void dprhs(xpp::Session &s, unsigned n, double t, double *y, double *f)
{
 my_rhs(s,t,y,f,n);

}

/* the initial step size of a method of order iord (both methods') */
static double hinit (xpp::Session &s, unsigned n, double x, double* y,
	      double posneg, double* f0, double* f1, double* yy1, int iord,
	      double hmax, double atoli, double rtoli)
{
  double   dnf, dny, sk, h, h1, der2, der12, sqr;
  unsigned i;

  dnf = 0.0;
  dny = 0.0;

  for (i = 0; i < n; i++)
    {
      sk = atoli + rtoli * fabs(y[i]);
      sqr = f0[i] / sk;
      dnf += sqr*sqr;
      sqr = y[i] / sk;
      dny += sqr*sqr;
    }

  if ((dnf <= 1.0E-10) || (dny <= 1.0E-10))
    h = 1.0E-6;
  else
    h = sqrt (dny/dnf) * 0.01;

  h = std::min(hmax, h);
  h = xpp::sign (h, posneg);

  /* perform an explicit Euler step */
  for (i = 0; i < n; i++)
    yy1[i] = y[i] + h * f0[i];
  dprhs (s, n, x+h, yy1, f1);

  /* estimate the second derivative of the solution */
  der2 = 0.0;
  for (i = 0; i < n; i++)
    {
      sk = atoli + rtoli * fabs(y[i]);
      sqr = (f1[i] - f0[i]) / sk;
      der2 += sqr*sqr;
    }
  der2 = sqrt (der2) / h;

  /* step size is computed such that h**iord * max(norm(f0),norm(der2)) = 0.01 */
  der12 = std::max(sqrt(dnf), fabs(der2));
  if (der12 <= 1.0E-15)
    h1 = std::max(fabs(h)*1.0E-3, 1.0E-6);
  else
    h1 = xpp::math::pow(0.01/der12, 1.0/static_cast<double>(iord));
  h = std::min(std::min(hmax, h1), 100.0 * fabs(h));

  return xpp::sign (h, posneg);

} /* hinit */

/* where a call's attempts start: k1 = f(x, y) and Hairer's initial step
   for a fresh run, else the step the run's last call chose */
static double dp_first_step (xpp::Session &s, unsigned n, double x, double *y, double posneg, int iord,
			     double hmax, double atoli, double rtoli, DpRun &r)
{
  double h = 0.0;
  if (!r.resume) dprhs (s, n, x, y, r.k1);
  else h = r.next_h;
  if (h == 0.0)
    h = hinit (s, n, x, y, posneg, r.k1, r.k2, r.k3, iord, hmax, atoli, rtoli);
  return h;
}

/* before an attempt from x with step h: the run's attempts spent (-2) or
   h below the rounding limit (-3) end the run; a step that would reach
   past xend is clipped to it, the last one; 0 to go on */
static int dp_before_attempt (DpRun &r, double x, double &h, double xend, double posneg, int &last)
{
  if (r.nstep >= DOPRI_NMAX)
    {
      r.xout = x;
      return -2;
    }
  if (0.1 * fabs(h) <= fabs(x) * DOPRI_UROUND)
    {
      r.xout = x;
      r.failed_h = h;
      return -3;
    }
  if ((x + 1.01*h - xend) * posneg > 0.0)
    {
      h = xend - x;
      last = 1;
    }
  r.nstep++;
  return 0;
}

/* a step accepted, now at x: 1 when it was the last one, else 2 with the
   next step size (hnew, bounded by hmax and, after a rejection, by h) and
   the controller kept for the run's next call */
static int dp_accepted (DpRun &r, double x, double h, double hnew, double hmax, double posneg, int last, int reject,
			double facold, double hlamb, int iasti, int nonsti)
{
  if (last)
    {
      r.xout = x;
      return 1;
    }
  if (fabs(hnew) > hmax)
    hnew = posneg * hmax;
  if (reject)
    hnew = posneg * std::min(fabs(h), fabs(hnew));
  r.next_h=hnew; r.facold=facold; r.hlamb=hlamb;
  r.iasti=iasti; r.nonsti=nonsti; r.xout=x; r.resume=true;
  return 2; /* one accepted step, with its interpolant and controller intact */
}

/* core integrator */
static int dopcor (xpp::Session &s, unsigned n, double x, double* y, double xend,
		   double hmax, double rtoli, double atoli, DpRun &r)
{
  constexpr double safe = DOPRI_SAFE, fac1 = DOP853_FAC1, fac2 = DOP853_FAC2, beta = DOP853_BETA;
  double   facold, expo1, fac, facc1, facc2, fac11, posneg, xph;
  double   hlamb, err, sk, hnew, ydiff, bspl;
  double   stnum, stden, sqr, err2, erri, deno;
  double   h = 0.0;
  int      iasti, iord, reject, last, nonsti=0;
  unsigned i;
  double   c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c14, c15, c16;
  double   b1, b6, b7, b8, b9, b10, b11, b12, bhh1, bhh2, bhh3;
  double   er1, er6, er7, er8, er9, er10, er11, er12;
  double   a21, a31, a32, a41, a43, a51, a53, a54, a61, a64, a65, a71, a74, a75, a76;
  double   a81, a84, a85, a86, a87, a91, a94, a95, a96, a97, a98;
  double   a101, a104, a105, a106, a107, a108, a109;
  double   a111, a114, a115, a116, a117, a118, a119, a1110;
  double   a121, a124, a125, a126, a127, a128, a129, a1210, a1211;
  double   a141, a147, a148, a149, a1410, a1411, a1412, a1413;
  double   a151, a156, a157, a158, a1511, a1512, a1513, a1514;
  double   a161, a166, a167, a168, a169, a1613, a1614, a1615;
  double   d41, d46, d47, d48, d49, d410, d411, d412, d413, d414, d415, d416;
  double   d51, d56, d57, d58, d59, d510, d511, d512, d513, d514, d515, d516;
  double   d61, d66, d67, d68, d69, d610, d611, d612, d613, d614, d615, d616;
  double   d71, d76, d77, d78, d79, d710, d711, d712, d713, d714, d715, d716;

  /* initialisations */
      c2  = 0.526001519587677318785587544488E-01;
      c3  = 0.789002279381515978178381316732E-01;
      c4  = 0.118350341907227396726757197510E+00;
      c5  = 0.281649658092772603273242802490E+00;
      c6  = 0.333333333333333333333333333333E+00;
      c7  = 0.25E+00;
      c8  = 0.307692307692307692307692307692E+00;
      c9  = 0.651282051282051282051282051282E+00;
      c10 = 0.6E+00;
      c11 = 0.857142857142857142857142857142E+00;
      c14 = 0.1E+00;
      c15 = 0.2E+00;
      c16 = 0.777777777777777777777777777778E+00;

      b1 =   5.42937341165687622380535766363E-2;
      b6 =   4.45031289275240888144113950566E0;
      b7 =   1.89151789931450038304281599044E0;
      b8 =  -5.8012039600105847814672114227E0;
      b9 =   3.1116436695781989440891606237E-1;
      b10 = -1.52160949662516078556178806805E-1;
      b11 =  2.01365400804030348374776537501E-1;
      b12 =  4.47106157277725905176885569043E-2;

      bhh1 = 0.244094488188976377952755905512E+00;
      bhh2 = 0.733846688281611857341361741547E+00;
      bhh3 = 0.220588235294117647058823529412E-01;

      er1  =  0.1312004499419488073250102996E-01;
      er6  = -0.1225156446376204440720569753E+01;
      er7  = -0.4957589496572501915214079952E+00;
      er8  =  0.1664377182454986536961530415E+01;
      er9  = -0.3503288487499736816886487290E+00;
      er10 =  0.3341791187130174790297318841E+00;
      er11 =  0.8192320648511571246570742613E-01;
      er12 = -0.2235530786388629525884427845E-01;

      a21 =    5.26001519587677318785587544488E-2;
      a31 =    1.97250569845378994544595329183E-2;
      a32 =    5.91751709536136983633785987549E-2;
      a41 =    2.95875854768068491816892993775E-2;
      a43 =    8.87627564304205475450678981324E-2;
      a51 =    2.41365134159266685502369798665E-1;
      a53 =   -8.84549479328286085344864962717E-1;
      a54 =    9.24834003261792003115737966543E-1;
      a61 =    3.7037037037037037037037037037E-2;
      a64 =    1.70828608729473871279604482173E-1;
      a65 =    1.25467687566822425016691814123E-1;
      a71 =    3.7109375E-2;
      a74 =    1.70252211019544039314978060272E-1;
      a75 =    6.02165389804559606850219397283E-2;
      a76 =   -1.7578125E-2;

      a81 =    3.70920001185047927108779319836E-2;
      a84 =    1.70383925712239993810214054705E-1;
      a85 =    1.07262030446373284651809199168E-1;
      a86 =   -1.53194377486244017527936158236E-2;
      a87 =    8.27378916381402288758473766002E-3;
      a91 =    6.24110958716075717114429577812E-1;
      a94 =   -3.36089262944694129406857109825E0;
      a95 =   -8.68219346841726006818189891453E-1;
      a96 =    2.75920996994467083049415600797E1;
      a97 =    2.01540675504778934086186788979E1;
      a98 =   -4.34898841810699588477366255144E1;
      a101 =   4.77662536438264365890433908527E-1;
      a104 =  -2.48811461997166764192642586468E0;
      a105 =  -5.90290826836842996371446475743E-1;
      a106 =   2.12300514481811942347288949897E1;
      a107 =   1.52792336328824235832596922938E1;
      a108 =  -3.32882109689848629194453265587E1;
      a109 =  -2.03312017085086261358222928593E-2;

      a111 =  -9.3714243008598732571704021658E-1;
      a114 =   5.18637242884406370830023853209E0;
      a115 =   1.09143734899672957818500254654E0;
      a116 =  -8.14978701074692612513997267357E0;
      a117 =  -1.85200656599969598641566180701E1;
      a118 =   2.27394870993505042818970056734E1;
      a119 =   2.49360555267965238987089396762E0;
      a1110 = -3.0467644718982195003823669022E0;
      a121 =   2.27331014751653820792359768449E0;
      a124 =  -1.05344954667372501984066689879E1;
      a125 =  -2.00087205822486249909675718444E0;
      a126 =  -1.79589318631187989172765950534E1;
      a127 =   2.79488845294199600508499808837E1;
      a128 =  -2.85899827713502369474065508674E0;
      a129 =  -8.87285693353062954433549289258E0;
      a1210 =  1.23605671757943030647266201528E1;
      a1211 =  6.43392746015763530355970484046E-1;

      a141 =  5.61675022830479523392909219681E-2;
      a147 =  2.53500210216624811088794765333E-1;
      a148 = -2.46239037470802489917441475441E-1;
      a149 = -1.24191423263816360469010140626E-1;
      a1410 =  1.5329179827876569731206322685E-1;
      a1411 =  8.20105229563468988491666602057E-3;
      a1412 =  7.56789766054569976138603589584E-3;
      a1413 = -8.298E-3;

      a151 =  3.18346481635021405060768473261E-2;
      a156 =  2.83009096723667755288322961402E-2;
      a157 =  5.35419883074385676223797384372E-2;
      a158 = -5.49237485713909884646569340306E-2;
      a1511 = -1.08347328697249322858509316994E-4;
      a1512 =  3.82571090835658412954920192323E-4;
      a1513 = -3.40465008687404560802977114492E-4;
      a1514 =  1.41312443674632500278074618366E-1;
      a161 = -4.28896301583791923408573538692E-1;
      a166 = -4.69762141536116384314449447206E0;
      a167 =  7.68342119606259904184240953878E0;
      a168 =  4.06898981839711007970213554331E0;
      a169 =  3.56727187455281109270669543021E-1;
      a1613 = -1.39902416515901462129418009734E-3;
      a1614 =  2.9475147891527723389556272149E0;
      a1615 = -9.15095847217987001081870187138E0;

      d41  = -0.84289382761090128651353491142E+01;
      d46  =  0.56671495351937776962531783590E+00;
      d47  = -0.30689499459498916912797304727E+01;
      d48  =  0.23846676565120698287728149680E+01;
      d49  =  0.21170345824450282767155149946E+01;
      d410 = -0.87139158377797299206789907490E+00;
      d411 =  0.22404374302607882758541771650E+01;
      d412 =  0.63157877876946881815570249290E+00;
      d413 = -0.88990336451333310820698117400E-01;
      d414 =  0.18148505520854727256656404962E+02;
      d415 = -0.91946323924783554000451984436E+01;
      d416 = -0.44360363875948939664310572000E+01;

      d51  =  0.10427508642579134603413151009E+02;
      d56  =  0.24228349177525818288430175319E+03;
      d57  =  0.16520045171727028198505394887E+03;
      d58  = -0.37454675472269020279518312152E+03;
      d59  = -0.22113666853125306036270938578E+02;
      d510 =  0.77334326684722638389603898808E+01;
      d511 = -0.30674084731089398182061213626E+02;
      d512 = -0.93321305264302278729567221706E+01;
      d513 =  0.15697238121770843886131091075E+02;
      d514 = -0.31139403219565177677282850411E+02;
      d515 = -0.93529243588444783865713862664E+01;
      d516 =  0.35816841486394083752465898540E+02;

      d61 =  0.19985053242002433820987653617E+02;
      d66 = -0.38703730874935176555105901742E+03;
      d67 = -0.18917813819516756882830838328E+03;
      d68 =  0.52780815920542364900561016686E+03;
      d69 = -0.11573902539959630126141871134E+02;
      d610 =  0.68812326946963000169666922661E+01;
      d611 = -0.10006050966910838403183860980E+01;
      d612 =  0.77771377980534432092869265740E+00;
      d613 = -0.27782057523535084065932004339E+01;
      d614 = -0.60196695231264120758267380846E+02;
      d615 =  0.84320405506677161018159903784E+02;
      d616 =  0.11992291136182789328035130030E+02;

      d71  = -0.25693933462703749003312586129E+02;
      d76  = -0.15418974869023643374053993627E+03;
      d77  = -0.23152937917604549567536039109E+03;
      d78  =  0.35763911791061412378285349910E+03;
      d79  =  0.93405324183624310003907691704E+02;
      d710 = -0.37458323136451633156875139351E+02;
      d711 =  0.10409964950896230045147246184E+03;
      d712 =  0.29840293426660503123344363579E+02;
      d713 = -0.43533456590011143754432175058E+02;
      d714 =  0.96324553959188282948394950600E+02;
      d715 = -0.39177261675615439165231486172E+02;
      d716 = -0.14972683625798562581422125276E+03;

  facold = r.resume ? r.facold : 1.0E-4;
  expo1 = 1.0/8.0 - beta * 0.2;
  facc1 = 1.0 / fac1;
  facc2 = 1.0 / fac2;
  posneg = xpp::sign (1.0, xend-x);

  /* initial preparations */
  last  = 0;
  hlamb = r.resume ? r.hlamb : 0.0;
  iasti = r.resume ? r.iasti : 0;
  nonsti = r.resume ? r.nonsti : 0;
  hmax = fabs (hmax);
  iord = 8;
  h = dp_first_step (s, n, x, y, posneg, iord, hmax, atoli, rtoli, r);
  reject = 0;
  r.xold = x;

  /* basic integration step */
  while (1)
  {
    if (const int failed = dp_before_attempt (r, x, h, xend, posneg, last))
      return failed;

    /* the twelve stages */
    for (i = 0; i < n; i++)
      r.yy1[i] = y[i] + h * a21 * r.k1[i];
    dprhs (s, n, x+c2*h, r.yy1, r.k2);
    for (i = 0; i < n; i++)
      r.yy1[i] = y[i] + h * (a31*r.k1[i] + a32*r.k2[i]);
    dprhs (s, n, x+c3*h, r.yy1, r.k3);
    for (i = 0; i < n; i++)
      r.yy1[i] = y[i] + h * (a41*r.k1[i] + a43*r.k3[i]);
    dprhs (s, n, x+c4*h, r.yy1, r.k4);
    for (i = 0; i <n; i++)
      r.yy1[i] = y[i] + h * (a51*r.k1[i] + a53*r.k3[i] + a54*r.k4[i]);
    dprhs (s, n, x+c5*h, r.yy1, r.k5);
    for (i = 0; i < n; i++)
      r.yy1[i] = y[i] + h * (a61*r.k1[i] + a64*r.k4[i] + a65*r.k5[i]);
    dprhs (s, n, x+c6*h, r.yy1, r.k6);
    for (i = 0; i < n; i++)
      r.yy1[i] = y[i] + h * (a71*r.k1[i] + a74*r.k4[i] + a75*r.k5[i] + a76*r.k6[i]);
    dprhs (s, n, x+c7*h, r.yy1, r.k7);
    for (i = 0; i < n; i++)
      r.yy1[i] = y[i] + h * (a81*r.k1[i] + a84*r.k4[i] + a85*r.k5[i] + a86*r.k6[i] +
			  a87*r.k7[i]);
    dprhs (s, n, x+c8*h, r.yy1, r.k8);
    for (i = 0; i <n; i++)
      r.yy1[i] = y[i] + h * (a91*r.k1[i] + a94*r.k4[i] + a95*r.k5[i] + a96*r.k6[i] +
			  a97*r.k7[i] + a98*r.k8[i]);
    dprhs (s, n, x+c9*h, r.yy1, r.k9);
    for (i = 0; i < n; i++)
      r.yy1[i] = y[i] + h * (a101*r.k1[i] + a104*r.k4[i] + a105*r.k5[i] + a106*r.k6[i] +
			  a107*r.k7[i] + a108*r.k8[i] + a109*r.k9[i]);
    dprhs (s, n, x+c10*h, r.yy1, r.k10);
    for (i = 0; i < n; i++)
      r.yy1[i] = y[i] + h * (a111*r.k1[i] + a114*r.k4[i] + a115*r.k5[i] + a116*r.k6[i] +
			  a117*r.k7[i] + a118*r.k8[i] + a119*r.k9[i] + a1110*r.k10[i]);
    dprhs (s, n, x+c11*h, r.yy1, r.k2);
    xph = x + h;
    for (i = 0; i < n; i++)
      r.yy1[i] = y[i] + h * (a121*r.k1[i] + a124*r.k4[i] + a125*r.k5[i] + a126*r.k6[i] +
			  a127*r.k7[i] + a128*r.k8[i] + a129*r.k9[i] +
			  a1210*r.k10[i] + a1211*r.k2[i]);
    dprhs (s, n, xph, r.yy1, r.k3);
    for (i = 0; i < n; i++)
    {
      r.k4[i] = b1*r.k1[i] + b6*r.k6[i] + b7*r.k7[i] + b8*r.k8[i] + b9*r.k9[i] +
	      b10*r.k10[i] + b11*r.k2[i] + b12*r.k3[i];
      r.k5[i] = y[i] + h * r.k4[i];
    }
     
    /* error estimation */
    err = 0.0;
    err2 = 0.0;
    for (i = 0; i < n; i++)
      {
	sk = atoli + rtoli * std::max(fabs(r.k5[i]), fabs(y[i]));
	erri = r.k4[i] - bhh1*r.k1[i] - bhh2*r.k9[i] - bhh3*r.k3[i];
	sqr = erri / sk;
	err2 += sqr*sqr;
	erri = er1*r.k1[i] + er6*r.k6[i] + er7*r.k7[i] + er8*r.k8[i] + er9*r.k9[i] +
	       er10 * r.k10[i] + er11*r.k2[i] + er12*r.k3[i];
	sqr = erri / sk;
	err += sqr*sqr;
      }
    deno = err + 0.01 * err2;
    if (deno <= 0.0)
      deno = 1.0;
    err = fabs(h) * err * sqrt (1.0 / (deno*static_cast<double>(n)));

    /* computation of hnew */
    fac11 = xpp::math::pow(err, expo1);
    /* Lund-stabilization */
    fac = fac11 / xpp::math::pow(facold,beta);
    /* we require fac1 <= hnew/h <= fac2 */
    fac = std::max(std::min(fac/safe, facc1), facc2);
    hnew = h / fac;

    if (err <= 1.0)
    {
      /* step accepted */

      facold = std::max(1.0E-4, err);
      r.naccpt++;
      dprhs (s, n, xph, r.k5, r.k4);
      
      /* stiffness detection */
      if (!(r.naccpt % DOPRI_NSTIFF) || (iasti > 0))
      {
	stnum = 0.0;
	stden = 0.0;
	for (i = 0; i < n; i++)
	{
	  sqr = r.k4[i] - r.k3[i];
	  stnum += sqr*sqr;
	  sqr = r.k5[i] - r.yy1[i];
	  stden += sqr*sqr;
	}
	if (stden > 0.0)
	  hlamb = h * sqrt (stnum / stden);
	if (hlamb > 6.1)
	{
	  nonsti = 0;
	  iasti++;
	  if (iasti == 15)
	  {
	    /* Hairer returns -4 here only when there is no message stream;
	       XPP always passed one (stdout), so it warns and goes on */
	    xpp::log(XPP_LOG_WARN, "The problem seems to become stiff at t = {:.16e}\n", x);
	  }
	}
	else
	{
	  nonsti++;
	  if (nonsti == 6)
	    iasti = 0;
	}
      }
       
      /* final preparation for dense output: save the first function evaluations */
      for (i = 0; i < n; i++)
	  {
	    r.rcont1[i] = y[i];
	    ydiff = r.k5[i] - y[i];
	    r.rcont2[i] = ydiff;
	    bspl = h * r.k1[i] - ydiff;
	    r.rcont3[i] = bspl;
	    r.rcont4[i] = ydiff - h*r.k4[i] - bspl;
	    r.rcont5[i] = d41*r.k1[i] + d46*r.k6[i] + d47*r.k7[i] + d48*r.k8[i] +
			d49*r.k9[i] + d410*r.k10[i] + d411*r.k2[i] + d412*r.k3[i];
	    r.rcont6[i] = d51*r.k1[i] + d56*r.k6[i] + d57*r.k7[i] + d58*r.k8[i] +
			d59*r.k9[i] + d510*r.k10[i] + d511*r.k2[i] + d512*r.k3[i];
	    r.rcont7[i] = d61*r.k1[i] + d66*r.k6[i] + d67*r.k7[i] + d68*r.k8[i] +
			d69*r.k9[i] + d610*r.k10[i] + d611*r.k2[i] + d612*r.k3[i];
	    r.rcont8[i] = d71*r.k1[i] + d76*r.k6[i] + d77*r.k7[i] + d78*r.k8[i] +
			d79*r.k9[i] + d710*r.k10[i] + d711*r.k2[i] + d712*r.k3[i];
	  }

	/* the next three function evaluations */
	for (i = 0; i < n; i++)
	  r.yy1[i] = y[i] + h * (a141*r.k1[i] + a147*r.k7[i] + a148*r.k8[i] +
			      a149*r.k9[i] + a1410*r.k10[i] + a1411*r.k2[i] +
			      a1412*r.k3[i] + a1413*r.k4[i]);
	dprhs (s, n, x+c14*h, r.yy1, r.k10);
	for (i = 0; i < n; i++)
	  r.yy1[i] = y[i] + h * (a151*r.k1[i] + a156*r.k6[i] + a157*r.k7[i] + a158*r.k8[i] +
			      a1511*r.k2[i] + a1512*r.k3[i] + a1513*r.k4[i] +
			      a1514*r.k10[i]);
	dprhs (s, n, x+c15*h, r.yy1, r.k2);
	for (i = 0; i < n; i++)
	  r.yy1[i] = y[i] + h * (a161*r.k1[i] + a166*r.k6[i] + a167*r.k7[i] + a168*r.k8[i] +
			      a169*r.k9[i] + a1613*r.k4[i] + a1614*r.k10[i] +
			      a1615*r.k2[i]);
	dprhs (s, n, x+c16*h, r.yy1, r.k3);

	/* final preparation */
	for (i = 0; i < n; i++)
	  {
	    r.rcont5[i] = h * (r.rcont5[i] + d413*r.k4[i] + d414*r.k10[i] +
			     d415*r.k2[i] + d416*r.k3[i]);
	    r.rcont6[i] = h * (r.rcont6[i] + d513*r.k4[i] + d514*r.k10[i] +
			     d515*r.k2[i] + d516*r.k3[i]);
	    r.rcont7[i] = h * (r.rcont7[i] + d613*r.k4[i] + d614*r.k10[i] +
			     d615*r.k2[i] + d616*r.k3[i]);
	    r.rcont8[i] = h * (r.rcont8[i] + d713*r.k4[i] + d714*r.k10[i] +
			     d715*r.k2[i] + d716*r.k3[i]);
	  }

      memcpy (r.k1, r.k4, n * sizeof(double)); 
      memcpy (y, r.k5, n * sizeof(double));
      r.xold = x;
      x = xph;

      return dp_accepted (r, x, h, hnew, hmax, posneg, last, reject, facold, hlamb, iasti, nonsti);
    }

    /* step rejected */
    h = h / std::min(fac11/safe, facc1);
    reject = 1;
    if (r.naccpt >= 1)
      r.nrejct=r.nrejct + 1;
    last = 0;
  }

} /* dopcor */

/* core integrator */
static int dopcor5 (xpp::Session &s, unsigned n, double x, double* y, double xend,
		   double hmax, double rtoli, double atoli, DpRun &r)
{
  constexpr double safe = DOPRI_SAFE, fac1 = DOPRI5_FAC1, fac2 = DOPRI5_FAC2, beta = DOPRI5_BETA;
  double   facold, expo1, fac, facc1, facc2, fac11, posneg, xph;
  double   hlamb, err, sk, hnew, yd0, ydiff, bspl;
  double   stnum, stden, sqr;
  double   h = 0.0;
  int      iasti, iord, reject, last, nonsti=0;
  unsigned i;
  double   c2, c3, c4, c5, e1, e3, e4, e5, e6, e7, d1, d3, d4, d5, d6, d7;
  double   a21, a31, a32, a41, a42, a43, a51, a52, a53, a54;
  double   a61, a62, a63, a64, a65, a71, a73, a74, a75, a76;

  /* initialisations */
      c2=0.2, c3=0.3, c4=0.8, c5=8.0/9.0;
      a21=0.2, a31=3.0/40.0, a32=9.0/40.0;
      a41=44.0/45.0, a42=-56.0/15.0; a43=32.0/9.0;
      a51=19372.0/6561.0, a52=-25360.0/2187.0;
      a53=64448.0/6561.0, a54=-212.0/729.0;
      a61=9017.0/3168.0, a62=-355.0/33.0, a63=46732.0/5247.0;
      a64=49.0/176.0, a65=-5103.0/18656.0;
      a71=35.0/384.0, a73=500.0/1113.0, a74=125.0/192.0;
      a75=-2187.0/6784.0, a76=11.0/84.0;
      e1=71.0/57600.0, e3=-71.0/16695.0, e4=71.0/1920.0;
      e5=-17253.0/339200.0, e6=22.0/525.0, e7=-1.0/40.0;
      d1=-12715105075.0/11282082432.0, d3=87487479700.0/32700410799.0;
      d4=-10690763975.0/1880347072.0, d5=701980252875.0/199316789632.0;
      d6=-1453857185.0/822651844.0, d7=69997945.0/29380423.0;

  facold = r.resume ? r.facold : 1.0E-4;
  expo1 = 0.2 - beta * 0.75;
  facc1 = 1.0 / fac1;
  facc2 = 1.0 / fac2;
  posneg = xpp::sign (1.0, xend-x);

  /* initial preparations */
  last  = 0;
  hlamb = r.resume ? r.hlamb : 0.0;
  iasti = r.resume ? r.iasti : 0;
  nonsti = r.resume ? r.nonsti : 0;
  hmax = fabs (hmax);
  iord = 5;
  h = dp_first_step (s, n, x, y, posneg, iord, hmax, atoli, rtoli, r);
  reject = 0;
  r.xold = x;

  /* basic integration step */
  while (1)
  {
    if (const int failed = dp_before_attempt (r, x, h, xend, posneg, last))
      return failed;

    /* the first 6 stages */
    for (i = 0; i < n; i++)
      r.yy1[i] = y[i] + h * a21 * r.k1[i];
    dprhs (s, n, x+c2*h, r.yy1, r.k2);
    for (i = 0; i < n; i++)
      r.yy1[i] = y[i] + h * (a31*r.k1[i] + a32*r.k2[i]);
    dprhs (s, n, x+c3*h, r.yy1, r.k3);
    for (i = 0; i < n; i++)
      r.yy1[i] = y[i] + h * (a41*r.k1[i] + a42*r.k2[i] + a43*r.k3[i]);
    dprhs (s, n, x+c4*h, r.yy1, r.k4);
    for (i = 0; i <n; i++)
      r.yy1[i] = y[i] + h * (a51*r.k1[i] + a52*r.k2[i] + a53*r.k3[i] + a54*r.k4[i]);
    dprhs (s, n, x+c5*h, r.yy1, r.k5);
    for (i = 0; i < n; i++)
      r.ysti[i] = y[i] + h * (a61*r.k1[i] + a62*r.k2[i] + a63*r.k3[i] + a64*r.k4[i] + a65*r.k5[i]);
    xph = x + h;
    dprhs (s, n, xph, r.ysti, r.k6);
    for (i = 0; i < n; i++)
      r.yy1[i] = y[i] + h * (a71*r.k1[i] + a73*r.k3[i] + a74*r.k4[i] + a75*r.k5[i] + a76*r.k6[i]);
    dprhs (s, n, xph, r.yy1, r.k2);
    for (i = 0; i < n; i++)
	{
	  r.rcont5[i] = h * (d1*r.k1[i] + d3*r.k3[i] + d4*r.k4[i] + d5*r.k5[i] + d6*r.k6[i] + d7*r.k2[i]);
	}
    for (i = 0; i < n; i++)
      r.k4[i] = h * (e1*r.k1[i] + e3*r.k3[i] + e4*r.k4[i] + e5*r.k5[i] + e6*r.k6[i] + e7*r.k2[i]);

    /* error estimation */
    err = 0.0;
    for (i = 0; i < n; i++)
      {
	sk = atoli + rtoli * std::max(fabs(r.yy1[i]), fabs(y[i]));
	sqr = r.k4[i] / sk;
	err += sqr*sqr;
      }
    err = sqrt (err / static_cast<double>(n));

    /* computation of hnew */
    fac11 = xpp::math::pow(err, expo1);
    /* Lund-stabilization */
    fac = fac11 / xpp::math::pow(facold,beta);
    /* we require fac1 <= hnew/h <= fac2 */
    fac = std::max(std::min(fac/safe, facc1), facc2);
    hnew = h / fac;

    if (err <= 1.0)
    {
      /* step accepted */

      facold = std::max(1.0E-4, err);
      r.naccpt++;

      /* stiffness detection */
      if (!(r.naccpt % DOPRI_NSTIFF) || (iasti > 0))
      {
	stnum = 0.0;
	stden = 0.0;
	for (i = 0; i < n; i++)
	{
	  sqr = r.k2[i] - r.k6[i];
	  stnum += sqr*sqr;
	  sqr = r.yy1[i] - r.ysti[i];
	  stden += sqr*sqr;
	}
	if (stden > 0.0)
	  hlamb = h * sqrt (stnum / stden);
	if (hlamb > 3.25)
	{
	  nonsti = 0;
	  iasti++;
	  if (iasti == 15)
	  {
	    /* Hairer returns -4 here only when there is no message stream;
	       XPP always passed one (stdout), so it warns and goes on */
	    xpp::log(XPP_LOG_WARN, "The problem seems to become stiff at t = {:.16e}\n", x);
	  }
	}
	else
	{
	  nonsti++;
	  if (nonsti == 6)
	    iasti = 0;
	}
      }

      for (i = 0; i < n; i++)
	  {
	    yd0 = y[i];
	    ydiff = r.yy1[i] - yd0;
	    bspl = h * r.k1[i] - ydiff;
	    r.rcont1[i] = y[i];
	    r.rcont2[i] = ydiff;
	    r.rcont3[i] = bspl;
	    r.rcont4[i] = -h * r.k2[i] + ydiff - bspl;
	  }
      memcpy (r.k1, r.k2, n * sizeof(double)); 
      memcpy (y, r.yy1, n * sizeof(double));
      r.xold = x;
      x = xph;

      return dp_accepted (r, x, h, hnew, hmax, posneg, last, reject, facold, hlamb, iasti, nonsti);
    }

    /* step rejected */
    h = h / std::min(fac11/safe, facc1);
    reject = 1;
    if (r.naccpt >= 1)
      r.nrejct=r.nrejct + 1;
    last = 0;
  }

} /* dopcor5 */

/* One run of DOP853 (eighth) or DOPRI5 from x to xend, its vectors carved
   from work: the dense output coefficients (eight or five of n), then the
   stage vectors; returns dopcor's code */
static int dormand_prince(xpp::Session &s, unsigned n, double x, double *y, double xend, double rtol, double atol,
                          double hmax, bool eighth, double *work, DpRun &r)
{
  double **const rcont[] = {&r.rcont1, &r.rcont2, &r.rcont3, &r.rcont4, &r.rcont5, &r.rcont6, &r.rcont7, &r.rcont8};
  double **const stages[] = {&r.yy1, &r.k1, &r.k2, &r.k3, &r.k4, &r.k5, &r.k6, eighth ? &r.k7 : &r.ysti, &r.k8, &r.k9, &r.k10};
  const unsigned coefficients = eighth ? 8 : 5, stage_vectors = eighth ? 11 : 8;
  for (unsigned i = 0; i < 8; i++) *rcont[i] = i < coefficients ? work + i * n : nullptr;
  for (unsigned i = 0; i < 11; i++) *stages[i] = i < stage_vectors ? work + (coefficients + i) * n : nullptr;
  if (eighth) return dopcor (s, n, x, y, xend, hmax, rtol, atol, r);
  return dopcor5 (s, n, x, y, xend, hmax, rtol, atol, r);
}

namespace xpp {
namespace {
/* The solver owns both internal endpoints and the interpolant. A caller's y
   remains its requested sample; it never becomes the internal FSAL state. */
class DormandPrince final : public Solver {
  // DOP853 needs eight coefficient vectors and eleven stage vectors.
  static constexpr int work_vectors=19;
  // Hairer's 1.01*h endpoint test must not clip a step bounded by DtMax.
  static constexpr double horizon_steps=2;
public:
  DormandPrince(const SolverInfo &info, Session &s, int n)
    : Solver(info,s), work_(xpp::size_product(work_vectors,n)), endpoint_(n), returned_(n),
      scratch_(std::max(n,s.model().neq)), derivative_(scratch_.size()) {}
  void begin(int *start) override
  {
    if(*start==1){run_=DpRun{};valid_=false;}
  }
  void finish() override {valid_=false;run_.resume=false;}
  bool section_found() const override { return section_; }
private:
  Result<> step_by(const SolverStep &step) override
  {
    auto &s=session_;
    auto &num=s.numerics;
    section_=false;
    if(step.sections&&num.poincare_map!=PoincareMap::none&&num.poincare_variable>s.model().neq)
      return failed("A Poincare section requires a model variable, not an added data column");
    if(step.sections&&num.poincare_map==PoincareMap::max&&num.poincare_variable>s.model().node)
      return failed("Poincare extrema require a state variable with a right-hand side");
    const int n=step.neq;
    if(static_cast<std::size_t>(n)!=endpoint_.size())return failed("Solver state dimension changed");
    const double direction=step.tout>*step.t?1:-1;
    if(*step.start==1){run_=DpRun{};valid_=false;*step.start=0;}
    // Torus wrapping and any external state edit invalidate the polynomial.
    if(valid_&&(*step.t!=returned_time_||!std::equal(returned_.begin(),returned_.end(),step.y)||
                (internal_time_-*step.t)*direction<0))valid_=false;
    if(!valid_){
      std::copy_n(step.y,n,endpoint_.begin());
      internal_time_=*step.t;
      run_.resume=false;
    }
    while((*step.t-step.tout)*direction<0){
      if(xpp::job::cancelled()){s.integrator.stop_flag=1;break;}
      if(!valid_||(internal_time_-*step.t)*direction<=0){
        const double end=s.delay.flag?step.tout:internal_time_+direction*horizon_steps*fabs(num.max_step);
        const int status=dormand_prince(s,n,internal_time_,endpoint_.data(),end,num.relative_tolerance,num.absolute_tolerance,
                                        num.max_step,info().traits.eighth_order,work_.data(),run_);
        /* dopcor fails when its attempts run out (-2) or its step falls
           below the rounding limit (-3) */
        if(status==-2)
          return stopped(xpp::format("more than {} step attempts (accepted and rejected) in the run",DOPRI_RUN_ATTEMPTS),run_.xout,-1,
                         {"relative_tolerance","absolute_tolerance"});
        if(status<0)
          return stopped(xpp::format("the step size {} fell below the rounding limit",run_.failed_h),run_.xout,-1,
                         {"relative_tolerance","absolute_tolerance"});
        internal_time_=run_.xout;
        valid_=true;
        events_checked_=false;
        if(s.integrator.step_error)return std::unexpected(*s.integrator.step_error);
      }
      const double until=(internal_time_-step.tout)*direction<0?internal_time_:step.tout;
      const auto state=[&](double time,std::span<double> y){sample(time,y);};
      // Inspect the accepted step once, even when many output times lie in it.
      // A future event stays pending until the caller reaches its root.
      if(!events_checked_||event_sections_!=step.sections){
        event_=dense_event(s,*step.t,internal_time_,run_.xold,internal_time_,state,scratch_,derivative_,step.sections);
        events_checked_=true;event_sections_=step.sections;
      }
      const bool reached=event_.found&&(event_.time-until)*direction<=0;
      const DenseEvent event=reached?event_:DenseEvent{};
      *step.t=reached?event.time:until;
      if(reached)events_checked_=false;
      sample(*step.t,std::span(step.y,static_cast<std::size_t>(n)));
      if(event.bound_variable>=0&&event.flag_count==0&&!(event.section&&num.poincare_stop)){
        const auto &name=s.model().uvar_names[event.bound_variable];
        valid_=false;
        return failed(xpp::format("{} reaches state_limit = {} at t = {}",name,xpp::number(num.state_limit),*step.t),model_place(s.model(),name));
      }
      if(event.flag_count){
        apply_dense_flags(s,step.y,*step.t,n,std::span(event.flags.data(),static_cast<std::size_t>(event.flag_count)));
        // A parameter reset also invalidates FSAL, even if no state changed.
        valid_=false;run_.resume=false;
        internal_time_=*step.t;
        std::copy_n(step.y,n,endpoint_.begin());
      }
      if(auto checked=check_state(step.y,n,*step.t);!checked)return checked;
      section_=event.section;
      if(section_||s.integrator.stop_flag)break;
    }
    std::copy_n(step.y,n,returned_.begin());
    returned_time_=*step.t;
    set_fix_rhs(s,*step.t,step.y);
    // History is updated by integrate.cpp at the output grid, so the old
    // derivative no longer describes the next delay interval.
    if(s.delay.flag){valid_=false;run_.resume=false;}
    return {};
  }
  Result<> check_state(const double *y,int n,double time) const
  {
    for(int i=0;i<n;++i){
      const auto &s=session_;
      const bool bounded=s.numerics.stop_at_limit&&!(s.integrator.range_flag||s.integrator.suppress_bounds);
      if(!std::isfinite(y[i])||(bounded&&fabs(y[i])>s.numerics.state_limit)){
        const auto &name=s.model().uvar_names[i];
        return failed(xpp::format("{} is not finite or exceeds state_limit = {} at t = {}",name,xpp::number(s.numerics.state_limit),time),model_place(s.model(),name));
      }
    }
    return {};
  }
  void sample(double time,std::span<double> y) const
  {
    const double u=(time-run_.xold)/(internal_time_-run_.xold),v=1-u;
    for(std::size_t i=0;i<endpoint_.size();++i){
      if(time==internal_time_)y[i]=endpoint_[i];
      else if(info().traits.eighth_order)
        y[i]=run_.rcont1[i]+u*(run_.rcont2[i]+v*(run_.rcont3[i]+u*(run_.rcont4[i]+v*(run_.rcont5[i]+u*(run_.rcont6[i]+v*(run_.rcont7[i]+u*run_.rcont8[i]))))));
      else
        y[i]=run_.rcont1[i]+u*(run_.rcont2[i]+v*(run_.rcont3[i]+u*(run_.rcont4[i]+v*run_.rcont5[i])));
    }
  }
  DpRun run_;
  DenseEvent event_;
  std::vector<double> work_,endpoint_,returned_,scratch_,derivative_;
  double internal_time_=0,returned_time_=0;
  bool valid_=false,section_=false,events_checked_=false,event_sections_=false;
};
}
std::unique_ptr<Solver> make_dormand_prince(const SolverInfo &info, Session &s, int n)
{
  return std::make_unique<DormandPrince>(info,s,n-s.model().nmarkov);
}
}
