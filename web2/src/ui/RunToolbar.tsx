/* Direct run actions and inline settings; no mandatory intermediate dialog. */
import {useEffect, useState} from 'preact/hooks';
import {BUSY_TITLE, useMay, useMayMain, useSession, useStore} from './context';
import {fieldKey, inspectNumber, sentText} from '../store/values';
import {steadyError, type SteadyInput} from '../store/steady';
import {NUMBER} from '../store/fieldKinds';
import {Field} from './Field';

export function RunToolbar() {
  const session = useSession();
  const mayMain = useMayMain(), may = useMay();
  const hello = useStore(s => s.hello);
  const core = useStore(s => s.core);
  const hasNow = !!core?.now;
  const busy = useStore(s => s.busy);
  const computing = useStore(s => s.computing);
  const stopping = useStore(s => s.stopping);
  const asking = useStore(s => !!s.ask);
  const total = useStore(s => s.numerics?.find(f => f.key === 'total'));
  const dt = useStore(s => s.numerics?.find(f => f.key === 'dt')?.value);
  const discrete = useStore(s => {const f = s.numerics?.find(f => f.key === 'method'); return f?.choices?.[Number(f.value)] === 'Discrete';});
  const interval = discrete ? 1 : Number(dt);
  const durationField = fieldKey('num', 'total');
  const sentDuration = useStore(s => sentText(s.values.inflight, durationField));
  const durationError = useStore(s => s.values.errors[durationField] ?? null);
  const storedTime = useStore(s => {
    const series = s.plots.windows.find(w => w.win === s.plots.active)?.series;
    return series?.rows ? series.columns.get(0)?.[series.rows - 1] : undefined;
  });
  const [steady, setSteady] = useState<SteadyInput | null>(null);
  const [continueMode, setContinueMode] = useState<'extra' | 'until'>('extra');
  const [continueValue, setContinueValue] = useState<string | null>(null);
  useEffect(() => {
    if (!hello || total?.value === undefined || total.value === null || !Number.isFinite(interval) || interval <= 0) return;
    if (!steady) setSteady({decimals: String(hello.steady.default_decimals),
      hold: String(Math.min(total.value, Math.max(interval, hello.steady.default_hold))), maximum: String(total.value)});
    if (continueValue === null) setContinueValue(String(total.value));
  }, [hello, total?.value, interval, steady, continueValue]);
  const initialOff = !mayMain('initialconds');
  const currentOff = initialOff || !hasNow;
  const steadyProblem = steady && hello ? steadyError(steady, interval, hello.steady.max_decimals) : null;
  const steadyOff = !may({cmd: 'steady'}) || !steady || !!steadyProblem;
  const continueNumber = Number(continueValue);
  const continueDuration = continueMode === 'extra' ? continueNumber : continueNumber - Number(core?.time);
  const tooShort = continueMode === 'extra' ? continueNumber < interval : continueNumber < Number(core?.time) + interval;
  const continueProblem = continueValue !== null && (!continueValue.trim() || !Number.isFinite(continueDuration) || tooShort)
    ? 'Choose at least one Dt of additional time.' : null;
  const continueOff = !may({cmd: 'continue'}) || !hasNow || continueValue === null || !!continueProblem;
  const changeMode = (mode: 'extra' | 'until') => {
    if (mode === continueMode) return;
    const time = Number(core?.time);
    if (Number.isFinite(time) && continueValue !== null)
      setContinueValue(String(mode === 'until' ? time + continueNumber : continueNumber - time));
    setContinueMode(mode);
  };
  const result = core?.steady;
  const resultText = result?.status === 'settled' ? `State values stopped changing at ${result.decimals} decimal places.`
    : result?.status === 'limit' ? 'Maximum duration reached; unchanged-digits hold not completed.'
    : result?.status === 'storage-limit' ? 'Storage limit reached before the unchanged-digits hold completed.'
    : result?.status === 'stopped' ? 'Steady-state run stopped by the user or model event.'
    : result ? 'Steady-state run failed; see the error.' : null;
  return <section class="run-toolbar" aria-label="Run controls">
    <div class="run-actions">
      <button class="primary" data-button="Integrate" aria-disabled={initialOff}
        onClick={() => { if (!initialOff) session.menuAction('main', 'initialconds', 'g'); }}
        title={initialOff ? BUSY_TITLE : 'Start a new trajectory from Initial values (I, G)'}>Run from initial</button>
      <button data-run="current" aria-disabled={currentOff}
        onClick={() => { if (!currentOff) session.menuAction('main', 'initialconds', 'l'); }}
        title={initialOff ? BUSY_TITLE : !hasNow ? 'Run once to obtain a current state' : 'Use the last state as Initial and start a new trajectory (I, L)'}>Run from current</button>
      <button data-run="steady" aria-disabled={steadyOff}
        onClick={() => { if (!steadyOff && steady) session.send({cmd: 'steady', decimals: Number(steady.decimals), hold: Number(steady.hold), maximum: Number(steady.maximum)}); }}
        title={initialOff ? BUSY_TITLE : steadyProblem ?? 'Run from Initial until every state stops changing at the configured precision'}>Run to steady state</button>
      <button data-run="continue" aria-disabled={continueOff}
        onClick={() => { if (!continueOff) session.send({cmd: 'continue', [continueMode]: continueNumber}); }}
        title={!may({cmd: 'continue'}) ? BUSY_TITLE : !hasNow ? 'Run once before continuing' : continueProblem ?? 'Extend the existing trajectory using the time below (legacy C still asks for an end time)'}>Continue</button>
      <button class="danger" data-run="stop" disabled={!busy || stopping} onClick={() => session.abort()}
        title="Stop the running command or cancel its prompt (Esc)">{stopping ? 'Stopping…' : 'Stop'}</button>
    </div>
    <div class="run-time-controls">
      {total && <label>Run duration <Field data-run-duration="" spec={{kind: 'number', positive: true}}
        value={sentDuration ?? String(total.value ?? '')} onCommit={text => session.setNumeric('total', text)}
        error={durationError} settling={sentDuration !== null} title="Total duration of Run from initial/current, in model time units" /></label>}
      <label>Continue <select aria-label="Continuation time mode" value={continueMode} onChange={e => changeMode(e.currentTarget.value as 'extra' | 'until')}>
        <option value="extra">For another</option><option value="until">Until time</option>
      </select><Field aria-label="Continuation time" data-continue-time="" spec={NUMBER} value={continueValue ?? ''}
        onInput={setContinueValue} error={hasNow ? continueProblem : null} /></label>
    </div>
    {steady && hello && <details class="steady-settings">
      <summary>Steady: {steady.decimals} decimal places · hold {steady.hold} · limit {steady.maximum}</summary>
      <div class="steady-fields">
        <label>Decimal places <Field data-steady="decimals" spec={{kind: 'integer', min: 0, max: hello.steady.max_decimals}} value={steady.decimals}
          onInput={text => setSteady({...steady, decimals: text})} /></label>
        <label>Hold duration <Field data-steady="hold" spec={{kind: 'number', positive: true}} value={steady.hold}
          onInput={text => setSteady({...steady, hold: text})} /></label>
        <label>Maximum duration <Field data-steady="maximum" spec={{kind: 'number', positive: true}} value={steady.maximum}
          onInput={text => setSteady({...steady, maximum: text})} /></label>
      </div>
      {steadyProblem && <p class="field-error" role="alert">{steadyProblem}</p>}
      <p>Starts from Initial. Compares every Dt interval ({inspectNumber(interval)}) using core values; stores every interval for this run. Adaptive solvers may take internal substeps. Matching digits depend on Dt and do not prove equilibrium stability.</p>
    </details>}
    <div class="run-context">
      <strong role="status">{stopping ? 'Stopping…' : computing ? 'Running' : asking ? 'Awaiting input' : busy ? 'Working…' : 'Idle'}</strong>
      {storedTime !== undefined && Number.isFinite(storedTime) && <span title="Time of the last stored point in the active plot; sampled trajectory precision">Stored t = {inspectNumber(storedTime)}</span>}
      <span>Parameter edits apply to the next run.</span>
    </div>
    {!busy && resultText && <p class="steady-result" role="status">{resultText} t = {inspectNumber(result!.time)}</p>}
  </section>;
}
