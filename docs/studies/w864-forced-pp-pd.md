# W864: forced_pp_pd's steady rows against W617 (#944)

Measured only; no source changed. The steady row is the `steady` run of
`tests/autox/forced_pp_pd.json` (an equilibrium continuation in `m` from the
current state, 100 steps, 15 mesh intervals), timed by the driver's own window
(`autox.run` sent to its `done`, `RunClock`), one CPU (`taskset -c 0`), one
thread, the exact Jacobian, diagram events on. Each binary runs with the
harness (`tools/`, `tests/autox/`, `python/`) of its own commit, since the
`--server` command names changed over the range.

## Numbers

| Binary (commit) | Median steady run | Note |
|---|---|---|
| W617 `5975912b` (code of `1bd5334`) | 1.24 ms (n=20) | the reference |
| W718 `f1515877` (W846) | 1.51 ms (n=20) | 1.22x W617 |
| same, unpinned | 1.70 ms (n=15) vs 1.25 ms W617 unpinned | 1.36x |
| W718 pass-1 (`w718-20261121a`) | 1.99 ms | not reproduced here |

Bisect, pinned, 8 runs each, in the order of the commits from `0d4666f9` (W717)
to `c14fd05`:

| Commit | Median | |
|---|---|---|
| `7c2f208e` W714 (MIR adopted) | 1.29 ms | W617 1.24 beside it |
| `0d4666f9` W717 (first tier) | 1.27 ms | fine |
| `81a4e15b` W742 | 1.29 ms | good |
| `211257cd` roadmap | 1.28 ms | good (parent of the next) |
| **`dfd8464c` W739 "autoX's scratch files through the sink" (#818)** | **1.41 ms** | **first slow** |
| `5bde719f` W736 | 1.41 ms | |
| `a9472169` W747 in progress | 1.46 ms | |
| `256a841b` | 1.45 ms | |
| `c14fd05b` W718 python integration | 1.39 ms | |
| `e8a8270c` W799 | 1.53 ms | |
| `f1515877` W846 | 1.51 ms | |

The first slow commit is on the first-parent line, its parent `211257cd` measured good.

## Cause

W739 (`dfd8464c`) moved autoX's scratch files (fort.3, .7, .8, .9) from open
streams to the `Sink` (temp file, commit by rename). The run's end then copies
each kept output through `copy_into`: the scratch file is written to a new
`.forced_predator_prey.odex.X.tmp`, read back, and renamed, where before it
was renamed directly. `strace` of one steady run, before and after, differs
exactly there (the other 40-odd calls are the same): 3 created temp files
(O_EXCL), 3 renames, 3 extra opens of the scratch file for reading, 3 unlinks
of the scratch files; `openat` 70 to 78, `rename` 6 to 9, `unlink` 9 to 8.
These are fixed per-run costs, not a per-point one, so they weigh most on a
run of about a millisecond. The steady run's own work is the same in both.

## Trade-off or defect

Not a trade-off: no other workload was sped up by W739, and it is not one of
the native-tier decisions (W714, W715, W717, W821, W839, W841, W846). The
bisect puts the tiers at or below the noise: W714 and W717 are within 4% of
W617 on this row. It is a cost of the file rule that W739 applied (every
write of ours through the `Sink`, the inner rings opening no files; W731,
W739, W740), which has no rename-into-place for a file already written, so
the copy stands in for the move. This is a defect of the implementation, a
fixed cost paid on every autoX run.

Fix cost (not done here): give the Sink seam a way to commit a file the
owner has already written (a move, or the scratch file opened under the
final temp name from the start), used by the three commit sites of W739 and
by the `.odex.b/.d/.s` copies. That touches the kernel's FileSystem interface,
LocalFileSystem and the in-memory test file system: a small card. Expected
gain: the 0.13 ms of the bisect's first step.

## Not located

- `dfd8464c` to `f1515877` is about 1.41 to 1.51 ms; W740 to W747 add some
  0.05 ms (`a9472169`, 1.46) and are not bisected. They are the same family
  of file changes (writers and exporters through the sink, W740), not
  verified here.
- `c14fd05` (1.39) to `e8a8270` (1.53), a step of about 0.14 ms, is not
  bisected (about 8 builds in that range). It is not the tiers: the native
  commits W816 onward sit after it.
- The W718 pass-1 figure (1.99 ms) is not reproduced: here the ratio is 1.22x
  pinned and 1.36x unpinned. An unpinned run has MIR's build thread on a spare
  CPU; the pass's measurement conditions are not recorded in its files.
- callgrind on the whole process shows MIR's code generator (about 12.6 M
  self instructions) in f1515877 and none in W617, but this includes startup and
  is not split from the run; its share of the steady row is unmeasured.
