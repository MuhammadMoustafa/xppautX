# W268 (#325): where would a GPU pay off in xppautX? A probe

Measure first, build nothing: no change to core/ or the build. Machine: NVIDIA RTX 3060 (GA106, 12 GB, driver 595.99, FP64 at 1/64 of
FP32), i7-11700F (8 cores, 16 threads). No CUDA toolkit: the probes use pip wheels in a new uv group, `uv run --locked --group gpu python ...`
(`pyproject.toml`'s `gpu`: `cupy-cuda13x` with `nvidia-cuda-nvrtc` and `nvidia-cuda-runtime`, held below 13.3 because the driver reports CUDA 13.2).
Why this route and not numba-cuda: CuPy's `RawModule` compiles CUDA C with NVRTC straight from a string, which is all a printed model
needs, and the two wheels besides it are the smallest set that ran (numba-cuda would add numba, llvmlite and the nvvm/nvcc wheels).
The default groups are unchanged.

**Every number below is a probe: untimed (machine not quiet).** The load average was 10 to 16 throughout (the maintainer's
`optimize_fit.py` on 8 jobs, the desktop); the GPU had no compute process (nvidia-smi). xppautX is the maintainer's built `./xppautX`
(-g -O2, 2026-11-15 09:54), run from the main checkout since the worktree has none. Repeats and spread are given with each number;
`paper/benchmarks/gpu/run.py` is the timed script (it refuses unless `tools/comparison-setup.sh --ready` passes and the GPU is free), for
the quiet window after W718; it was checked only to refuse.

Files (all in `paper/benchmarks/gpu/`, one command each, documented in its docstring): `core_math_device.py` (CORE-MATH as device code),
`q1_bits.py`, `q2_sweep.py` + `kernels/ml_rk4.cu`, `q3_network.py` + `kernels/network_rk4.cu`, `q5_launch.py`, `run.py`, `gpucommon.py`.

## 1. Can the GPU give xppautX's bits? Yes.

`core_math_device.py` reads `third_party/core-math/{exp,tanh,cosh}/*.c` as vendored (the functions xpp::math calls; the Makefile builds each
twice, plain and `-mfma`, with `-ffp-contract=off`) and rewrites it by text for NVRTC, nothing copied or edited: the includes, pragmas and
`__attribute__`s are dropped, `__builtin_fma/fabs/copysign/expect/roundeven` become `fma/fabs/copysign/(a)/rint`, `__x86_64__` is undefined
(each file's portable branch is taken: its SSE parts are for x86 only), every function and table becomes `__device__`, each file sits in
its own namespace (they share names such as `as_exp_accurate`). Nothing failed to compile: no 128-bit integers or intrinsics beyond those
(`--std=c++20` is needed for the designated initialisers `{.f = x}`). `-fmad=false --prec-div=true --prec-sqrt=true --ftz=false`.

`q1_bits.py`: 4.0 million arguments per function (uniform in the models' range, the whole exponent range, random bit patterns, the
neighbours of CORE-MATH's own hard-to-round cases, specials), against the host copies built with the Makefile's flags:

| function | host plain vs host FMA copy | CORE-MATH device, `-fmad=false` | CORE-MATH device, fmad on | CUDA native (either) |
|---|---|---|---|---|
| exp | 0 differ | **0 differ** | 0 differ | 130604 differ, max 1 ulp |
| tanh | 0 | **0** | 12 differ (1 ulp) | 54051, max 1 ulp |
| cosh | 0 | **0** | 3 differ (1 ulp) | 377267, max 1 ulp |

CUDA's own functions differ from the correctly rounded ones in 1-9 % of arguments by 1 ulp (its `-fmad` setting changes nothing there). With
contraction on, even CORE-MATH's device code misses (their algorithms are correct rounding only without a fused `a*b+c` the source did not
write): `-fmad=false` is required, as `-ffp-contract=off` is on the host. The bits are given up only by choosing CUDA's functions.

## 2. A parameter sweep: bits identical, 16 to 67 times xppautX

`kernels/ml_rk4.cu`: Morris-Lecar of `examples/ode/lecar.odex` (the right-hand side is its lines 14-21 in their order: `minf`, `ninf`, `lamn`,
`ica`, `v'`, `w'`, `ode_divisor`; `par` and `init` values read from the file), fixed-step RK4 operation by operation as
`core/odesol2.cpp`'s `one_step_rk4`, dt .05, total 30 (600 steps), one run per thread over `iapp` from 0 to 0.2, N values. (Morris-Lecar uses
tanh and cosh, not exp: those are the functions of question 1 the kernel needs.) xppautX's reference rows are in doubles: the .odex is
copied with `storage_precision: double`, the only change; without it xppautX stores float32 and a bit check would be weak.

Check against xppautX (`q2_sweep.py`, protocol client `python/xppautx`, `run.initial` per value, `data.page` rows):

| N | variant | final rows compared | differ | trajectory rows (12 runs x 601) |
|---|---|---|---|---|
| 1e3, 1e4, 1e5 | CORE-MATH device `-fmad=false` | 1000, 1002, 1002 | **0, 0, 0** (every t, v, w) | **0 differ** |
| 1e3, 1e4, 1e5 | CUDA tanh/cosh, `-fmad=false` | same | 740, 746, 706 differ; max 12408, 79104, 42576 ulp, |dv,dw| <= 2.6e-14 | 2614, 2744, 1997 rows differ |
| 1e3, 1e4, 1e5 | CUDA tanh/cosh, fmad on | same | 784, 778, 761 differ; max 6520-348928 ulp, <= 2.6e-14 | about 2700 rows differ |

The CUDA-native differences are the 1-ulp function differences amplified along the orbit (about 1e-14 in absolute terms). `run.range`'s last
run (value arithmetic `start + i*((end-start)/steps)`) equals the GPU's last run bit for bit at each N.

Throughput, runs per second, one repeat set each (median, min-max of 5 GPU / 3 CPU repeats; xppautX is its in-process `run.range`, no
protocol per run, one process and 16 processes each over a slice, started and loaded before the clock):

| N | GPU, CORE-MATH (xppautX bits) | GPU, CUDA functions, fmad off | GPU, CUDA, fmad on | xppautX, 1 process | xppautX, 16 processes |
|---|---|---|---|---|---|
| 1e3 | 3.33e4 (30.0 ms, 30.0-30.1) | 7.2e4 | 7.5e4 | 2074 (0.482 s, 0.452-0.483) | 6548 (0.153 s) |
| 1e4 | 1.23e5 (81.7 ms) | 2.5e5 | 2.6e5 | 2056 (4.86 s) | 8517 (1.17 s) |
| 1e5 | **1.35e5** (0.739 s, 0.728-0.740) | 2.7e5 | 2.9e5 | 2020 (49.5 s) | **8588** (11.6 s, 11.3-11.8) |

So with xppautX's bits the GPU is 5x (N=1e3), 14x (1e4) and 16x (1e5) the 16-process CPU, 16-67x one process. Giving up the bits (CUDA's tanh/cosh)
doubles the GPU's rate again (about 2x), not more: the FP64 rate (1/64) bounds both. The 16 processes reach only 4x one process here because of
the load on the machine (and 16 threads on 8 cores); a quiet run will move the CPU side, probably up by 1.5-2x. Results copy back included
changes nothing (a few microseconds).

## 3. A large network: lecdiff grown to 200, 2000, 20000 cells

`q3_network.py` grows the file by text (every `200` is n, `v201 = v200` is `v<n+1> = v<n>`; the initial values, -0.49 from cell 20 on,
the numerics: total 20, dt .05, Runge-Kutta, the program's defaults, 400 steps). `kernels/network_rk4.cu`: one thread per cell, the cell's
right-hand side is lecdiff200.odex's lines 10-19 in order (`dif*(v[j+1]-2*v[j]+v[j-1])` with the file's edge cells); a step is four kernel
launches (stage kernels: a cell reads its neighbours' stage values), eager, and also recorded once as a CUDA graph.
Final state of every cell (2n values) against xppautX's stored doubles: **0 of 400, 4000, 40000 differ** with CORE-MATH's device code;
41 differ (max 21 ulp) with CUDA's functions (the same 41 at each size: the cells in the wave front).

| cells | xppautX `run.initial`, 1 thread (median, min-max of 3) | GPU CORE-MATH, eager (graph) | GPU CUDA fmad on, eager | GPU over xppautX (bits / CUDA) |
|---|---|---|---|---|
| 200 | 0.047 s (0.022-0.090) | 22.6 ms (20.6) | 12.8 ms | 2.1x / 3.7x |
| 2000 | 0.946 s (0.929-0.990) | 22.7 ms (20.6) | 11.6 ms | 42x / 82x |
| 20000 | 34.1 s (15.3-37.3: the machine) | 91 ms (90) | 54.8 ms | 375x / 622x (about 170x / 280x on the min) |

Crossover (5 repeats, sizes 50-1000): xppautX 8.6 ms at 50 cells, 16.7 ms at 100, 36.7 ms at 200, 116 ms at 500, 432 ms at 1000 (min 174); the GPU
(bits) 15.3, 20.9, 21.3, 21.4, 22.8 ms. **The GPU wins from about 100-150 cells with xppautX's bits** (about 50-100 with CUDA's functions). Below
that the 1600 launches (4 per step, 400 steps) cost 13-14 us each, latency not launch rate: the kernel's dependent chain of three CORE-MATH
calls takes about that long even for one warp (a CUDA graph saves only 1-2 us a launch).

## 4. Portability and the route a model would take (desk research)

- **CUDA** is NVIDIA's; NVRTC compiles CUDA C at run time from a string, with `-fmad=false` and IEEE division and square root: the route probed. **HIP** (AMD) takes
  nearly the same source through hipRTC (not tried: no AMD GPU here); consumer AMD GPUs run FP64 at 1/16 to 1/32 of FP32 (vendor data sheets, not verified here).
- **Vulkan compute** has an optional `shaderFloat64` feature (and `shaderInt64`), absent on many mobile GPUs. The decisive gap: SPIR-V's GLSL.std.450 `Exp`, `Log`,
  `Sin`, `Tanh`, `Pow` accept only 16- and 32-bit float operands and give no error bound ([GLSL.std.450 spec](https://registry.khronos.org/SPIR-V/specs/unified1/GLSL.std.450.html):
  "The operand x must be a scalar or vector whose component type is 16-bit or 32-bit floating-point"); only the basic operations (`Fma`, `Sqrt`, `FAbs`, ...) take doubles. So
  every double transcendental would be our own code: CORE-MATH written once in GLSL/SPIR-V (it needs 64-bit integers for its bit work, `shaderInt64`, and an exact `fma`), and the
  driver's compiler must not contract `a*b+c` (SPIR-V's `NoContraction` decoration, per operation).
- **OpenCL** double precision is optional (`cl_khr_fp64`, or the `__opencl_c_fp64` feature of OpenCL C 3.0: [OpenCL C spec](https://registry.khronos.org/OpenCL/specs/3.0-unified/html/OpenCL_C.html), 6.2.1, 6.3.1.1);
  its `exp`, `tanh`, `cosh` are allowed several ulps of error (the spec's table, section 7.4; the exact double bounds were not read here), so they would not give our bits either; a CORE-MATH port
  with `#pragma OPENCL FP_CONTRACT OFF` would. NVIDIA's OpenCL runs on this driver; Apple's is deprecated.
- **Metal** (Apple GPUs): no `double` in the Metal Shading Language and no FP64 hardware on Apple GPUs (the MSL specification's type section and Apple's
  developer forum say so: [forum](https://developer.apple.com/forums/thread/709663), [MSL specification](https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf), which this probe could not
  fetch whole). Bit identity with a CPU double is therefore only by emulating binary64 in 32-bit and 64-bit integers: a different, much slower program. On macOS the GPU is out as a path to
  xppautX's numbers; a Mac keeps the CPU path.
- **The route from a model**: xppautX compiles a model's expression IR (core/expr_ir.h) to native code with MIR (core/xpp_mir.cpp, no GPU target). The IR is a tree of
  double operations with calls to the `xpp::math` functions, so printing it as CUDA C (or OpenCL C) is a back end like W764's planned language targets (#843): the
  function bodies become the `rhs` device function, the operation order is the IR's, `crd_*` calls the device CORE-MATH of `core_math_device.py` (the generated
  source would carry the vendored files through the same text rewrite or, better, a header kept in third_party for it), NVRTC compiles it at load. What
  the probe's hand-written kernels did for two models, the printer would do for any model that has no tables, delays, flags or stored state (a first-card scope).

## 5. autoX's collocation: "unlikely" confirmed

`q5_launch.py`. Work of one mesh interval in a typical run (`tests/autox/ode_lecar.json`, Morris-Lecar cycles: NTST 15, NCOL 4, ndim 2, two free
parameters), counted from `core/setubv2.cpp`'s `setubv_make_fa_aa_bb` and `core/conpar2.cpp`'s `conpar_process`: 28 right-hand-side evaluations
(per collocation point: 1 + 2 ndim central-difference + 2 parameter differences = 7, times 4), 56 counting the residual pass that calls `funi` again
(docs/xppaut-findings.md 39), 272 flops of assembly, 494 of elimination (an 8 x 10 block). One pass over the mesh: 420 (840) evaluations and about 11.5 kflop. At the
measured <= 206 ns per evaluation (q2: 495 us for 2400 evaluations; an upper bound) that is about 85-170 us of CPU per assembly pass, and what remains
(the elimination across intervals, the boundary and the global solve) is sequential.

Latencies (median, 2000 repeats, untimed): empty kernel launch and synchronise **6.0 us** (min 5.4); back-to-back launches 2.9 us each; copy of the mesh data, 1200 doubles
there and back, **16 us**; one launch of 420 right-hand sides (CORE-MATH functions) **20 us** (15 us with CUDA's). So handing a pass to the GPU costs 6 us + 16 us + a launch of the work
(20 us), 40 us or more, against the 85 us it saves at most, for the assembly alone; the elimination still has to come back to the CPU, and a Newton step does that
a few times. A typical run's intervals are too few and too cheap: **does not pay** at ndim <= 10, NTST <= 100. It would only start to pay when one pass is thousands of
evaluations at once, which means ndim in the hundreds or thousands with the Jacobian columns computed together; there the dense solve, not the assembly, dominates, and that is a GPU
linear-algebra card (cuSOLVER), not a collocation kernel.

## Verdicts

| candidate | verdict | what a real card costs |
|---|---|---|
| parameter sweep (run.range, Morris-Lecar) | **pays, with xppautX's bits**: 5-16x the 16-process CPU, 16-67x one process at N = 1e3-1e5, identical to the last bit; CUDA's functions would double the rate and give the bits up (1-ulp function differences become 1e-14 in the orbit, which breaks the md5 baseline) | an NVIDIA-only back end behind `run.range` / sweeps: the IR printed as CUDA C (W764's printer), device CORE-MATH kept in third_party and tested by this probe's bit check against `xpp::math`, NVRTC at run time (a driver dependency; the window plugin pattern of W13e loads it only when asked), data back as the same doubles; no tables, delays, flags, events or stored aux in a first scope. Mac/AMD: stays on the CPU. |
| large network (one integration) | **pays from about 100-150 cells with the bits** (2x at 200, 42x at 2000, 375x at 20000 against one CPU thread; the CPU side is serial, so compare with a threaded CPU if one comes), identical to the last bit | the same printer plus a cell-parallel right-hand side: needs array equations as the IR sees them (the probe wrote the stencil by hand), stage kernels and the integrator step on the device (every method's step, not only RK4; adaptive and implicit methods need reductions and solves on the device), and the data table fed from device memory. Much bigger than the sweep. |
| autoX collocation | **does not pay** (launch + copy 40 us against at most 85 us of work, on a loop the CPU needs back) | none recommended; revisit for ndim in the thousands as a dense-solve card |

## Open points

- All timings are untimed probes on a loaded machine (CPU side especially: 16 processes gave only 4x one process; 20000 cells ranged 15-37 s). `run.py` repeats questions 2 and 3
  quietly; the verdicts' ratios (5-16x, 42x, 375x) have wide margins but the exact figures will move.
- The rule of the same numbers is kept only by NVIDIA GPUs with the bit check, a gate any GPU card must run in every build; a GPU result is a machine-dependent option (like a thread count), not the baseline.
  Apple and most mobile GPUs cannot take part.
- GPU numbers use an FP64 rate of 1/64; a data-centre GPU (1/2) would shift the whole table, but the portability argument stays.
- Tested one model per candidate (published ones: Morris-Lecar and its cable). Not tried: HIP, OpenCL, Vulkan (desk research only), implicit or adaptive methods, models with tables.
- The CPU reference used one xppautX process per CPU worker through the protocol (`run.range` in process); per-run client calls cost 4.8 ms a run instead of 0.45 ms.
