/* The common run actions have one permanent home, above the plots.
   Stable command identities reuse the core's existing run semantics. */
import {BUSY_TITLE, useMayMain, useSession, useStore} from './context';
import {inspectNumber} from '../store/values';

export function RunToolbar() {
  const session = useSession();
  const mayMain = useMayMain();
  const hasNow = useStore(s => !!s.core?.now);
  const busy = useStore(s => s.busy);
  const computing = useStore(s => s.computing);
  const stopping = useStore(s => s.stopping);
  const asking = useStore(s => !!s.ask);
  const storedTime = useStore(s => {
    const series = s.plots.windows.find(w => w.win === s.plots.active)?.series;
    return series?.rows ? series.columns.get(0)?.[series.rows - 1] : undefined;
  });
  const initialOff = !mayMain('initialconds');
  const continueOff = !mayMain('continue') || !hasNow;
  const currentOff = initialOff || !hasNow;
  return <section class="run-toolbar" aria-label="Run controls">
    <div class="run-actions">
      {/* Keep the focus while a run starts so subsequent legacy keys still work. */}
      <button class="primary" data-button="Integrate" aria-disabled={initialOff}
        onClick={() => { if (!initialOff) session.menuAction('main', 'initialconds', 'g'); }}
        title={initialOff ? BUSY_TITLE : 'Start a new trajectory from Initial values at the configured start time (I, G)'}>Run from initial</button>
      <button data-run="current" aria-disabled={currentOff}
        onClick={() => { if (!currentOff) session.menuAction('main', 'initialconds', 'l'); }}
        title={initialOff ? BUSY_TITLE : !hasNow ? 'Run once to obtain a current state' : 'Use the last state as Initial and start a new trajectory at the configured start time (I, L)'}>Run from current</button>
      <button data-run="continue" aria-disabled={continueOff}
        onClick={() => { if (!continueOff) session.menuAction('main', 'continue'); }}
        title={!mayMain('continue') ? BUSY_TITLE : !hasNow ? 'Run once before continuing' : 'Extend the existing trajectory to a chosen end time (C)'}>Continue…</button>
      <button class="danger" data-run="stop" disabled={!busy || stopping} onClick={() => session.abort()}
        title="Stop the running command or cancel its prompt (Esc)">{stopping ? 'Stopping…' : 'Stop'}</button>
    </div>
    <div class="run-context">
      <strong role="status">{stopping ? 'Stopping…' : computing ? 'Running' : asking ? 'Awaiting input' : busy ? 'Working…' : 'Idle'}</strong>
      {storedTime !== undefined && Number.isFinite(storedTime) && <span title="Time of the last stored point in the active plot; sampled trajectory precision">Stored t = {inspectNumber(storedTime)}</span>}
      <span>Parameter edits apply to the next run.</span>
    </div>
  </section>;
}
