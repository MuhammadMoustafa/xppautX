/* Direct run actions and inline settings; no mandatory intermediate dialog. */
import {useEffect} from 'preact/hooks';
import {BUSY_TITLE, useMay, useMayMain, useSession, useStore} from './context';
import {fieldKey, inspectNumber, sentText} from '../store/values';
import {continuePlan, NUM_TOTAL, numericsField, runInterval, steadyRun} from '../store/steady';
import {NUMBER} from '../store/fieldKinds';
import {Field} from './Field';
import {QuickAccess} from './QuickAccess';

export function RunToolbar() {
  const session = useSession();
  const mayMain = useMayMain(), may = useMay();
  const hello = useStore(s => s.hello);
  const core = useStore(s => s.core);
  const numerics = useStore(s => s.numerics);
  const steadyEdit = useStore(s => s.steadyEdit);
  const hasNow = !!core?.now;
  const busy = useStore(s => s.busy);
  const computing = useStore(s => s.computing);
  const stopping = useStore(s => s.stopping);
  const asking = useStore(s => !!s.ask);
  const total = useStore(s => numericsField(s.numerics, NUM_TOTAL));
  const interval = useStore(s => runInterval(s.numerics));
  const durationField = fieldKey('num', NUM_TOTAL);
  const sentDuration = useStore(s => sentText(s.values.inflight, durationField));
  const durationError = useStore(s => s.values.errors[durationField] ?? null);
  const storedTime = useStore(s => {
    const series = s.plots.windows.find(w => w.win === s.plots.active)?.series;
    return series?.rows ? series.columns.get(0)?.[series.rows - 1] : undefined;
  });
  /* what the user typed; anything else follows Run duration, Dt and the core time (steadyDefaults,
     continueDefault), and a new Run duration drops the typed values so no old limit survives it */
  const continueInput = useStore(s => s.continueInput);
  const setContinue = (input: typeof continueInput) => session.store.dispatch({type: 'continueInput', input});
  const editSteady = (edit: typeof steadyEdit) => session.store.dispatch({type: 'steadyEdit', edit});
  useEffect(() => {
    editSteady({});
    session.store.dispatch({type: 'continueInput', input: {...session.store.getState().continueInput, text: null}});
  }, [total?.value]);
  const run = steadyRun(hello, numerics, steadyEdit);
  const steady = run?.input ?? null;
  const continueKeys = hello?.command_table.find(row => row.id === 'continue')?.default_keys.join(' / ') ?? '';
  const plan = continuePlan(continueInput, numerics, core, hello);
  const initialOff = !mayMain('initialconds');
  const currentOff = initialOff || !hasNow;
  const steadyProblem = run?.problem ?? null;
  const steadyOff = !may({cmd: 'steady'}) || !steady || !!steadyProblem;
  const continueOff = !may({cmd: 'continue'}) || !plan.command;
  const changeMode = (mode: 'extra' | 'until') => {
    if (mode === continueInput.mode) return;
    const time = Number(core?.time), value = Number(plan.text);
    if (Number.isFinite(time) && continueInput.text !== null)
      setContinue({mode, text: String(mode === 'until' ? time + value : value - time)});
    else setContinue({...continueInput, mode});
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
        onClick={() => { if (!initialOff) session.menuAction('main', 'run_initial'); }}
        title={initialOff ? BUSY_TITLE : 'Start a new trajectory from Initial values (I, G)'}>Run from initial</button>
      <button data-run="current" aria-disabled={currentOff}
        onClick={() => { if (!currentOff) session.menuAction('main', 'run_last'); }}
        title={initialOff ? BUSY_TITLE : !hasNow ? 'Run once to obtain a last state' : 'Use the last state as Initial and start a new trajectory (I, L)'}>Run from last state</button>
      <button data-run="steady" aria-disabled={steadyOff}
        onClick={() => { if (!steadyOff) session.runSteady(); }}
        title={initialOff ? BUSY_TITLE : steadyProblem ?? 'Run from Initial until every state stops changing at the configured precision'}>Run to steady state</button>
      <button data-run="continue" aria-disabled={continueOff}
        onClick={() => { if (!continueOff) session.continueRun(); }}
        title={!may({cmd: 'continue'}) ? BUSY_TITLE : !hasNow ? 'Run once before continuing' : plan.problem ?? `Extend the existing trajectory using the time below (${continueKeys})`}>Continue</button>
      <button class="danger" data-run="stop" disabled={!busy || stopping} onClick={() => session.abort()}
        title="Stop the running command or cancel its prompt (Esc)">{stopping ? 'Stopping…' : 'Stop'}</button>
    </div>
    <QuickAccess />
    <div class="run-time-controls">
      {total && <label>Run duration <Field data-run-duration="" spec={{kind: 'number', positive: true}}
        value={sentDuration ?? String(total.value ?? '')} onCommit={text => session.setNumeric('total', text)}
        error={durationError} settling={sentDuration !== null} title="Total duration of Run from initial/last state, in model time units" /></label>}
      <label>Continue <select aria-label="Continuation time mode" value={continueInput.mode} onChange={e => changeMode(e.currentTarget.value as 'extra' | 'until')}>
        <option value="extra">For another</option><option value="until">Until time</option>
      </select><Field aria-label="Continuation time" data-continue-time="" spec={NUMBER} value={plan.text ?? ''}
        onInput={text => setContinue({...continueInput, text})} error={plan.problem} />
        {plan.end !== undefined && <span class="continue-end" data-continue-end="">will end at t={inspectNumber(plan.end)}</span>}</label>
    </div>
    {steady && hello && <details class="steady-settings">
      <summary>Steady: {steady.decimals} decimal places · hold {steady.hold} · limit {steady.maximum}</summary>
      <div class="steady-fields">
        <label>Decimal places <Field data-steady="decimals" spec={{kind: 'integer', min: 0, max: hello.steady.max_decimals}} value={steady.decimals}
          onInput={text => editSteady({...steadyEdit, decimals: text})} /></label>
        <label>Hold duration <Field data-steady="hold" spec={{kind: 'number', positive: true}} value={steady.hold}
          onInput={text => editSteady({...steadyEdit, hold: text})} /></label>
        <label>Maximum duration <Field data-steady="maximum" spec={{kind: 'number', positive: true}} value={steady.maximum}
          onInput={text => editSteady({...steadyEdit, maximum: text})} /></label>
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
