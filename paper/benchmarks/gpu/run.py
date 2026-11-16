"""W268 (#325): the timed rerun of questions 2 and 3's timings, for the quiet window after W718. Not run until then (the probe numbers in
docs/studies/w268-gpu.md are untimed).

    uv run --locked --group gpu python paper/benchmarks/gpu/run.py --timed --bin ./xppautX --out ~/xppautx-gpu/w268-timed

It refuses to start unless `tools/comparison-setup.sh --ready` passes (the governor, no other process on the CPU: the same wait as
paper/benchmarks/comparison/run.py, reused from its common.py) and no other process computes on the GPU. It then runs q2_sweep's
throughput (the sweep at N = 1e3, 1e4, 1e5: the GPU's three variants, xppautX in 1 and 16 processes) and q3_network's integrations
(200, 2000, 20000 cells), with 10 repeats on the GPU and 5 on the CPU, and keeps the machine record and the output beside each other."""
import argparse
import contextlib
import io
from pathlib import Path
import subprocess
import sys

from gpucommon import REPO, binary
sys.path.insert(0, str(REPO / 'paper' / 'benchmarks' / 'comparison'))
from common import TOOLS, wait_until_ready  # noqa: E402

import q2_sweep  # noqa: E402
import q3_network  # noqa: E402

GPU_REPEATS, CPU_REPEATS = 10, 5


def gpu_is_free():
    """No process other than the desktop's graphics clients computes on the GPU (nvidia-smi lists compute processes)."""
    out = subprocess.run(['nvidia-smi', '--query-compute-apps=pid,name', '--format=csv,noheader'], capture_output=True, text=True, check=True).stdout.strip()
    return out == '', out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--timed', action='store_true', help='required: this script exists to time')
    ap.add_argument('--bin')
    ap.add_argument('--out', required=True, type=Path)
    a = ap.parse_args()
    if not a.timed:
        sys.exit('run.py times: pass --timed (and run it only in the quiet window)')
    wait_until_ready()
    free, who = gpu_is_free()
    if not free:
        sys.exit('the GPU is in use (%s): refusing to time' % who)
    a.out.mkdir(parents=True, exist_ok=True)
    record = subprocess.run(['bash', str(TOOLS / 'comparison-setup.sh'), '--record'], cwd=REPO, check=True, capture_output=True, text=True).stdout
    (a.out / 'machine.txt').write_text(record)
    bin_path = binary(a.bin)
    for name, work in (('q2_sweep', lambda: q2_sweep.throughput(bin_path, [1000, 10000, 100000], GPU_REPEATS, CPU_REPEATS)),
                       ('q3_network', lambda: q3_network.measure(bin_path, [200, 2000, 20000], GPU_REPEATS, CPU_REPEATS))):
        wait_until_ready()
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            work()
        (a.out / (name + '.txt')).write_text(buf.getvalue())
        print(buf.getvalue())


if __name__ == '__main__':
    main()
