# colormaps

The 17 colour maps xppautX adds to XPPAUT's seven (W492, #556), each a 256-entry table of 8 bit sRGB in
`colormaps.h`, sampled from matplotlib 3.11.2's data (`matplotlib/_cm_listed.py`, `_cm.py`) on 2026-10-24 by
rounding `cmap(i/255)` to 8 bit, except `seismic` and `hsv`, which are formulas. Licences checked
the same day against each source:

| map | designer | licence |
|---|---|---|
| `viridis`, `plasma`, `inferno`, `magma` | Nathaniel J. Smith, Stefan van der Walt (viridis also Eric Firing), [BIDS/colormap](https://github.com/BIDS/colormap) | CC0 1.0 (public domain dedication): "released under the CC0 license / public domain dedication" in its `colormaps.py` |
| `cividis` | Jamie R. Nunez, Christopher R. Anderton, Ryan S. Renslow, [pnnl/cmaputil](https://github.com/pnnl/cmaputil) | Battelle's BSD-style licence, below |
| `twilight` | Bastian Bechtold, [bastibe/twilight](https://github.com/bastibe/twilight), merged into matplotlib | no licence text in its own repository; used as distributed with matplotlib (matplotlib's licence, below) |
| `turbo` | Anton Mikhailov, Google | Apache License 2.0 (`Copyright 2019 Google LLC.`, `SPDX-License-Identifier: Apache-2.0`, gist ee72ba4191942acecc03fe6da94fc73f) |
| `greys`, `blues`, `reds`, `greens`, `oranges`, `purples`, `rdbu` | Cynthia Brewer, Mark Harrower and The Pennsylvania State University, [ColorBrewer](https://colorbrewer2.org/) (Greys, Blues, Reds, Greens, Oranges, Purples, RdBu), their anchor colours interpolated linearly as matplotlib does | Apache License 2.0 ("Apache-Style Software License for ColorBrewer"), below |
| `coolwarm` | Kenneth Moreland, "Diverging Color Maps for Scientific Visualization" (CoolWarmFloat33.csv, [kennethmoreland.com/color-maps](http://www.kennethmoreland.com/color-maps/)), interpolated by matplotlib | published by its author for free use; no licence text |
| `seismic` | blue (0, 0, 0.3) to blue, white, red, dark red (0.5, 0, 0), linear in between: a formula of ours | none needed |
| `hsv` | hue from 0 to 1 at full saturation and value (Python's `colorsys`): a formula of ours | none needed |

## ColorBrewer (and turbo): Apache License 2.0

Copyright (c) 2002 Cynthia Brewer, Mark Harrower, and The Pennsylvania State University.
Copyright 2019 Google LLC (turbo).

Licensed under the Apache License, Version 2.0 (the "License"); you may not use these files except in compliance with
the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.

## cividis: Battelle Memorial Institute

Copyright (c) 2017, Battelle Memorial Institute

1. Battelle Memorial Institute (hereinafter Battelle) hereby grants permission to any person or entity lawfully
obtaining a copy of this software and associated documentation files (hereinafter "the Software") to redistribute and
use the Software in source and binary forms, with or without modification. Such person or entity may use, copy,
modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and may permit others to do so,
subject to the following conditions:

+ Redistributions of source code must retain the above copyright notice, this list of conditions and the following
disclaimers.
+ Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
disclaimer in the documentation and/or other materials provided with the distribution.
+ Other than as used herein, neither the name Battelle Memorial Institute or Battelle may be used in any form
whatsoever without the express written consent of Battelle.

2. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL BATTELLE OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## matplotlib's licence (twilight)

matplotlib's licence is based on the Python Software Foundation licence version 2: copying, modification and
redistribution are permitted provided the copyright notice ("Copyright (c) 2012- Matplotlib Development Team; All
Rights Reserved") and the licence are kept with the copies, and the licence's disclaimer of warranty applies
(https://github.com/matplotlib/matplotlib/blob/main/LICENSE/LICENSE).
