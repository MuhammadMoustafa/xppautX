# W185 measurement samples

Companion to [the exact-FMA report](wasm-exact-fma.md). Application blocks alternate native, baseline and candidate; kernel blocks are separate. Timings never decide pass/fail.

## check-reviewed.log  ```text
build/w185/fma-baseline.js: 51825 MPFR 4.2.2 vectors, 0 failed
build/w185/fma-original.js: 51825 MPFR 4.2.2 vectors, 0 failed
build/w185/fma-int128.js: 51825 MPFR 4.2.2 vectors, 0 failed
```

## model-original.log  ```text
perf: kuramot100.odex native 1037 ms (warm-up) series=off events=17 bytes=8758
perf: kuramot100.odex wasm 3573 ms (warm-up) series=off events=41 bytes=9561
perf: kuramot100.odex wasm-comparison 3097 ms (warm-up) series=off events=36 bytes=9393
perf: kuramot100.odex wasm-comparison 3058 ms series=off events=36 bytes=9393
perf: kuramot100.odex wasm 3639 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1056 ms series=off events=18 bytes=8791
perf: kuramot100.odex native 1104 ms series=off events=18 bytes=8791
perf: kuramot100.odex wasm 3506 ms series=off events=41 bytes=9561
perf: kuramot100.odex wasm-comparison 3075 ms series=off events=36 bytes=9393
perf: kuramot100.odex wasm-comparison 3121 ms series=off events=36 bytes=9393
perf: kuramot100.odex wasm 3569 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1107 ms series=off events=18 bytes=8791
perf: kuramot100.odex native 1109 ms series=off events=18 bytes=8791
perf: kuramot100.odex wasm 3585 ms series=off events=41 bytes=9561
perf: kuramot100.odex wasm-comparison 3094 ms series=off events=36 bytes=9393
perf: kuramot100.odex wasm-comparison 3052 ms series=off events=36 bytes=9393
perf: kuramot100.odex wasm 3511 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1066 ms series=off events=18 bytes=8791
perf: kuramot100.odex native median 1104 ms (alternated samples: 1056, 1104, 1107, 1109, 1066)
perf: kuramot100.odex wasm median 3569 ms (alternated samples: 3639, 3506, 3569, 3585, 3511)
perf: kuramot100.odex wasm-comparison median 3075 ms (alternated samples: 3058, 3075, 3121, 3094, 3052)
all passed
```

## model-int128.log  ```text
perf: kuramot100.odex native 1034 ms (warm-up) series=off events=17 bytes=8758
perf: kuramot100.odex wasm 3617 ms (warm-up) series=off events=41 bytes=9561
perf: kuramot100.odex wasm-comparison 3293 ms (warm-up) series=off events=37 bytes=9426
perf: kuramot100.odex wasm-comparison 3253 ms series=off events=37 bytes=9427
perf: kuramot100.odex wasm 3476 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1041 ms series=off events=17 bytes=8758
perf: kuramot100.odex native 1031 ms series=off events=17 bytes=8758
perf: kuramot100.odex wasm 3526 ms series=off events=41 bytes=9561
perf: kuramot100.odex wasm-comparison 3265 ms series=off events=38 bytes=9461
perf: kuramot100.odex wasm-comparison 3256 ms series=off events=37 bytes=9427
perf: kuramot100.odex wasm 3684 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1062 ms series=off events=18 bytes=8791
perf: kuramot100.odex native 1124 ms series=off events=18 bytes=8791
perf: kuramot100.odex wasm 3642 ms series=off events=41 bytes=9561
perf: kuramot100.odex wasm-comparison 3423 ms series=off events=41 bytes=9561
perf: kuramot100.odex wasm-comparison 3206 ms series=off events=36 bytes=9393
perf: kuramot100.odex wasm 3616 ms series=off events=41 bytes=9561
perf: kuramot100.odex native 1100 ms series=off events=18 bytes=8791
perf: kuramot100.odex native median 1062 ms (alternated samples: 1041, 1031, 1062, 1124, 1100)
perf: kuramot100.odex wasm median 3616 ms (alternated samples: 3476, 3526, 3684, 3642, 3616)
perf: kuramot100.odex wasm-comparison median 3256 ms (alternated samples: 3253, 3265, 3256, 3423, 3206)
all passed
```

## math-original.log  ```text
fma_witness fused=-0x1p-54 unfused=0x0p+0
arithmetic fingerprint=a7e6b13e80453d25 samples_ms=11.6383,11.6204,11.6018,11.5331,11.5035, median_ms=11.6018
sin fingerprint=b5706e6c8c5aee20 samples_ms=144.053,144.052,144.402,144.568,144.217, median_ms=144.217
fma fingerprint=da23cfb980453d25 samples_ms=40.8481,40.7652,40.8393,40.7885,40.7307, median_ms=40.7885
```

## math-int128.log  ```text
fma_witness fused=-0x1p-54 unfused=0x0p+0
arithmetic fingerprint=a7e6b13e80453d25 samples_ms=12.3233,11.7171,12.8744,12.0839,11.8206, median_ms=12.0839
sin fingerprint=b5706e6c8c5aee20 samples_ms=185.944,162.045,154.991,155.081,155.049, median_ms=155.081
fma fingerprint=da23cfb980453d25 samples_ms=44.4759,44.3827,44.434,44.271,44.3995, median_ms=44.3995
```

## test-math-original.log  ```text
xpp::math              106 checks, 0 failed
```

## test-math-int128.log  ```text
xpp::math              106 checks, 0 failed
```

## w185-browser.log  ```text
{"protocolVersion":"1.3","product":"Chrome/154.0.8037.95","revision":"@05d469856e75794131cc2e5d9b2f6b6f10a70388","userAgent":"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) HeadlessChrome/154.0.0.0 Safari/537.36","jsVersion":"15.4.80.19"}
baseline: runtime validation true
baseline {"cases":51825,"failures":0,"firstMismatch":null}
original: runtime validation true
original {"cases":51825,"failures":0,"firstMismatch":null}
int128: runtime validation true
int128 {"cases":51825,"failures":0,"firstMismatch":null}
wide: runtime validation false
```
