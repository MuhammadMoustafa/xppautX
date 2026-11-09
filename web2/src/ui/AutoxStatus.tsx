/* The autoX view's status strip and its Output log (docs/ui-v2.md T21): what
   autoX is doing (idle, waiting for how to start, running steady states or
   periodic orbits, stopping, done or stopped), the branch and point it got
   to, the points the run added, the last label it found and the time it
   took, with the one Stop (A10: the status bar's Stop does the same), and
   once a run ended, why its last branch ended (T23: "Stopped: parameter
   iapp reached parameter_max (0.45)"; the core writes the same line in Output).
   T26: the autoX view now covers the main status bar too, so this strip also
   carries what it says that matters here: the core's last message
   (store's `bottom`, StatusBar.tsx's own `.status-message`) and the
   connection state when it is not simply connected (Connecting…, XPP has
   stopped), with the dot to match. The Output panel shows autoX's console
   table and messages (the `log` lines store/state.ts classifies as autoX's)
   as they arrive. */
import {useEffect, useMemo, useRef, useState} from 'preact/hooks';
import {formatElapsed, jobsText, runStatus} from '../plot/autoxStatus';
import {activeView} from '../store/diagram';
import {logEntries} from '../store/log';
import {useSession, useStore} from './context';
import {CommandLabel} from './CommandButton';
import {connectionText} from './StatusBar';

export function AutoxStatus() {
  const session = useSession();
  const run = useStore(s => s.diagram.run);
  const points = useStore(s => activeView(s.diagram).points);
  const labels = useStore(s => activeView(s.diagram).labels);
  const stop = useStore(s => s.diagram.stop);
  const jobs = useStore(s => jobsText(s.diagram.jobs));
  const asking = useStore(s => !!s.ask);
  const stopping = useStore(s => s.stopping);
  const busy = useStore(s => s.busy);
  const connected = useStore(s => s.connected);
  const exited = useStore(s => s.exited);
  const bottom = useStore(s => s.bottom);
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
  const conn = connectionText(connected, exited);
  const dot = exited !== null ? 'down' : !connected ? '' : st.phase === 'running' || st.phase === 'stopping' ? 'busy' : 'up';
  return (
    <div class="autox-status" data-phase={st.phase}>
      <span class={`status-dot ${dot}`} aria-hidden="true" />
      <span class="autox-status-text" role="status" data-testid="autox-status" data-why={st.why ?? undefined}>
        <b>{st.text}</b>
        {st.detail && <span> · {st.detail}</span>}
        {st.branch !== null && <span> · branch {st.branch}, point {st.point}</span>}
        {run && <span> · {st.points} point{st.points === 1 ? '' : 's'}</span>}
        {active && jobs && <span data-testid="autox-jobs"> · {jobs}</span>}
        {st.label && <span> · last label {st.label}</span>}
        {st.elapsed !== null && <span> · {formatElapsed(st.elapsed)}</span>}
      </span>
      {conn && <span class="autox-conn" role="status" data-testid="autox-conn">{conn}</span>}
      {bottom && <span class="autox-message muted">{bottom}</span>}
      {busy && (
        <button class="small danger autox-stop" disabled={stopping} onClick={() => session.abort()}
          title="Stop the running command (autoX keeps the points computed so far)">
          {stopping ? 'Stopping…' : <CommandLabel command="run.stop" />}
        </button>
      )}
    </div>
  );
}

export function AutoxOutput() {
  const log = useStore(s => s.log);
  const list = useRef<HTMLPreElement>(null);
  const [open, setOpen] = useState(false);
  const count = log.counts.autox;
  /* the lines only while the panel is open: a long run's table is thousands of them */
  const text = useMemo(() => (open ? logEntries(log).filter(l => l.kind === 'autox').map(l => l.text.replace(/\n$/, '')).join('\n') : ''),
    [open, log]);
  /* the newest line in view, as a console */
  useEffect(() => {
    const el = list.current;
    if (open && el) el.scrollTop = el.scrollHeight;
  }, [count, open]);
  return (
    <details class="autox-output" open={open} onToggle={e => setOpen((e.target as HTMLDetailsElement).open)}>
      <summary>Output ({count})</summary>
      {count ? (
        <pre ref={list} class="autox-output-text" tabIndex={0} aria-label="autoX's output">
          {text}
        </pre>
      ) : <p class="muted">autoX has printed nothing yet: its table appears here while it runs.</p>}
    </details>
  );
}
