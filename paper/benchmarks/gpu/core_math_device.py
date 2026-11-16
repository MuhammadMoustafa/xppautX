"""CORE-MATH's exp, tanh and cosh (third_party/core-math, the routines xpp::math calls) as CUDA device code, for NVRTC.

The vendored .c files are read as they are and rewritten by text, never copied: the includes, pragmas and attributes go, GCC's
builtins become CUDA's (fma, fabs, copysign, rint for roundeven), every function and table becomes __device__, and each
function sits in its own namespace (exp, tanh and cosh each have their own as_exp_accurate, db, ...). Every rewrite is listed
in REWRITES, so what the device runs is the host's source with nothing else changed (the x86 SSE branches are cut by undefining
__x86_64__, leaving each file's portable branch)."""
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
CORE_MATH = REPO / 'third_party' / 'core-math'
FUNCTIONS = ('exp', 'tanh', 'cosh')

PRELUDE = r'''
#undef __x86_64__
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef int int32_t;
typedef unsigned int uint32_t;
#define feraiseexcept(x) ((void)0)
#define __builtin_expect(a, b) (a)
__device__ __forceinline__ double cm_copysign(double a, double b) { return copysign(a, b); }  // the C call takes ints
'''

REWRITES = (
    (r'^\s*#\s*include.*$', ''),                          # <stdint.h>, <fenv.h>, <x86intrin.h>, <errno.h>
    (r'^\s*#\s*pragma.*$', ''),                           # STDC FENV_ACCESS, GCC diagnostic
    (r'__attribute__\s*\(\(.*?\)\)', ''),                 # noinline, cold
    (r'__builtin_fma\b', 'fma'),
    (r'__builtin_fabs\b', 'fabs'),
    (r'__builtin_copysign\b', 'cm_copysign'),
    (r'__builtin_roundeven\b', 'rint'),                   # round to nearest even in the default rounding mode
    (r'^#if \(\(defined\(__GNUC__\) && __GNUC__ >= 10\).*$', '#if 1'),  # take the __builtin_roundeven branch
    (r'^static\s+(?:inline\s+)?(const\s+double|double|i64|u64|int)\s', r'static __device__ \1 '),  # file-scope functions and tables
    (r'^double\s+(cr_\w+)\(', r'__device__ double \1('),
)


def translate(name):
    text = (CORE_MATH / name / (name + '.c')).read_text()
    for pattern, replacement in REWRITES:
        text = re.sub(pattern, replacement, text, flags=re.M | re.S if pattern.startswith('__attribute__') else re.M)
    return text


def device_source(names=FUNCTIONS):
    """One translation unit: each function in namespace cm_<name>, and `double cm_<name>(double)` calling it."""
    parts = [PRELUDE]
    for n in names:
        parts.append('namespace cm_%s {\n%s\n}\n__device__ __forceinline__ double crd_%s(double x) { return cm_%s::cr_%s(x); }\n' % (n, translate(n), n, n, n))
    return '\n'.join(parts)


NVRTC_OPTIONS = ('--std=c++20', '-fmad=false', '--ftz=false', '--prec-div=true', '--prec-sqrt=true')

if __name__ == '__main__':
    print(device_source())
