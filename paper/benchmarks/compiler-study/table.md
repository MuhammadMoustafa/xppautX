| model | compiler | instr/call (total) | model-code instr/call | ns/call | compile ms | identical to TinyCC |
|---|---|---:|---:|---:|---:|---|
| mu26 | TinyCC | 9043 | 3790 | 899 | 0.9 | yes |
| mu26 | gcc -O2 | 6507 | 1254 | 664 | 410.8 | yes |
| mu26 | clang -O2 | 6613 | 1360 | 702 | 229.1 | yes |
| mu26 | MIR O1 | 8740 | 3487 | 884 | 37.1 | yes |
| mu26 | MIR O2 (default) | 6952 | 1699 | 771 | 37.9 | yes |
| ma23 | TinyCC | 2737 | 1276 | 253 | 0.4 | yes |
| ma23 | gcc -O2 | 1844 | 383 | 179 | 138.4 | yes |
| ma23 | clang -O2 | 1875 | 414 | 185 | 102.4 | yes |
| ma23 | MIR O1 | 2529 | 1068 | 231 | 11.2 | yes |
| ma23 | MIR O2 (default) | 2080 | 619 | 213 | 11.1 | yes |
| motoneuron6c | TinyCC | 10309 | 5349 | 951 | 5.4 | yes |
| motoneuron6c | gcc -O2 | 6788 | 1828 | 598 | 500.5 | yes |
| motoneuron6c | clang -O2 | 6749 | 1789 | 606 | 289.5 | yes |
| motoneuron6c | MIR O1 | 9341 | 4381 | 810 | 40.3 | yes |
| motoneuron6c | MIR O2 (default) | 7481 | 2521 | 666 | 41.2 | yes |
| PY_S1Bf_bench | TinyCC | 2762 | 1035 | 235 | 0.2 | yes |
| PY_S1Bf_bench | gcc -O2 | 2150 | 423 | 185 | 70.4 | yes |
| PY_S1Bf_bench | clang -O2 | 2105 | 378 | 187 | 66.6 | yes |
| PY_S1Bf_bench | MIR O1 | 2763 | 1036 | 243 | 6.2 | yes |
| PY_S1Bf_bench | MIR O2 (default) | 2230 | 503 | 169 | 6.0 | yes |
| lecdiff200 | TinyCC | 137205 | 63420 | 15164 | 1.1 | yes |
| lecdiff200 | gcc -O2 | 96391 | 22606 | 10472 | 495.3 | yes |
| lecdiff200 | clang -O2 | 94190 | 20405 | 12972 | 575.0 | yes |
| lecdiff200 | MIR O1 | 120604 | 46819 | 14980 | 51.2 | yes |
| lecdiff200 | MIR O2 (default) | 100792 | 27007 | 11744 | 50.7 | yes |
| kuramot100 | TinyCC | 3613953 | 733700 | 381672 | 0.7 | yes |
| kuramot100 | gcc -O2 | 3263755 | 383502 | 281126 | 420.6 | yes |
| kuramot100 | clang -O2 | 3213954 | 333701 | 282171 | 260.3 | yes |
| kuramot100 | MIR O1 | 3455453 | 575200 | 391928 | 28.4 | yes |
| kuramot100 | MIR O2 (default) | 3234353 | 354100 | 377514 | 34.6 | yes |
| amari | TinyCC | 13441 | 7608 | 1596 | 1.0 | yes |
| amari | gcc -O2 | 12038 | 6205 | 1546 | 389.6 | yes |
| amari | clang -O2 | 11037 | 5204 | 1546 | 274.0 | yes |
| amari | MIR O1 | 14240 | 8407 | 2479 | 33.4 | yes |
| amari | MIR O2 (default) | 11240 | 5407 | 1869 | 34.0 | yes |
| clustor | TinyCC | 115555 | 57348 | 9430 | 3.3 | yes |
| clustor | gcc -O2 | 83566 | 25359 | 11621 | 1219.1 | yes |
| clustor | clang -O2 | 83864 | 25657 | 14744 | 721.7 | yes |
| clustor | MIR O1 | 117752 | 59545 | 19730 | 175.9 | yes |
| clustor | MIR O2 (default) | 82001 | 23794 | 16515 | 154.6 | yes |
| episodic | TinyCC | 69767 | 29245 | 6139 | 0.8 | yes |
| episodic | gcc -O2 | 51224 | 10702 | 4332 | 589.3 | yes |
| episodic | clang -O2 | 50241 | 9719 | 4471 | 311.5 | yes |
| episodic | MIR O1 | 63118 | 22596 | 5131 | 36.8 | yes |
| episodic | MIR O2 (default) | 54315 | 13793 | 4636 | 38.9 | yes |
| izh_2003_rs | TinyCC | 178 | 97 | 11 | 0.1 | yes |
| izh_2003_rs | gcc -O2 | 124 | 43 | 11 | 25.3 | yes |
| izh_2003_rs | clang -O2 | 118 | 37 | 11 | 41.1 | yes |
| izh_2003_rs | MIR O1 | 186 | 105 | 14 | 1.2 | yes |
| izh_2003_rs | MIR O2 (default) | 128 | 47 | 13 | 1.2 | yes |
| izh_2003_fs | TinyCC | 178 | 97 | 11 | 0.1 | yes |
| izh_2003_fs | gcc -O2 | 124 | 43 | 11 | 25.2 | yes |
| izh_2003_fs | clang -O2 | 118 | 37 | 11 | 40.1 | yes |
| izh_2003_fs | MIR O1 | 186 | 105 | 14 | 1.2 | yes |
| izh_2003_fs | MIR O2 (default) | 128 | 47 | 13 | 1.2 | yes |
| izh_2004c_tonic_bursting | TinyCC | 178 | 97 | 11 | 0.1 | yes |
| izh_2004c_tonic_bursting | gcc -O2 | 124 | 43 | 11 | 25.0 | yes |
| izh_2004c_tonic_bursting | clang -O2 | 118 | 37 | 11 | 40.2 | yes |
| izh_2004c_tonic_bursting | MIR O1 | 186 | 105 | 14 | 1.2 | yes |
| izh_2004c_tonic_bursting | MIR O2 (default) | 128 | 47 | 13 | 1.2 | yes |
| izh_2004e_mixed_mode | TinyCC | 178 | 97 | 11 | 0.1 | yes |
| izh_2004e_mixed_mode | gcc -O2 | 124 | 43 | 11 | 25.2 | yes |
| izh_2004e_mixed_mode | clang -O2 | 118 | 37 | 11 | 40.3 | yes |
| izh_2004e_mixed_mode | MIR O1 | 186 | 105 | 14 | 1.2 | yes |
| izh_2004e_mixed_mode | MIR O2 (default) | 128 | 47 | 13 | 1.3 | yes |
| izh_2004g_class1_excitable | TinyCC | 184 | 103 | 11 | 0.1 | yes |
| izh_2004g_class1_excitable | gcc -O2 | 128 | 47 | 11 | 25.7 | yes |
| izh_2004g_class1_excitable | clang -O2 | 121 | 40 | 11 | 40.4 | yes |
| izh_2004g_class1_excitable | MIR O1 | 198 | 117 | 14 | 1.2 | yes |
| izh_2004g_class1_excitable | MIR O2 (default) | 132 | 51 | 13 | 1.2 | yes |

Geometric-mean ratios against TinyCC over the models (below 1 = fewer / faster):
| compiler | model-code instr | total instr | ns/call | compile ms (x) |
|---|---:|---:|---:|---:|
| gcc -O2 | 0.42 | 0.73 | 0.86 | 285 |
| clang -O2 | 0.39 | 0.71 | 0.89 | 266 |
| MIR O1 | 0.95 | 0.99 | 1.15 | 20 |
| MIR O2 (default) | 0.48 | 0.76 | 1.00 | 20 |
