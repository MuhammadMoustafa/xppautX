/* The AUTO view's status strip and its Output log (docs/ui-v2.md T21): what
   AUTO is doing (idle, waiting for how to start, running steady states or
   periodic orbits, stopping, done or stopped), the branch and point it got
   to, the points the run added, the last label it found and the time it
   took, with the one Stop (A10: the status bar's Stop does the same), and
   once a run ended, why its last branch ended (T23: "Stopped: parameter
   iapp reached Par Max (0.45)"; the core writes the same line in Output). The
   Output panel shows AUTO's console table and messages (the `log` lines
   store/state.ts classifies as AUTO's) as they arrive. */
import {useEffect, useRef, useState} from 'preact/hooks';
import {formatElapsed, runStatus} from '../plot/autoStatus';
import {useSession, useStore} from './context';

export function AutoStatus() {
  const session = useSession();
  const run = useStore(s => s.diagram.run);
  const points = useStore(s => s.diagram.points);
  const labels = useStore(s => s.diagram.labels);
  const stop = useStore(s => s.diagram.stop);
  const asking = useStore(s => !!s.ask);
  const stopping = useStore(s => s.stopping);
  const busy = useStore(s => s.busy);
  const [now, setNow] = useState(() => Date.now());
  const active = !!run?.active;
  /* the clock ticks while a run goes */
  useEffect(() => {
    if (!active) return;
    setNow(Date.now());
    const t = setInterval(() => setNow(Date.now()), 250);
    return () => clearInterval(t);
  }, [active]);
  const st = runStatus(run, points, labels, now, {asking, stopping}, stop);
  return (
    <div class="auto-status" data-phase={st.phase}>
      <span class={`status-dot ${st.phase === 'running' || st.phase === 'stopping' ? 'busy' : 'up'}`} aria-hidden="true" />
      <span class="auto-status-text" role="status" data-testid="auto-status" data-why={st.why ?? undefined}>
        <b>{st.text}</b>
        {st.detail && <span> · {st.detail}</span>}
        {st.branch !== null && <span> · branch {st.branch}, point {st.point}</span>}
        {run && <span> · {st.points} point{st.points === 1 ? '' : 's'}</span>}
        {st.label && <span> · last label {st.label}</span>}
        {st.elapsed !== null && <span> · {formatElapsed(st.elapsed)}</span>}
      </span>
      {busy && (
        <button class="small danger auto-stop" disabled={stopping} onClick={() => session.abort()}
          title="Stop the running command (AUTO keeps the points computed so far)">
          {stopping ? 'Stopping…' : 'Stop'}
        </button>
      )}
    </div>
  );
}

export function AutoOutput() {
  const log = useStore(s => s.log);
  const lines = log.filter(l => l.kind === 'auto');
  const list = useRef<HTMLPreElement>(null);
  const [open, setOpen] = useState(false);
  /* the newest line in view, as a console */
  useEffect(() => {
    const el = list.current;
    if (open && el) el.scrollTop = el.scrollHeight;
  }, [lines.length, open]);
  return (
    <details class="auto-output" open={open} onToggle={e => setOpen((e.target as HTMLDetailsElement).open)}>
      <summary>Output ({lines.length})</summary>
      {lines.length ? (
        <pre ref={list} class="auto-output-text" tabIndex={0} aria-label="AUTO's output">
          {lines.map(l => l.text.replace(/\n$/, '')).join('\n')}
        </pre>
      ) : <p class="muted">AUTO has printed nothing yet: its table appears here while it runs.</p>}
    </details>
  );
}
