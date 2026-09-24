/* What the AUTO view's status strip says (docs/ui-v2.md T21): whether AUTO
   is idle, waiting for the Start menu, running (steady states, periodic
   orbits, a two-parameter curve) or stopping, the branch and point it got
   to, how many points the run added, the last label it found and how long
   it took. Read from the store: the run (store/diagram.ts AutoRun) and the
   diagram's points from the run's first on. Pure. */
import type {AutoRun, DiagramLabel, DiagramPoints} from '../store/diagram';

export interface RunStatus {
  /** 'idle' | 'starting' | 'running' | 'stopping' | 'done' | 'stopped' */
  phase: 'idle' | 'starting' | 'running' | 'stopping' | 'done' | 'stopped';
  /** the phase in words, with what is computed: "Running: periodic orbits" */
  text: string;
  /** what the last point of the run is: "steady states", "periodic orbits", "two-parameter curve", or '' */
  kind: string;
  branch: number | null;
  point: number | null;
  /** points the run added */
  points: number;
  /** the last label the run found: "HB 2 at point 19" */
  label: string | null;
  /** ms since the run started (to its end once it ended) */
  elapsed: number | null;
}

export function kindOfPoint(ty: number, f2: number): string {
  if (f2) return 'two-parameter curve';
  return ty === 3 || ty === 4 ? 'periodic orbits' : ty === 1 || ty === 2 ? 'steady states' : '';
}

/** "0.4 s", "12 s", "2:05" */
export function formatElapsed(ms: number): string {
  const s = Math.max(0, ms) / 1000;
  if (s < 10) return `${s.toFixed(1)} s`;
  if (s < 60) return `${Math.floor(s)} s`;
  const m = Math.floor(s / 60);
  return `${m}:${String(Math.floor(s - 60 * m)).padStart(2, '0')}`;
}

export function runStatus(run: AutoRun | null, p: DiagramPoints, labels: DiagramLabel[], now: number,
  flags: {asking: boolean; stopping: boolean}): RunStatus {
  if (!run) return {phase: 'idle', text: 'Idle', kind: '', branch: null, point: null, points: 0, label: null, elapsed: null};
  const n = p.x.length, first = Math.min(run.first, n), last = n - 1;
  const added = n - first;
  const kind = added > 0 ? kindOfPoint(p.ty[last], p.f2[last]) : '';
  const lab = [...labels].reverse().find(l => l.point >= first && l.point < n);
  const label = lab ? `${lab.sym || 'Label'} ${lab.lab} at point ${p.pt[lab.point]}` : null;
  const elapsed = (run.ended ?? now) - run.started;
  const base = {kind, branch: added > 0 ? p.br[last] : null, point: added > 0 ? p.pt[last] : null, points: added, label,
    elapsed};
  if (run.active) {
    if (flags.stopping) return {...base, phase: 'stopping', text: 'Stopping…'};
    if (flags.asking && added === 0) return {...base, phase: 'starting', text: 'Waiting for how to start', elapsed: null};
    return {...base, phase: 'running', text: `Running: ${kind || 'starting'}`};
  }
  const what = kind ? `: ${kind}` : '';
  return run.stopped ? {...base, phase: 'stopped', text: `Stopped${what}`} : {...base, phase: 'done', text: `Done${what}`};
}
