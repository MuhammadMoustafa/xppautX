# W184 measurement samples

Companion to [the report](wasm-performance.md). Timings are observations, never pass/fail thresholds. Historical microkernel logs use one warm-up; final kernels use five. Browser logs are separate sequential blocks, not an alternating application comparison.

## Pre-review Node kernel variation

The earlier browser-ready baseline kernel (one warm-up) recorded:

```text
arithmetic median_ms=11.1897
sin samples_ms=165.725,103.995,73.8487,74.7089,73.7223 median_ms=74.7089
fma samples_ms=22.1286,21.6177,21.9028,22.2937,21.7081 median_ms=21.9028
```

Final reviewed builds below use five warm-ups and a runtime rounding witness.
The variation is a limitation of microkernel timing, not an exact speed guarantee.

## w184-arithmetic.log

```text
perf: wasm-arithmetic.odex native 395 ms (warm-up) series=off events=11 bytes=8479
perf: wasm-arithmetic.odex wasm 411 ms (warm-up) series=off events=11 bytes=8479
perf: wasm-arithmetic.odex wasm 409 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex native 413 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex native 404 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex wasm 395 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex wasm 409 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex native 412 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex native 399 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex wasm 415 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex wasm 409 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex native 398 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex native median 404 ms (alternated samples: 413, 404, 412, 399, 398)
perf: wasm-arithmetic.odex wasm median 409 ms (alternated samples: 409, 395, 409, 415, 409)
```

## w184-ceiling-kuramoto.log

```text
perf: kuramot100.odex native 1038 ms (warm-up) series=off events=18 bytes=8791
perf: kuramot100.odex wasm 1394 ms (warm-up) series=off events=21 bytes=8892
perf: kuramot100.odex wasm 1394 ms series=off events=21 bytes=8892
perf: kuramot100.odex native 1038 ms series=off events=17 bytes=8758
perf: kuramot100.odex native 1057 ms series=off events=18 bytes=8792
perf: kuramot100.odex wasm 1410 ms series=off events=21 bytes=8892
perf: kuramot100.odex wasm 1352 ms series=off events=21 bytes=8892
perf: kuramot100.odex native 1066 ms series=off events=18 bytes=8791
perf: kuramot100.odex native 1037 ms series=off events=17 bytes=8758
perf: kuramot100.odex wasm 1455 ms series=off events=22 bytes=8926
perf: kuramot100.odex wasm 1472 ms series=off events=22 bytes=8926
perf: kuramot100.odex native 1152 ms series=off events=19 bytes=8825
perf: kuramot100.odex native median 1057 ms (alternated samples: 1038, 1057, 1066, 1037, 1152)
perf: kuramot100.odex wasm median 1410 ms (alternated samples: 1394, 1410, 1352, 1455, 1472)
```

## w184-ceiling-math.log

```text
fma_witness fused=-0x1p-54 unfused=0x0p+0
arithmetic fingerprint=a7e6b13e80453d25 samples_ms=12.8448,12.353,11.9633,13.1193,12.5914, median_ms=12.5914
sin fingerprint=b5706e6c8c5aee20 samples_ms=51.413,50.7966,49.7597,50.5363,53.5174, median_ms=50.7966
fma fingerprint=da23cfb980453d25 samples_ms=12.4825,12.3906,12.5432,12.6769,12.8193, median_ms=12.5432
```

## w184-ceiling-test-math.log

```text
xpp::math              106 checks, 0 failed
```

## w184-final-ceiling-math.log

```text
fma_witness fused=-0x1p-54 unfused=0x0p+0
arithmetic fingerprint=a7e6b13e80453d25 samples_ms=11.7912,11.5397,11.6081,11.6043,11.3666, median_ms=11.6043
sin fingerprint=b5706e6c8c5aee20 samples_ms=47.1675,47.2461,47.317,47.2531,47.3129, median_ms=47.2531
fma fingerprint=da23cfb980453d25 samples_ms=11.7034,11.6425,11.6622,11.623,11.7368, median_ms=11.6622
```

## w184-final-native-math.log

```text
fma_witness fused=-0x1p-54 unfused=0x0p+0
arithmetic fingerprint=a7e6b13e80453d25 samples_ms=6.88112,5.45509,6.98061,6.07566,6.14754, median_ms=6.14754
sin fingerprint=b5706e6c8c5aee20 samples_ms=28.7764,28.7174,28.671,28.6464,28.7199, median_ms=28.7174
fma fingerprint=da23cfb980453d25 samples_ms=5.27012,5.39528,5.47675,5.30399,5.28921, median_ms=5.30399
```

## w184-final-wasm-math.log

```text
fma_witness fused=-0x1p-54 unfused=0x0p+0
arithmetic fingerprint=a7e6b13e80453d25 samples_ms=11.5535,11.4975,11.8983,12.2151,11.6067, median_ms=11.6067
sin fingerprint=b5706e6c8c5aee20 samples_ms=192.821,192.531,193.252,192.715,193.525, median_ms=192.821
fma fingerprint=da23cfb980453d25 samples_ms=49.7368,50.4989,50.37,50.0908,50.5233, median_ms=50.37
```

## w184-math-native.log

```text
arithmetic fingerprint=a7e6b13e80453d25 samples_ms=5.12632,5.16244,5.17614,5.17769,5.19333, median_ms=5.17614
sin fingerprint=b5706e6c8c5aee20 samples_ms=28.1467,28.087,28.1076,28.1564,28.165, median_ms=28.1467
fma fingerprint=da23cfb980453d25 samples_ms=5.11014,5.1133,5.13209,5.10463,5.13801, median_ms=5.1133
```

## w184-math-wasm.log

```text
arithmetic fingerprint=a7e6b13e80453d25 samples_ms=11.4737,11.4075,11.439,11.4915,11.4491, median_ms=11.4491
sin fingerprint=b5706e6c8c5aee20 samples_ms=175.442,189.061,190.747,188.967,184.653, median_ms=188.967
fma fingerprint=da23cfb980453d25 samples_ms=55.9479,56.9567,54.6235,53.723,55.1355, median_ms=55.1355
```

## w184-o3-arithmetic.log

```text
perf: wasm-arithmetic.odex native 385 ms (warm-up) series=off events=11 bytes=8479
perf: wasm-arithmetic.odex wasm 378 ms (warm-up) series=off events=11 bytes=8479
perf: wasm-arithmetic.odex wasm 378 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex native 379 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex native 382 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex wasm 392 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex wasm 379 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex native 418 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex native median 382 ms (alternated samples: 379, 382, 418)
perf: wasm-arithmetic.odex wasm median 379 ms (alternated samples: 378, 392, 379)
```

## w184-o3-kuramoto.log

```text
perf: kuramot100.odex native 1165 ms (warm-up) series=off events=19 bytes=8825
perf: kuramot100.odex wasm 3920 ms (warm-up) series=off events=41 bytes=9561
perf: kuramot100.odex wasm 3946 ms series=off events=41 bytes=9560
perf: kuramot100.odex native 1063 ms series=off events=18 bytes=8791
perf: kuramot100.odex native 1038 ms series=off events=18 bytes=8791
perf: kuramot100.odex wasm 3930 ms series=off events=41 bytes=9561
perf: kuramot100.odex wasm 3915 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1038 ms series=off events=17 bytes=8758
perf: kuramot100.odex native median 1038 ms (alternated samples: 1063, 1038, 1038)
perf: kuramot100.odex wasm median 3930 ms (alternated samples: 3946, 3930, 3915)
```

## w184-o3-lto-arithmetic.log

```text
perf: wasm-arithmetic.odex native 380 ms (warm-up) series=off events=11 bytes=8479
perf: wasm-arithmetic.odex wasm 497 ms (warm-up) series=off events=12 bytes=8512
perf: wasm-arithmetic.odex wasm 504 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex native 415 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex native 382 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex wasm 465 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex wasm 474 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex native 420 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex native median 415 ms (alternated samples: 415, 382, 420)
perf: wasm-arithmetic.odex wasm median 474 ms (alternated samples: 504, 465, 474)
```

## w184-o3-lto-kuramoto.log

```text
perf: kuramot100.odex native 1140 ms (warm-up) series=off events=19 bytes=8825
perf: kuramot100.odex wasm 5519 ms (warm-up) series=off events=57 bytes=10096
perf: kuramot100.odex wasm 5624 ms series=off events=57 bytes=10096
perf: kuramot100.odex native 1096 ms series=off events=18 bytes=8791
perf: kuramot100.odex native 1038 ms series=off events=17 bytes=8758
perf: kuramot100.odex wasm 5307 ms series=off events=57 bytes=10096
perf: kuramot100.odex wasm 5326 ms series=off events=57 bytes=10096
perf: kuramot100.odex native 1088 ms series=off events=18 bytes=8791
perf: kuramot100.odex native median 1088 ms (alternated samples: 1096, 1038, 1088)
perf: kuramot100.odex wasm median 5326 ms (alternated samples: 5624, 5307, 5326)
```

## w184-optimized-arithmetic.log

```text
perf: wasm-arithmetic.odex native 412 ms (warm-up) series=off events=12 bytes=8512
perf: wasm-arithmetic.odex wasm 515 ms (warm-up) series=off events=13 bytes=8546
perf: wasm-arithmetic.odex wasm 465 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex native 410 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex native 382 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex wasm 475 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex wasm 474 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex native 377 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex native 379 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex wasm 489 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex wasm 475 ms series=off events=12 bytes=8512
perf: wasm-arithmetic.odex native 378 ms series=off events=11 bytes=8479
perf: wasm-arithmetic.odex native median 379 ms (alternated samples: 410, 382, 377, 379, 378)
perf: wasm-arithmetic.odex wasm median 475 ms (alternated samples: 465, 475, 474, 489, 475)
```

## w184-optimized-kuramoto.log

```text
perf: kuramot100.odex native 1110 ms (warm-up) series=off events=18 bytes=8791
perf: kuramot100.odex wasm 5327 ms (warm-up) series=off events=57 bytes=10096
perf: kuramot100.odex wasm 5297 ms series=off events=57 bytes=10096
perf: kuramot100.odex native 1031 ms series=off events=17 bytes=8758
perf: kuramot100.odex native 1036 ms series=off events=17 bytes=8758
perf: kuramot100.odex wasm 5436 ms series=off events=57 bytes=10096
perf: kuramot100.odex wasm 5561 ms series=off events=57 bytes=10096
perf: kuramot100.odex native 1032 ms series=off events=17 bytes=8758
perf: kuramot100.odex native 1034 ms series=off events=17 bytes=8758
perf: kuramot100.odex wasm 5345 ms series=off events=57 bytes=10096
perf: kuramot100.odex wasm 5357 ms series=off events=57 bytes=10096
perf: kuramot100.odex native 1028 ms series=off events=17 bytes=8758
perf: kuramot100.odex native median 1032 ms (alternated samples: 1031, 1036, 1032, 1034, 1028)
perf: kuramot100.odex wasm median 5357 ms (alternated samples: 5297, 5436, 5561, 5345, 5357)
```

## w184-optimized-test-math.log

```text
xpp::math              106 checks, 0 failed
```

## w184-paired.log

```text
perf: kuramot100.odex native 1136 ms (warm-up) series=off events=19 bytes=8825
perf: kuramot100.odex wasm 3666 ms (warm-up) series=off events=42 bytes=9594
perf: kuramot100.odex wasm-comparison 1416 ms (warm-up) series=off events=21 bytes=8891
perf: kuramot100.odex wasm-comparison 1430 ms series=off events=21 bytes=8892
perf: kuramot100.odex wasm 3681 ms series=off events=41 bytes=9560
perf: kuramot100.odex native 1040 ms series=off events=18 bytes=8791
perf: kuramot100.odex native 1137 ms series=off events=19 bytes=8826
perf: kuramot100.odex wasm 3658 ms series=off events=41 bytes=9561
perf: kuramot100.odex wasm-comparison 1448 ms series=off events=22 bytes=8926
perf: kuramot100.odex wasm-comparison 1358 ms series=off events=21 bytes=8892
perf: kuramot100.odex wasm 3526 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1113 ms series=off events=18 bytes=8791
perf: kuramot100.odex native 1060 ms series=off events=18 bytes=8791
perf: kuramot100.odex wasm 3612 ms series=off events=41 bytes=9561
perf: kuramot100.odex wasm-comparison 1357 ms series=off events=21 bytes=8892
perf: kuramot100.odex wasm-comparison 1468 ms series=off events=22 bytes=8925
perf: kuramot100.odex wasm 3760 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1119 ms series=off events=18 bytes=8791
perf: kuramot100.odex native median 1113 ms (alternated samples: 1040, 1137, 1113, 1060, 1119)
perf: kuramot100.odex wasm median 3658 ms (alternated samples: 3681, 3658, 3526, 3612, 3760)
perf: kuramot100.odex wasm-comparison median 1430 ms (alternated samples: 1430, 1448, 1358, 1357, 1468)
```

## w184-series-off-unprofiled.log

```text
perf: kuramot100.odex native 1061 ms (warm-up) series=off events=18 bytes=8791
perf: kuramot100.odex wasm 3691 ms (warm-up) series=off events=41 bytes=9561
perf: kuramot100.odex wasm 3746 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1127 ms series=off events=18 bytes=8791
perf: kuramot100.odex native 1200 ms series=off events=19 bytes=8825
perf: kuramot100.odex wasm 3898 ms series=off events=41 bytes=9561
perf: kuramot100.odex wasm 3704 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1138 ms series=off events=19 bytes=8825
perf: kuramot100.odex native 1131 ms series=off events=18 bytes=8791
perf: kuramot100.odex wasm 3669 ms series=off events=41 bytes=9561
perf: kuramot100.odex wasm 3605 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1139 ms series=off events=19 bytes=8825
perf: kuramot100.odex native median 1138 ms (alternated samples: 1127, 1200, 1138, 1131, 1139)
perf: kuramot100.odex wasm median 3704 ms (alternated samples: 3746, 3898, 3704, 3669, 3605)
```

## w184-series-off.log

```text
perf: kuramot100.odex native 1113 ms (warm-up) series=off events=18 bytes=8791
perf: kuramot100.odex wasm 3829 ms (warm-up) series=off events=41 bytes=9561
perf: kuramot100.odex wasm 3869 ms series=off events=42 bytes=9594
perf: kuramot100.odex native 1083 ms series=off events=18 bytes=8791
perf: kuramot100.odex native 1114 ms series=off events=18 bytes=8791
perf: kuramot100.odex wasm 3699 ms series=off events=41 bytes=9561
perf: kuramot100.odex wasm 3715 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1117 ms series=off events=18 bytes=8791
perf: kuramot100.odex native 1123 ms series=off events=19 bytes=8825
perf: kuramot100.odex wasm 3765 ms series=off events=41 bytes=9561
perf: kuramot100.odex wasm 3742 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1101 ms series=off events=18 bytes=8791
perf: kuramot100.odex native median 1114 ms (alternated samples: 1083, 1114, 1117, 1123, 1101)
perf: kuramot100.odex wasm median 3742 ms (alternated samples: 3869, 3699, 3715, 3765, 3742)
```

## w184-series-on.log

```text
perf: kuramot100.odex native 1141 ms (warm-up) series=on events=81 bytes=19690
perf: kuramot100.odex wasm 3822 ms (warm-up) series=on events=243 bytes=35388
perf: kuramot100.odex wasm 3766 ms series=on events=242 bytes=35354
perf: kuramot100.odex native 1162 ms series=on events=86 bytes=20154
perf: kuramot100.odex native 1162 ms series=on events=85 bytes=20047
perf: kuramot100.odex wasm 3843 ms series=on events=242 bytes=35354
perf: kuramot100.odex wasm 3828 ms series=on events=242 bytes=35354
perf: kuramot100.odex native 1100 ms series=on events=73 bytes=18822
perf: kuramot100.odex native 1132 ms series=on events=78 bytes=19365
perf: kuramot100.odex wasm 3812 ms series=on events=242 bytes=35354
perf: kuramot100.odex wasm 3776 ms series=on events=242 bytes=35354
perf: kuramot100.odex native 1145 ms series=on events=83 bytes=19829
perf: kuramot100.odex native median 1145 ms (alternated samples: 1162, 1162, 1100, 1132, 1145)
perf: kuramot100.odex wasm median 3812 ms (alternated samples: 3766, 3843, 3828, 3812, 3776)
```

## w184-test-math.log

```text
xpp::math              106 checks, 0 failed
```

## Profile frames (microseconds)

```json
{
  "self_us": {
    "xpp::integrate(xpp::Session&, double*, double*, double, double, int, int, int*)": 4667,
    "xpp::(anonymous namespace)::eval_rpn(int const*, xpp::Session&)": 4702137,
    "fma": 11979336,
    "cr_sin": 5191473,
    "xpp::math::sin(double)": 249157
  },
  "inclusive_us": {
    "xpp::integrate(xpp::Session&, double*, double*, double, double, int, int, int*)": 22592890,
    "xpp::(anonymous namespace)::eval_rpn(int const*, xpp::Session&)": 22422296,
    "fma": 11979336,
    "cr_sin": 17372592,
    "xpp::math::sin(double)": 17715783
  }
}
```

## Chrome 154 / V8 15.4, Windows, final kernels

```text
{"protocolVersion":"1.3","product":"Chrome/154.0.8037.95","revision":"@05d469856e75794131cc2e5d9b2f6b6f10a70388","userAgent":"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) HeadlessChrome/154.0.0.0 Safari/537.36","jsVersion":"15.4.80.19"}
baseline fma_witness fused=-0x1p-54 unfused=0x0p+0 arithmetic fingerprint=a7e6b13e80453d25 samples_ms=20.955,20.86,20.84,20.93,19.505, median_ms=20.86 sin fingerprint=b5706e6c8c5aee20 samples_ms=198.535,200.015,199.345,199.365,198.875, median_ms=199.345 fma fingerprint=da23cfb980453d25 samples_ms=61.44,69.375,124.175,124.84,113.95, median_ms=113.95
PASS: browser baseline has all three native fingerprints and the exact rounding witness
ceiling fma_witness fused=-0x1p-54 unfused=0x0p+0 arithmetic fingerprint=a7e6b13e80453d25 samples_ms=30.93,31.72,30.69,33.65,30.375, median_ms=30.93 sin fingerprint=b5706e6c8c5aee20 samples_ms=97.98,86.42,94.13,105.465,106.82, median_ms=97.98 fma fingerprint=da23cfb980453d25 samples_ms=44.475,44.43,44.72,44.285,37.83, median_ms=44.43
PASS: browser ceiling has all three native fingerprints and the exact rounding witness
```
