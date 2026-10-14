# W356: AUTO locates special points as AUTO-07p does

W356 ([#418](https://github.com/MuhammadMoustafa/xppautX/issues/418)), W302 stage 4.
Methods reimplemented in C++; no Fortran was copied. The reference is now
AUTO-07p **0.9.4**, refreshed by W357 ([#419](https://github.com/MuhammadMoustafa/xppautX/issues/419)).
No foreign reference file or scientific comparison tolerance was changed here.

## Acceptance decision and the initial stop

The first D1 audit stopped under a literal bitwise interpretation of
"never worse". On 2026-10-14 the maintainer clarified on #418 that an
increase is worse only beyond the reference's last printed digit, or
beyond EPSS for a located point. The initial increases of roughly 1e-10
to 3e-9 are noise under that rule, and D1 was retained. Examples from that
initial audit: lamom2 LP17 spectrum 3.7766112018e-6 -> 3.7767605249e-6;
tstauto_lecar HB50 period 0.00166834651905 -> 0.00166834961347;
ode_lecar HB46 parameter 5.45113838818e-5 -> 5.45114020380e-5.
Those figures predate W357; the current 0.9.4 comparison is below.

The code was then rebuilt warning-free after rebase. Each part was
compared against the unchanged diagram baseline before the next part.
All 26 recorded-reference checks passed at D1, D2 and D3. No largest
AUTO-07p difference increased by more than EPSS, either relative to the
original baseline or between parts. Every changed reference quantity is at a located special point;
EPSS is 1e-4 in these descriptions. No broader tolerance was introduced.
The old generated check files were removed; `build/ucrt` objects were kept.

## Search, reuse and implementation

Searched the common numerical owner `xpp_math`, all of `core/` for Mueller,
discriminants and location state, and AUTO's translated sources for test
history, multiplier sorting and NINS. There is one Mueller implementation
and two callers. Reused W326's owned `LocationState`, the existing test
functions, typed `AutoPointCode`/`AutoPointKind` and `AutoProblem`, and
`autocompare`'s full-precision diagram and reference readers. Both AE and
BVP controllers use the new shared `locate_tests` ordering helper and
`location_sign_change` coincidence/sign logic. AUTO's files were included
in the search and changes, with no vendored-code exemption.

- **D1**: `MUELLER`, support.f90:31-61. Nonpositive discriminants use a zero
  radical; `-2*q/(a+root)` avoids cancellation. `LCSPAE`, ae.f90:743, and
  `LCSPBV`, bvp.f90:1414: the abscissa uses the actual step after retries.
  IEEE `std::sqrt` is explicitly permitted by `xpp_math.h:53`.
- **D2**: `CNRLAE`, ae.f90:199-214, and `CNRLBV`, bvp.f90:287-305. Track a
  test abscissa separately from the solver's step history and reevaluate
  earlier tests when a later locator moves the solution. Retain the
  existing initial EPSS early return and zero the BVP test sample there.
  `LCSPAE`, ae.f90:679-698, and `LCSPBV`, bvp.f90:1309-1329: first label
  wins for coincident tests, with a bifurcation overriding UZ; sign tests
  avoid multiplying values that can overflow. AE branch data are stored
  after the final label is settled. Failed searches retain W326's restore.
- **D3**: periodic.f90:1960-1983,2031-2037,2136-2140. Insert conjugate
  blocks together, count `|mu| <= 1+1e-5`, suppress PD/TR in planar flows.
  The existing PD/TR NINS-change gate is retained; `FNHBEQ`,
  equilibrium.f90:624-629, additionally requires a whole pair for HB.

**Not applicable here:** the CP-over-LP coincidence exception has no CP
point kind in our current typed code; codimension-two labels belong to
W302 stage 7. Two-parameter resonance systems are likewise outside this
stage. BP is *not* suppressed by NINS: AUTO-07p's actual `FNBPAE`
(toolboxae.f90:672-700) and `FNBPBV` (toolboxbv.f90:366-412) are determinant
tests without that condition. Applying the study's broad shorthand to BP
would incorrectly change this method.

There is no new trust boundary, input reader or input-sized allocation.
No negative-discriminant XPPAUT finding is claimed: the unit test reaches
our numerical routine; no XPPAUT executable reproduction was performed.

## Every changed point, with its part

Rows below are resolved stored diagram rows, excluding branch metadata
lines (the byte check's first-difference number includes those lines).
D1's root changes propagate along the following continuation rows. D2's
extra changes are only the stored continued parameter: reevaluating
`fnlpae` sets `par[icp[0]]=rlcur[0]` after the last corrector update.
States, periods and spectra are unchanged between D1 and D2. D3 changes
no diagram bytes relative to D2 on these fixtures. No labels, run starts
or point counts change in any part. Models not listed remain byte-exact.

| Description | Part | Every changed stored row versus preceding part | Changed labelled points (run-local) |
|---|---|---|---|
| ode_lecar | D1 | 19-49, 78-79 | steady HB19; steady output20; steady output40; steady HB46; steady EP49; cycles LP29; cycles EP30 |
| tstauto_brper | D1 | 49 | cycles EP29 |
| tstauto_lamom2 | D1 | 47-60 | cycles LP17; cycles output20; cycles EP30 |
| tstauto_lecar | D1 | 24-53, 83 | steady output40; steady HB50; steady EP53; cycles EP30 |
| tstauto_pp2 | D1 | 129-130 | cycles LP29; cycles EP30 |
| tstauto_simplefold | D1 | 63-100 | steady LP63; steady output80; steady EP100 |
| ode_lecar | D2 | 19, 46 | steady HB19; steady HB46 |
| tstauto_frcp | D2 | 38 | steady HB38 |
| tstauto_lecar | D2 | 23, 50 | steady HB23; steady HB50 |
| tstauto_pp2 | D2 | 44 | steady BP44 |

The seven final baselines are `ode_lecar`, `tstauto_brper`, `tstauto_frcp`,
`tstauto_lamom2`, `tstauto_lecar`, `tstauto_pp2`, `tstauto_simplefold`.
Every candidate matches between 1 and automatic threads. Only these
names were supplied to `autocompare --update --only`; no unexpected model
changed. The two coarse pp2 cycle folds remain in the previously traced
underresolved regime and are not validated physical folds.

## Current AUTO-07p 0.9.4 maxima, all quantities of the changed models

Computed using the existing `diagram_rows`, `read_references` and
`compare_references`; the saved original and per-part candidate diagrams
are the inputs. Existing `calc` transitions delimit runs. Values are
largest absolute differences among untraced, matched points, exactly as
in the main result table. Unchanged models retain every original value.

| Description | Quantity | Original | D1 | D2 / D3 | Final maximum at |
|---|---|---:|---:|---:|---|
| ode_lecar | par | 5.4511383881794551e-05 | 5.4511402037993317e-05 | 1.184836946965806e-05 | steady HB 19 |
| ode_lecar | norm | 1.5026158562414516e-06 | 1.5026158562969627e-06 | 1.5026158562969627e-06 | steady HB 19 |
| ode_lecar | period | 0.0016683903897511954 | 0.0016683894349842632 | 0.0016683894349842632 | steady HB 46 |
| ode_lecar | spectrum | 0.0028047409676305035 | 0.0028047396963796212 | 0.0028047396963796212 | cycles LP 29 |
| tstauto_brper | par | 0 | 0 | 0 | steady EP 1 |
| tstauto_brper | norm | 0 | 0 | 0 | steady EP 1 |
| tstauto_frcp | par | 5.1552683617561712e-06 | 5.1552683617561712e-06 | 3.4462432907389484e-12 | steady HB 38 |
| tstauto_frcp | norm | 1.0491607582707729e-13 | 1.0491607582707729e-13 | 1.0491607582707729e-13 | steady HB 38 |
| tstauto_frcp | period | 2.0769225240191425e-06 | 2.0769225240191425e-06 | 2.0769225240191425e-06 | steady HB 38 |
| tstauto_frcp | spectrum | 0.00092825935894314828 | 0.00092825935894314828 | 0.00092825935894314828 | steady HB 38 |
| tstauto_lamom2 | par | 9.1399110502266012e-13 | 9.1404661617389138e-13 | 9.1404661617389138e-13 | cycles LP 17 |
| tstauto_lamom2 | norm | 2.9070079676785099e-12 | 2.9071189899809724e-12 | 2.9071189899809724e-12 | cycles LP 17 |
| tstauto_lamom2 | period | 2.0476953466186387e-11 | 2.0476953466186387e-11 | 2.0476953466186387e-11 | steady HB 20 |
| tstauto_lamom2 | spectrum | 3.7766112017667552e-06 | 3.7767605248761882e-06 | 3.7767605248761882e-06 | cycles LP 17 |
| tstauto_lecar | par | 5.450019072367196e-05 | 5.4500126072332122e-05 | 1.2390428469510884e-05 | steady HB 23 |
| tstauto_lecar | norm | 1.5714117854170873e-06 | 1.5714117854170873e-06 | 1.5714117854170873e-06 | steady HB 23 |
| tstauto_lecar | period | 0.0016683465190503455 | 0.0016683496134710651 | 0.0016683496134710651 | steady HB 50 |
| tstauto_lecar | spectrum | 0.0029018880726365825 | 0.0029018880726365825 | 0.0029018880726365825 | cycles LP 29 |
| tstauto_pp2 | par | 1.7630341631047486e-12 | 1.7630341631047486e-12 | 1.7630341631047486e-12 | steady HB 36 |
| tstauto_pp2 | norm | 3.33333360913457e-12 | 3.33333360913457e-12 | 3.33333360913457e-12 | steady BP 44 |
| tstauto_pp2 | period | 6.2534866174246417e-11 | 6.2534866174246417e-11 | 6.2534866174246417e-11 | steady HB 36 |
| tstauto_pp2 | spectrum | 4.9529635082201651e-07 | 4.9529635082201651e-07 | 4.9529635082201651e-07 | steady BP 44 |
| tstauto_simplefold | par | 4.8356318949061006e-12 | 4.8356318949061006e-12 | 4.8356318949061006e-12 | steady LP 63 |
| tstauto_simplefold | norm | 4.4745984695282459e-11 | 4.4745984695282459e-11 | 4.4745984695282459e-11 | steady EP 1 |
| tstauto_simplefold | spectrum | 8.57248716457093e-11 | 8.57248716457093e-11 | 8.57248716457093e-11 | steady LP 63 |

Overall AUTO-07p maxima: parameter 5.45113838818e-5 -> 1.23904284695e-5;
norm 1.57141178542e-6 -> unchanged; period 0.00166839038975 ->
0.00166838943498; spectrum 0.00290188807264 -> unchanged.
The expected large period/spectrum reduction did not occur. The method
changes alone do not justify claiming it; no further numerical method was
changed to force the anticipated result.

## Differences from XPPAUT, traced by part

D1 moves the six initial diagrams by root arithmetic and its propagation;
the parameter-only D2 changes are at ode/lecar HB19/HB46, tstauto/lecar
HB23/HB50, frcp HB38 and pp2 BP44. Against XPPAUT the largest continued-
parameter difference becomes 5.46013381325e-5 at tstauto/lecar HB50;
frcp HB38 differs by 5.18554344620e-6. The data states are unchanged in
D2, and every quantity still passes its existing reference tolerance.
D1 changes tstauto/lecar's largest XPPAUT period difference from
1.83296045009e-9 to 3.51347040350e-9. No new missing/extra-point exception
was added. W326's LP and coarse-mesh pp2 explanations are unchanged.

## Validation

All gates run serially and in the foreground through `tools/heavy.sh`,
Git Bash, UCRT gcc 16, `-j4`, `WERROR=1`. Each part's build and all 50 unit
suites passed, with zero compiler warnings. The final unit run passed 42,876 assertions; the location suite has 40
location assertions, including negative discriminant, exact linear root,
a retried location, two events in one step, failed-location restoration,
nonfinite-step rejection and refusal to store nonfinite state.
The two-events fixture uses the actual controller and model u'=p-u:
marks 0.35 and 0.3 fall in one step, tested in that order. The second
location moves back to 0.3; refreshing the earlier sample lets the next
step find 0.35. Both accepted states satisfy u=p, independently checked.

Before refresh: D1 84 checks, 12 byte failures; D2/D3 84 checks, 14 byte
failures. All 26 recorded-reference checks pass at every stage, and both
thread counts produce the same bytes. The targeted refresh passes 7/7
thread-agreement checks (1,208 stored points including both thread counts).
Final comparison: **84/84 passed**, 38 descriptions, 9 skipped, 4,054 points
across both thread counts. Full autocheck: **247/247 passed**, including
HomCont speed/mesh/interval checks. WSL gates and sourcecheck are the
reviewer's tier. Examples: **185 models**, **184 checksums unchanged**;
`ml-range.odex` produces no output by design. `tests/examples.md5` is unchanged.

All foreground gates completed; no background run or polling loop was started.
Temporary check scripts, logs, saved diagrams, audit JSON and test model files
were removed after recording the results here; `build/ucrt` was retained.
The coordinator owns its live W356 agent entry in the background-task register
and closes it at handoff; this agent added no background-run entries.
