# W437: tracing the remaining reference differences

All production tolerances and the descriptions' default meshes are unchanged. These experiments diagnose differences; they are not substituted reference values.

## Upper R2 of forced_pp_pd

Both programs test `abs(EV(3))-1`: `core/auto_codim.cpp` and AUTO-07p `periodic.f90:2046-2053`. The imposed and remaining -1 multipliers coalesce into a complex pair. At the default labelled points, the numerical tests are about +2.03e-6 and -1.00e-5. Their parameter discrepancy is sensitive to location history and finite-precision Floquet evaluation, not a different phase equation.

| Experiment | NTST | EPSS | xppautX par2 | AUTO-07p par2 | Delta |
|---|---:|---:|---:|---:|---:|
| base | 40 | 1e-05 | 2.57022297458 | 2.5702456957 | -2.27211e-05 |
| loc7 | 40 | 1e-07 | no R2 label | 2.5702456957 | - |
| mesh80 | 80 | 1e-05 | no R2 label | 2.5703174272 | - |
| mesh160 | 160 | 1e-05 | 2.57022664392 | 2.5701986843 | 2.79596e-05 |
| smallstep | 40 | 1e-05 | 2.5702150219 | 2.5702265172 | -1.14953e-05 |

At EPSS 1e-7, branch 3 point 205 exhausts eight location iterations and reports `NOTE:Possible special point`. The SPB tests oscillate through 7.15e-6, 3.60e-5, 4.17e-5, -5.46e-5, -7.96e-5, 2.03e-6, 7.41e-6 and 2.11e-5 as the nearly double multipliers alternate between real and complex. The failed search restores the converged pre-location state. Thus tightening location accuracy does not remove the Floquet-test noise.

The no-label rows in the JSON retain a nearby ordinary point for diagnostics; its distance is not treated as an R2 location error. The lower R2 and both complete curves remain compared. Only its par2 is recorded explicitly as a traced difference, with the original tolerance unchanged.

## Mu26: saved-orbit mesh and derivative controls

For each condition, execute the committed steady, cycles and marked to_snic runs at one thread and save the session. Grab each UZ at 1, 0.1 and 0.07 pA. Restart for only three stored points with NTST 50, 100, 200, 400, NCOL 4, DS=DSMAX=1e-6, DSMIN=1e-10, NPR 1, no marks, and the original Newton/location tolerances. AUTO-07p restarts its own corresponding saved UZ with the same settings. The final corrected point is measured. Mesh refinement can move the free parameter; raw parameters, norms, periods and all multipliers are retained in the JSON.

| Condition | Current | NTST 50 spectrum delta | 100 | 200 | 400 |
|---|---:|---:|---:|---:|---:|
| ode_mu26_control | 1 | 0.00014982 | 5.33724e-06 | 6.46108e-06 | 1.55992e-06 |
| ode_mu26_control | 0.1 | 0.000323902 | 1.87742e-05 | 2.31281e-06 | 3.82341e-05 |
| ode_mu26_control | 0.07 | 0.000523773 | 1.38448e-05 | 1.17492e-05 | 0.000181345 |
| ode_mu26_blocked | 1 | 6.79955e-05 | 1.56273e-05 | 8.87541e-06 | 2.55198e-05 |
| ode_mu26_blocked | 0.1 | 9.93992e-06 | 0.000240987 | 0.000303597 | 0.000222249 |
| ode_mu26_blocked | 0.07 | 1.3823e-06 | 0.000282254 | 4.48741e-06 | 7.19955e-05 |

The mesh dependence is nonmonotonic; refining alone is not proof of convergence. The dominant discrepancy is in the neutral multiplier, which is mathematically 1 for a nonconstant autonomous periodic orbit. It is not a claim that every multiplier is close to 1.

Changing only xppautX's derivative mode on the same NTST 200 restarts isolates the sensitivity to the finite-difference Jacobian. AUTO-07p retains its original numerical Jacobian. The orbit itself barely changes:

| Condition | Current | Neutral, difference | Neutral, exact | Norm change | Period change |
|---|---:|---:|---:|---:|---:|
| ode_mu26_control | 1 | 1.00003353892 | 1.0000000011 | -1.42e-14 | -2.32e-11 |
| ode_mu26_control | 0.1 | 1.00003231281 | 0.999999996401 | -4.01e-11 | 1.06e-08 |
| ode_mu26_control | 0.07 | 1.00003825078 | 0.999999999967 | 9.73e-11 | -2.99e-08 |
| ode_mu26_blocked | 1 | 1.00007887541 | 1.00000000156 | -4.26e-14 | -8.58e-11 |
| ode_mu26_blocked | 0.1 | 1.00023640335 | 0.999999999731 | 5.23e-12 | -2.22e-09 |
| ode_mu26_blocked | 0.07 | 1.00497448741 | 1.00000003469 | 1.7e-10 | -3.11e-07 |

The long-period variational problem amplifies the finite-difference perturbation; the coarse-mesh results add discretization/history error. Neither the default FD setting nor the mesh is changed to obtain a pass. The five default UZ points with norm/spectrum failures have quantity-specific traced exceptions; all eight target parameter locations pass.

## Blocked norm at exactly the 0.07 pA mark

To avoid comparing different free parameters after mesh refinement, grab the saved 0.1 pA UZ and relocate a new 0.07 mark. Use NTST 50/100/200, NCOL 4, DS .1, DSMAX .4, DSMIN 1e-10, NPR 1, NMX 100, RL0 .069; otherwise original settings. Both programs stop beyond the target before the SNIC. These runs require the UZ label and compare that point, not the endpoint.

| NTST | xppautX norm | AUTO-07p norm | Difference |
|---:|---:|---:|---:|
| 50 | 100.73440318 | 100.73440241 | 7.70256e-07 |
| 100 | 100.734404545 | 100.73440454 | 4.87718e-09 |
| 200 | 100.734404548 | 100.73440455 | -2.22303e-09 |

The original NTST 50 forward history differs by 8.34e-5. Re-location with a nearby seed already reduces it to 7.70e-7, and NTST 100/200 yields differences of a few e-9. This identifies adaptive mesh/history resolution at the coarse orbit, rather than an incorrect mark value. Spectral convergence is separate: matching norms do not make the FD neutral multiplier accurate.

## tstauto_pp2

The four obsolete AUTO-07p LP exceptions are removed. Both LPs are now compared directly; 30 reference quantities pass from the committed diagram. The first LP's period 79.89357756 agrees with AUTO-07p 79.89348504, while XPPAUT retains its uncentered phase condition and reports 64.656143027. The expected XPPAUT norm/period difference is traced by quantity explicitly (finding 73); its second missing LP retains the existing W326 explanation. These NTST 15 folds remain underresolved, not established physical bifurcations.


## Native XPPAUT and comparison controls

The committed raw JSON retains both marked native `to_snic` point summaries; their full spectra remain in the committed `.xppaut` references. Control
misses 1 pA and reverses through 5 pA; blocked misses 0.1/0.07 pA and labels two
LPs before reversing. The control rerun with unconditional diagnostic slicing is
byte-identical to the corrected captured prefix. Every other run's point and
curve records are unchanged in both references. Both blocked LPC runs hit the
600 s guard and recover as unavailable, retaining W436's more detailed reasons.

Four quantity-trace checks pass: the real trace data; a wrong UZ parameter still
fails despite a spectrum trace; bringing that spectrum within tolerance fails
as stale; an unknown quantity is rejected. Only the failed quantity is traced.
