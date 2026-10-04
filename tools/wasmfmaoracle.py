#!/usr/bin/env python3
"""W185: independent MPFR binary64 FMA vectors, including exceptional values.

MPFR's 8192-bit operation is exact for finite binary64 a*b+c: even the
largest alignment spans fewer than 4300 bits. mpfr_get_d rounds once to
binary64, including subnormal results. No candidate implementation is used.
"""
import argparse
import ctypes
import ctypes.util
import json
import math
import pathlib
import random
import struct

class MpfrValue(ctypes.Structure):
    _fields_ = [('precision', ctypes.c_long), ('sign', ctypes.c_int),
                ('exponent', ctypes.c_long), ('limbs', ctypes.POINTER(ctypes.c_ulong))]

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=pathlib.Path)
args = parser.parse_args()
library = ctypes.util.find_library('mpfr')
if library is None:
    raise SystemExit('MPFR runtime library is required')
mpfr = ctypes.CDLL(library)
ptr = ctypes.POINTER(MpfrValue)
mpfr.mpfr_init2.argtypes = [ptr, ctypes.c_long]
mpfr.mpfr_clear.argtypes = [ptr]
mpfr.mpfr_set_d.argtypes = [ptr, ctypes.c_double, ctypes.c_int]
mpfr.mpfr_fma.argtypes = [ptr, ptr, ptr, ptr, ctypes.c_int]
mpfr.mpfr_get_d.argtypes = [ptr, ctypes.c_int]
mpfr.mpfr_get_d.restype = ctypes.c_double
mpfr.mpfr_get_version.restype = ctypes.c_char_p
# MPFR_RNDN: nearest, ties to even, the application's numerical contract.
Nearest = 0
Precision = 8192
RandomCases = 20000
CancellationCases = 6000
Seed = 185

def number(bits):
    return struct.unpack('>d', struct.pack('>Q', bits))[0]

def bits(value):
    return struct.unpack('>Q', struct.pack('>d', value))[0]

edge = [0, 1, 2, 0x000fffffffffffff, 0x0010000000000000,
        0x0010000000000001, 0x3fefffffffffffff, 0x3ff0000000000000,
        0x3ff0000000000001, 0x7fefffffffffffff, 0x7ff0000000000000,
        0x7ff8000000000000]
edge += [x | (1 << 63) for x in edge]
cases = [(x,y,z) for x in edge for y in edge for z in edge]
random_source = random.Random(Seed)
cases += [tuple(random_source.getrandbits(64) for _ in range(3)) for _ in range(RandomCases)]
for _ in range(CancellationCases):
    # Moderate exponents keep the rounded product finite; exact cancellation
    # and its neighbors test information discarded by a separate multiply.
    x = math.ldexp(1 + random_source.random(), random_source.randrange(-400,401))
    y = math.ldexp(1 + random_source.random(), random_source.randrange(-400,401))
    z = -(x*y)
    cases += [(bits(x), bits(y), bits(v)) for v in
              [z, math.nextafter(z, -math.inf), math.nextafter(z, math.inf)]]
cases.append((bits(1+2**-27),bits(1-2**-27),bits(-1.0)))
values = [MpfrValue() for _ in range(4)]
for value in values:
    mpfr.mpfr_init2(ctypes.byref(value), Precision)
rows = []
try:
    for case in cases:
        for value, raw in zip(values,case):
            mpfr.mpfr_set_d(ctypes.byref(value),number(raw),Nearest)
        mpfr.mpfr_fma(ctypes.byref(values[3]),*(ctypes.byref(v) for v in values[:3]),Nearest)
        expected = mpfr.mpfr_get_d(ctypes.byref(values[3]),Nearest)
        rows.append([*(f'{v:016x}' for v in case),
                     'nan' if math.isnan(expected) else f'{bits(expected):016x}'])
finally:
    for value in values:
        mpfr.mpfr_clear(ctypes.byref(value))
if rows[-1][-1] != f'{bits(-2**-54):016x}':
    raise SystemExit('MPFR rounding witness differs')
args.output.write_text(json.dumps({'mpfr':mpfr.mpfr_get_version().decode(),
    'precision':Precision,'seed':Seed,'vectors':rows}),encoding='utf-8')
print(f'MPFR {mpfr.mpfr_get_version().decode()}: {len(rows)} independent FMA vectors')