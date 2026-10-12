# W314 tried on a real model: the 6-compartment motoneuron

The maintainer asked to see the proposed components (W314, #373) on the model of their
paper: Moustafa et al. 2023, ModelDB 267710, the 6-compartment spinal motoneuron
(`paper/benchmarks/modeldb-267710/models/6cModel/`). Today it is `main.ode` and 20 `.inc`
files, 331 statement lines, 25 ODEs. Below is the same model written with components and
W309's named groups. It is a sketch to judge the design: none of this syntax exists yet, and
every point it decides is listed at the end.

How the original is built: six compartments (soma, axon hillock, dendrites d0 to d3), each a
membrane equation with a leak, linked by axial conductances; channels in four of them (fast
Na, Kdr and the soma's calcium set in the soma; fast Na, persistent Na and Kdr in the
hillock; the dendritic L-type plateau with K(Ca) in d2 and d3). The hillock's channels repeat
the soma's kinetics with other numbers; `dend2_CaDen.inc` and `dend3_CaDen.inc` are 60 lines
each and differ in three numbers.

## channels.odex: the channel types, written once

```
# Channels of the 6-compartment motoneuron (Moustafa et al. 2023, ModelDB 267710)
const temp = 36, vtraub = -10, vtraub2 = -70
const tadj = 3^((temp - 36)/10)

fun expmod(x) = if x < -100 then 0 else exp(x)
fun vtrap(x, y) = if abs(x/y) < 1e-6 then y*(1 - x/y/2) else x/(expmod(x/y) - 1)
fun vtrap1(x, a, b, c) = if abs((x + b)/c) < 1e-6 then a*c else a*(x + b)/(1 - exp(-(x + b)/c))
fun vtrap2(x, a, b, c) = if abs((x + b)/c) < 1e-6 then -a*c else -a*(x + b)/(1 - exp((x + b)/c))
fun taufunc(tail, w, df, wth) = 0.001 + 0.999/(1 + exp((tail*w*df - wth)/0.006))
fun dflag(x) = 1/(1 + expmod(5000*(x + 0.01)))

# Fast sodium (Traub); the hillock's kinetics are its own group
component NaF {
  input v
  par soma default { g: 0, e: 50, ama: 22, bma: 46, th: 40, tht: 47, kap: 7 }
  par hillock      { ama: 18, bma: 38 }
  init soma default { m: 0.00590903, h: 0.864964 }
  init hillock      { m: 0.0131252 }
  a = 0.4*vtrap(ama - (v - vtraub2), 5)
  b = 0.4*vtrap((v - vtraub2) - bma, 5)
  mt = 1/(a + b)/tadj
  ht = th*tadj/(expmod((v - vtraub + 50)/15) + expmod(-(v - vtraub + 50)/16))
  hf = 1/(1 + expmod((v - vtraub + tht)/kap))
  m' = (a/(a + b) - m)/mt
  h' = (hf - h)/ht
  i = g*m^3*h*(v - e)
}

# Delayed rectifier
component Kdr {
  input v
  par base default { g: 0, e: -80, tn: 5, tht: 20, kap: -15 }
  init rest default { n: 0.0649692 }
  nt = tn*tadj/(expmod((v - vtraub + 40)/40) + expmod(-(v - vtraub + 40)/50))
  n' = (1/(1 + expmod((v - vtraub + tht)/kap)) - n)/nt
  i = g*n^4*(v - e)
}

# Persistent sodium
component NaP {
  input v
  par base default { g: 0, e: 50, ama: 0.01, amb: 30.4, amc: 5, bma: 0.00025, bmb: 34.7, bmc: 5 }
  init rest default { m: 0.00214921 }
  const tadj1 = 2.2^((temp - 20)/10)
  a = vtrap1(v - 10, ama, amb, amc)
  b = vtrap2(v - 10, bma, bmb, bmc)
  mt = 1/(a + b)/tadj1
  m' = (a/(a + b) - m)/mt
  i = g*m^3*(v - e)
}

# The soma's calcium: N-type, L-type, Ca-activated K, and the calcium they bring in
component SomaCa {
  input v
  par base default { gn: 0.01, tmn: 15, thtmn: 14, thn: 50, ththn: 32,
                     gl: 0, mlexp: 1, tml: 400, thtml: 45.8, kml: -3.7,
                     gk: 0.0221, nexp: 1, kd: 0.0005, f: 0.01, alpha: 1, kca: 8,
                     e: 130, ek: -80 }
  init rest default { mn: 0.000101029, hn: 0.996316, ml: 0.0210859, ca: 0.0001 }
  mn' = (1/(1 + exp((v - vtraub + thtmn)/(-5))) - mn)/(tmn*tadj)
  hn' = (1/(1 + exp((v - vtraub + ththn)/5)) - hn)/(thn*tadj)
  ml' = (1/(1 + exp((v - vtraub + thtml)/kml)) - ml)/(tml*tadj)
  i_n = gn*mn^2*hn*(v - e)
  i_l = gl*ml^mlexp*(v - e)
  ca' = f*(-alpha*(i_n + i_l) - kca*ca)
  i = i_n + i_l + gk*ca^nexp/(ca^nexp + kd)*(v - ek)
}

# The dendrites' L-type plateau with Ca-activated K
component CaDen {
  input v
  par base default { g: 0, gk: 0, e: 60, thtm: -43, kapm: -6, ot: 10, ot2: 50,
                     wt_d: 1200, tilon: 1, arang: 0.5, wg: 1, wth: 0.27,
                     nexp: 10, kd: 0.0005, st: 40, f: 0.01, alpha: 1, kca: 8, ek: -80 }
  init rest default { o: 0.0109869, w: 8.76425e-8, s: 0, ca: 0.0001 }
  of = 1/(1 + expmod((v - thtm)/kapm))
  df = dflag(of - o)
  wt = 50 + (1150/wg)/(1 + expmod((v + 32)/7))
  o' = (of - o)*taufunc(tilon, w, df, wth)/(ot + ot2*df)
  w' = (1/(1 + expmod(-(v + 57)/0.8))*(1 - df) - w)/(wt + (wt_d - wt)*df)
  s' = (1/((kd/(ca - 0.00001))^nexp + 1) - s)/st
  i_ca = g*o*(arang*0.3 + 1)*(v - e)
  ca' = f*(-alpha*i_ca - kca*ca)
  i = i_ca + gk*s*(v - ek)
}
```

## compartment.odex: a membrane patch

```
const cm = 0.001

component Compartment {
  input i_ion = 0        # the channels' current densities, summed with +=
  input i_axial = 0      # the currents from the neighbours, summed with +=
  input i_inj = 0        # injected current
  par base default { len: required, diam: required, gpas: required, epas: -70 }
  init rest default { v: -70 }
  area = pi*len*diam
  cross = 0.25*pi*diam^2
  v' = ((i_inj + i_axial)/area - gpas*(v - epas) - i_ion)/cm
}
```

## motoneuron.odex: the cell

```
# Moustafa et al. 2023, ModelDB 267710: soma, axon hillock, four dendrites
import "compartment.odex"
import "channels.odex"

const ra = 70, max_amp = 50, cad_max = 0.000168

par rest default { amp: 0 }      # ampPar, the stimulus: AUTO's parameter
par fire         { amp: 0.3 }    # was glob_fire.inc, a copy of glob.inc

Compartment soma(len: 48.8e-4,         diam: 48.8e-4,         gpas: 1/225,
                 i_inj: amp*max_amp*1e-6)
Compartment ah  (len: 20e-4,           diam: 12.94999838e-4,  gpas: 1/225)
Compartment d0  (len: 1450.97e-4,      diam: 42.2188e-4,      gpas: 1/11000)
Compartment d1  (len: 1450.97e-4,      diam: 42.2188e-4,      gpas: 1/11000)
Compartment d2  (len: 754.842*1.81e-4, diam: 36.4297e-4,      gpas: 1/11000)
Compartment d3  (len: 1037.91*1.81e-4, diam: 10.1901e-4,      gpas: 1/11000)

NaF    soma_na (g: 0.06,               v: soma.v)
Kdr    soma_kdr(g: 0.80048,            v: soma.v)
SomaCa soma_ca (                       v: soma.v)
NaF    ah_na   (hillock, g: 1.3392,    v: ah.v)
NaP    ah_nap  (g: 3.2971e-5,          v: ah.v)
Kdr    ah_kdr  (g: 0.16552,            v: ah.v)
CaDen  d2_ca   (g: cad_max,      gk: 0.41*cad_max,      v: d2.v)
CaDen  d3_ca   (g: 0.25*cad_max, gk: 0.25*0.41*cad_max, v: d3.v)

soma.i_ion += soma_na.i + soma_kdr.i + soma_ca.i
ah.i_ion   += ah_na.i + ah_nap.i + ah_kdr.i
d2.i_ion   += d2_ca.i
d3.i_ion   += d3_ca.i

# Axial conductances from half of each compartment's length; the soma to d0
# link counts d0's half only, as published
fun g_axial(l1, c1, l2, c2) = 1/(ra*(0.5*l1/c1 + 0.5*l2/c2))
g_ah = g_axial(soma.len, soma.cross, ah.len, ah.cross)
g_d0 = 1/(ra*0.5*d0.len/d0.cross)
g_d[j] = g_axial(d[j-1].len, d[j-1].cross, d[j].len, d[j].cross)  for j in 1..3

soma.i_axial += g_ah*(ah.v - soma.v) + g_d0*(d0.v - soma.v)
ah.i_axial   += g_ah*(soma.v - ah.v)
d0.i_axial   += g_d0*(soma.v - d0.v)
d[j-1].i_axial += g_d[j]*(d[j].v - d[j-1].v)  for j in 1..3
d[j].i_axial   += g_d[j]*(d[j-1].v - d[j].v)  for j in 1..3

options paper default {
  integration:  { method: backward_euler, dt: 0.025, total_time: 200 },
  plot:         { yp: soma.v, xhi: 200, ylo: -90, yhi: 50 },
  continuation: { ntst: 100, dsmax: 0.1, nmax: 10000000, npr: 100000,
                  parmin: -10, parmax: 1 },
}
```

122 statement lines in three files, against 331 in 21. What changes for the reader:

- **One channel, one place.** NaF and Kdr are written once and used in the soma and the
  hillock; CaDen once for d2 and d3, which now differ in one visible line (`0.25*cad_max`).
- **Names are scoped.** The columns are `soma.v`, `ah_na.m`, `d2_ca.ca`. The original works
  only because XPPAUT ignores case: `ah_NapIsb1.inc` declares `NaP_mpf` and its equation
  reads `Nap_mpf`. Inside a component each name is the component's own.
- **The wiring is the model.** Which channel sits where, and which compartment touches which,
  are a dozen lines at the bottom of the cell, instead of `I...Comp` and `I...Ion` sums spread
  over the compartment files.
- **Conditions are overlays.** `rest` and `fire` were two copies of `glob.inc`; now they are
  two `par` groups, and a condition of a study is one line, e.g.
  `par no_plateau { d2_ca.g: 0, d3_ca.g: 0 }`.

## The cell in a network

The cell file is itself a component (a file with no `component` is one, named after the
file). Ten motoneurons with gap junctions between neighbouring somata, if the cell declares
`input i_gap = 0` and adds it (`soma.i_axial += i_gap`):

```
import "motoneuron.odex"

par net default { g_gap: 1e-5 }
motoneuron mn[j](fire)  for j in 1..10
mn[j].i_gap += g_gap*(mn[j-1].soma.v - mn[j].soma.v)  for j in 2..10
mn[j].i_gap += g_gap*(mn[j+1].soma.v - mn[j].soma.v)  for j in 1..9
```

250 ODEs; the columns are `mn3.soma.v`, `mn3.ah_na.m`.

## The continuation (the maintainer's note, 2026-10-12)

The paper's limit cycle comes from the rightmost Hopf point of the equilibria in `amp`,
continued with a negative ds. That is what W269's benchmark already runs, in xppautX
(`auto_xppautx.py`) and AUTO-07p (`auto07p/6c.auto`: `max(eq('HB'), key=PAR(1))`). It is a run,
not part of the model, so it is not in the files above.

## What the example decides (W314's questions)

| Question | What the example writes |
|---|---|
| Q1 instance syntax | `Compartment soma(...)`: type first |
| Q2 declaration | `component NAME { ... }`, several in one file; a file of components runs nothing |
| Q3 arguments | an optional group name first (`hillock`), then `name: value` for parameters and inputs |
| Q4 inputs | `input NAME = default`; given once at the instance line, or summed by `+=` lines, never both |
| Q5 the network | reads any `inst.name` (states, parameters, quantities); sets only inputs and parameters |
| Q6 arrays | `d[j-1].i_axial += ... for j in 1..3`, `motoneuron mn[j](fire) for j in 1..10` |
| Q7 names | dotted, nested: `mn3.soma.v` in columns, plots and the protocol |
| Q8 groups | instance parameters in the file's groups: `par no_plateau { d2_ca.g: 0 }` |
| Q9 contents | anything a model holds: states, quantities, groups, consts, other instances |
| Q10 flat files | a file with no component is one, named after the file |

## New questions the example raised

1. **Expressions in values.** `gpas: 1/225`, `len: 754.842*1.81e-4`, `g: 0.25*cad_max` read
   much better than their decimals, but `par` takes numbers only (W80). Proposed: instance
   arguments and group values may be constant expressions (numbers and consts, worked out at
   load), as a range's ends already are.
2. **Parameters every instance must give.** A compartment has no sensible default length.
   The sketch writes `len: required`; W309 says a default group is complete. Proposed: a
   component's default group may mark a parameter `required`, and an instance that leaves it
   out is an error at the instance line.
3. **What a component sees.** The channels read `tadj`, `vtraub` and `vtrap` from their file's
   top level. Proposed: a component sees its own file's consts and functions, never the
   importer's names, so it means the same wherever it is used.
4. **One group name for several kinds.** `NaF ah_na(hillock, ...)` picks `par hillock` and
   `init hillock` together. W309 left "same name across kinds" open (its question 8); the
   example needs the pair.
5. **Links.** Each axial link is three lines (a conductance and two `+=`). A `Link` component
   taking two compartments would be one, but needs instances as arguments; proposed for later,
   not in W314's first version.
6. **The numbers.** `(i_inj + i_axial)/area` rounds differently from the original's
   `I/sArea + 1/sArea*(...)`. Converting a flat model to components must keep its arithmetic
   to keep its md5; a model written fresh does not have that constraint.
