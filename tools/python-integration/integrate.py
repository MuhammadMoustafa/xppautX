#!/usr/bin/env python3
"""tools/python-integration/integrate.py: runs an .odex model with xppautX's methods written in Python (W427, #489).

    uv run python tools/python-integration/integrate.py MODEL.odex [--method KEY] [--setfile FILE] [--outfile FILE]

Writes the columns xppautX's output.dat has (time, the variables, the auxiliary quantities) at the
same stored times. The method is the model's own unless --method names another of driver.METHODS
(the keys .odex writes: euler, runge_kutta, discrete, backward_euler, qualrk, dopri8).
"""

import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # tools/: odex_model

from driver import FAILURES, METHODS, format_rows, load, run_model


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('model', type=Path, help='the .odex model')
    parser.add_argument('--method', choices=METHODS, help="a method other than the model's own")
    parser.add_argument('--setfile', type=Path, help='an XPPAUT set file to apply (as xppautX --setfile)')
    parser.add_argument('--outfile', type=Path, default=Path('output.dat'), help='the output (default output.dat)')
    args = parser.parse_args()
    try:
        model, numerics = load(args.model, args.setfile)
        run = run_model(model, numerics, args.method)
    except FAILURES as e:
        sys.exit('%s: %s' % (args.model, e))
    args.outfile.write_text(format_rows(run.rows), encoding='utf-8')


if __name__ == '__main__':
    main()
