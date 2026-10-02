# musl Bessel functions (W163, issue #215)

Vendored `src/math/j0.c`, `j1.c` and `jn.c` from
https://git.musl-libc.org/cgit/musl/, downloaded from master on 2026-10-02
by the reviewer. The precise upstream commit/version was not recorded.
The three `.c` files and musl's `COPYRIGHT` are unmodified; update by swapping
those files. `COPYRIGHT` contains musl's MIT licence and notes the permissive
Sun/fdlibm origins; each source retains its original Sun notice.

On 2026-10-02 CORE-MATH's upstream catalogue
(https://core-math.gitlabpages.inria.fr/) listed no correctly rounded binary64
j0, j1, y0 or y1. These musl routines are portable fdlibm approximations,
not correctly rounded Bessel functions.

Our adapter `libm.h` supplies GET_HIGH_WORD and EXTRACT_WORDS by copying the
IEEE binary64 representation with memcpy and shifting its unsigned integer
bits, independent of byte order and without aliasing. It uses the system's
`double_t`, with a compile-time requirement for binary64 evaluation
(`FLT_EVAL_METHOD == 0`). It renames j0/j1/jn/y0/y1/yn to `xpp_musl_*`,
including calls between the files, so no platform Bessel symbol is used.

The adapter redirects sin, cos, log, sqrt and fabs to small C-callable
wrappers in core/xpp_math.cpp. Those reuse xpp::math's correctly rounded
CORE-MATH functions and CPU dispatch; sqrt and fabs use exact IEEE operations.
`bessel.h` declares the C boundary. The Makefile builds three third-party C
objects without LTO and with `-ffp-contract=off`, like CORE-MATH. Round-to-nearest
and IEEE binary64 arithmetic are required; do not enable fast-math.
The parser's order truncation to int is unchanged.

`tests/test_bessel.cpp` retains the first pass's independent published
John Burkardt TEST_VALUES references:
https://people.sc.fsu.edu/~jburkardt/cpp_src/test_values/test_values.cpp
(the tables cite Abramowitz & Stegun and Mathematica). SciPy was inaccessible
in the sandbox. Tolerance is 16 ULP, or 64 ULP at selected near-zero inputs
for cancellation and printed-table rounding. Tests check repeat-call bits,
order truncation, parity, tiny arguments and IEEE special values, and print
a result-bit fingerprint to compare builds. Cross-platform CI remains the
check of other CPUs and systems. Run the Bessel tests and examples_check after
a source update and name every changed example before updating its checksum.
