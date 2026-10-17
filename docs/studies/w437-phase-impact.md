# W437 phase-condition impact before baseline updates

Full comparison of source `1bab3a6645`: `AUTO FAILED: 60 models, 9 skipped, 18674 points, 150 checks, 36 failures`. This was the state **before the update** approved on 2026-10-17.

The two committed Mu26 changes label all four requested user points and remain monotone toward the SNIC.
The table records the measured differences before the update; all sixteen were subsequently approved and adopted.

| Description | Points before / after | Changed paired rows | Max norm delta | Max period delta | Threads equal |
|---|---:|---:|---:|---:|---|
| forced_pp_ns | 264 / 262 | 205 | 0.00240022 | 2.62013e-14 | True |
| forced_pp_pd | 706 / 706 | 649 | 0.00526007 | 3.75255e-14 | True |
| hill_cycles_pd | 151 / 151 | 103 | 3.4861e-13 | 3.55271e-15 | True |
| hill_cycles_torus | 196 / 196 | 148 | 7.67053e-13 | 3.55271e-15 | True |
| ode_lecar | 79 / 79 | 29 | 6.94955e-07 | 0.00024097 | True |
| ode_ma23 | 613 / 613 | 290 | 6.51003e-06 | 0.00125482 | True |
| ode_motoneuron6c | 1239 / 1239 | 19 | 1.26329e-05 | 1.3651e-05 | True |
| res_cycles | 376 / 376 | 328 | 5.98905e-07 | 7.90479e-13 | True |
| tstauto_brper | 49 / 49 | 27 | 3.02891e-12 | 3.14593e-12 | True |
| tstauto_frcp | 103 / 103 | 29 | 1.73922e-10 | 3.60672e-09 | True |
| tstauto_lamom | 70 / 70 | 28 | 1.9984e-15 | 4.44089e-15 | True |
| tstauto_lamom2 | 60 / 60 | 28 | 5.41145e-12 | 2.1358e-11 | True |
| tstauto_lecar | 83 / 83 | 29 | 6.95434e-07 | 0.000241136 | True |
| tstauto_lo | 50 / 50 | 28 | 1.57652e-14 | 2.04281e-14 | True |
| tstauto_pp2 | 130 / 130 | 29 | 0.0132213 | 59.7399 | True |
| tstauto_torx | 81 / 81 | 28 | 9.54792e-15 | 3.01981e-14 | True |

Deltas compare rows with the same stored index. Adaptive step histories can differ, so these are change sizes, not physical error estimates. The existing independent-reference checks still apply.

The upward R2 of `forced_pp_pd` fails the existing AUTO-07p bound on the second parameter: old 2.570226584593134, new 2.5702229745790253, reference 2.5702456957. The new error is 2.27e-5 against 2.10e-5 allowed. No tolerance was widened.

Reference results:

- `PASS explicit_homoclinic against AUTO-07p (7 quantities)`
- `PASS forced_pp_ns against AUTO-07p and XPPAUT 8.0 (44 quantities)`
- `FAIL forced_pp_pd against AUTO-07p and XPPAUT 8.0 (40 quantities)`
- `PASS henon_rot_lp against AUTO-07p (33 quantities)`
- `PASS henon_rot_pd against AUTO-07p and XPPAUT 8.0 (58 quantities)`
- `PASS henon_rot_th against AUTO-07p and XPPAUT 8.0 (44 quantities)`
- `PASS henon_rot_tr against AUTO-07p and XPPAUT 8.0 (32 quantities)`
- `PASS hill_cycles_pd against AUTO-07p and XPPAUT 8.0 (62 quantities)`
- `PASS hill_cycles_torus against AUTO-07p and XPPAUT 8.0 (52 quantities)`
- `PASS izh_class1_integrator against AUTO-07p and XPPAUT 8.0 (10 quantities)`
- `PASS izh_class2 against AUTO-07p and XPPAUT 8.0 (18 quantities)`
- `PASS izh_resonator against AUTO-07p and XPPAUT 8.0 (18 quantities)`
- `PASS lorenz_homoclinic against AUTO-07p (7 quantities)`
- `PASS ode_del_log against AUTO-07p and XPPAUT 8.0 (4 quantities)`
- `PASS ode_lecar against AUTO-07p and XPPAUT 8.0 (24 quantities)`
- `PASS ode_ma23 against AUTO-07p and XPPAUT 8.0 (24 quantities)`
- `PASS ode_motoneuron6c against AUTO-07p and XPPAUT 8.0 (88 quantities)`
- `FAIL ode_mu26_blocked against AUTO-07p and XPPAUT 8.0 (76 quantities)`
- `FAIL ode_mu26_control against AUTO-07p and XPPAUT 8.0 (48 quantities)`
- `PASS ode_osc against AUTO-07p and XPPAUT 8.0 (4 quantities)`
- `PASS res_cycles against AUTO-07p and XPPAUT 8.0 (60 quantities)`
- `PASS tstauto_ab against AUTO-07p and XPPAUT 8.0 (10 quantities)`
- `PASS tstauto_ab_fold against AUTO-07p and XPPAUT 8.0 (24 quantities)`
- `PASS tstauto_ab_hopf against AUTO-07p and XPPAUT 8.0 (36 quantities)`
- `PASS tstauto_bigaut against AUTO-07p and XPPAUT 8.0 (4 quantities)`
- `PASS tstauto_bistable against AUTO-07p and XPPAUT 8.0 (4 quantities)`
- `PASS tstauto_bob against AUTO-07p and XPPAUT 8.0 (4 quantities)`
- `PASS tstauto_brper against AUTO-07p and XPPAUT 8.0 (8 quantities)`
- `PASS tstauto_dumb against AUTO-07p and XPPAUT 8.0 (4 quantities)`
- `PASS tstauto_dumbvp against AUTO-07p (2 quantities)`
- `PASS tstauto_eiz-hom against AUTO-07p and XPPAUT 8.0 (4 quantities)`
- `PASS tstauto_exp against AUTO-07p (2 quantities)`
- `PASS tstauto_frcp against AUTO-07p and XPPAUT 8.0 (12 quantities)`
- `PASS tstauto_lamom against AUTO-07p and XPPAUT 8.0 (12 quantities)`
- `PASS tstauto_lamom2 against AUTO-07p and XPPAUT 8.0 (20 quantities)`
- `PASS tstauto_lecar against AUTO-07p and XPPAUT 8.0 (24 quantities)`
- `PASS tstauto_lo against AUTO-07p and XPPAUT 8.0 (12 quantities)`
- `PASS tstauto_pen against AUTO-07p and XPPAUT 8.0 (4 quantities)`
- `PASS tstauto_pf against AUTO-07p and XPPAUT 8.0 (10 quantities)`
- `FAIL tstauto_pp2 against AUTO-07p and XPPAUT 8.0 (22 quantities)`
- `PASS tstauto_san against AUTO-07p and XPPAUT 8.0 (10 quantities)`
- `PASS tstauto_she against AUTO-07p and XPPAUT 8.0 (4 quantities)`
- `PASS tstauto_simplefold against AUTO-07p and XPPAUT 8.0 (16 quantities)`
- `PASS tstauto_torx against AUTO-07p and XPPAUT 8.0 (18 quantities)`
- `PASS tstauto_torx_homcont against AUTO-07p (12 quantities)`
- `PASS tstauto_tsthom against AUTO-07p and XPPAUT 8.0 (4 quantities)`
- `PASS tstauto_wcbiwave against AUTO-07p and XPPAUT 8.0 (4 quantities)`
- `PASS zero_hopf against AUTO-07p and XPPAUT 8.0 (20 quantities)`

Individual reference failures:

- forced_pp_pd AUTO-07p/pd_up R2 (stored point 210) par2: 2.27e-05 > 2.1e-05
- ode_mu26_blocked AUTO-07p/to_snic UZ (stored point 135) spectrum: 6.8e-05 > 2.16e-05
- ode_mu26_blocked AUTO-07p/to_snic UZ (stored point 150) norm: 8.34e-05 > 6.03e-05
- ode_mu26_control AUTO-07p/to_snic UZ (stored point 134) spectrum: 0.00015 > 2.16e-05
- ode_mu26_control AUTO-07p/to_snic UZ (stored point 141) spectrum: 0.000324 > 2.09e-05
- ode_mu26_control AUTO-07p/to_snic UZ (stored point 142) spectrum: 0.000524 > 2.08e-05
- tstauto_pp2 XPPAUT 8.0/cycles LP (stored point 27) norm: 0.01 > 0.000489
- tstauto_pp2 XPPAUT 8.0/cycles LP (stored point 27) period: 15.2 > 0.989

Other gates: build with zero compiler warnings and all 58 unit binaries pass; 286 autocheck assertions pass;
216 examples pass (215 checksum outputs, one intentionally without output); all 8 ODE/ODEX fixtures pass;
web2 typecheck exits 0. The native XPPAUT Mark probe passes 20 points and 5 quantities.

`tstauto_pp2`'s two new LP values directly match AUTO-07p, but its four old AUTO-07p LP exceptions currently
exclude those points from automated comparison. Those four AUTO-07p exceptions were subsequently removed and both LPs are now compared; the expected
XPPAUT phase-condition difference is explicitly traced. The sixteen baseline updates were approved on #499. These NTST 15 cycles are underresolved: the existing NTST 60/EPS 1e-9
study has no LP up to period 84.4, so matching discrete LPs are not proof of physical model folds.

The approved update ran `autocompare.py --only` these sixteen names `--update`: 8496 points across one and
automatic threads, 16 equality checks, zero failures. Every new file is byte-for-byte the earlier saved computation.
Every labelled-point type sequence and label ID is unchanged. Point counts are unchanged except `forced_pp_ns`
(264 to 262): `ns_up` crosses its parameter stopping bound after 29 instead of 31 curve points; its ordinary
adaptive-step sequence changes, with the same special-point labels and both reference curves agreeing.
See [the trace data](w437-reference-traces.json) and [the numerical investigation](w437-reference-traces.md)
for the subsequent resolution of the four failed reference checks. The failed gate above remains the historical
measurement before these updates and traced differences, not the final validation result.
