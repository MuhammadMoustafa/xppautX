/* The autoX window's script panel (W684, docs/protocol.md "autoX scripts"): the open .autox script's runs in order, each
   with what it starts from, a Done mark once it has run (the error of a run that was refused, or why it was not made, in place of it), and its own Run; Run all runs them in order. Its list is the
   `autox.script_runs` event (store/autoxScript.ts). A run whose start has not run waits, its Run off and the reason in
   its tip. No script open, no panel. */
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {useCommandLabel} from './CommandButton';
import {ShownName} from './CutText';
import {startText, unfinishedStart} from '../store/autoxScript';
import {choiceLabel} from '../store/fieldKinds';

export function AutoxScript() {
  const session = useSession();
  const may = useMay();
  const script = useStore(s => s.autoxScript);
  const runAll = useCommandLabel('autox.run_script');
  if (!script.open) return null;
  const off = !may({cmd: 'autox.run_script'});
  const done = script.runs.filter(r => r.done).length;
  const file = script.file.split(/[\\/]/).pop() ?? script.file;
  return (
    <details class="autox-script" open>
      <summary>Script {file} <span class="muted">{done} of {script.runs.length} runs done</span></summary>
      <ol class="autox-script-runs" aria-label="Runs of the script">
        {script.runs.map((run, i) => {
          const waiting = unfinishedStart(script.runs, run);
          const runOff = off || waiting !== null;
          const state = run.done ? 'done' : run.error ? 'error' : run.skipped ? 'skipped' : '';
          return (
            <li key={`${i}-${run.name}`} class="autox-script-run" data-script-run={run.name} data-done={run.done ? 'true' : 'false'} data-state={state}>
              <span class="autox-script-number">{i + 1}</span>
              <span class="autox-script-name"><ShownName name={run.name} copy={false} /></span>
              <span class="autox-script-from muted">{startText(run.from)}</span>
              <span class="autox-script-state">{state && choiceLabel(state)}</span>
              <button class="small" disabled={runOff} aria-label={`Run ${run.name}`} data-script-run-button={run.name}
                title={waiting !== null ? `Waits for run "${waiting}" to be done` : off ? BUSY_TITLE : undefined}
                onClick={() => session.send({cmd: 'autox.run_script', run: run.name})}>Run</button>
              {/* after Run, so the message takes a line of its own and Run stays on the run's row */}
              {run.error && <span class="autox-script-error" role="alert">{run.error}</span>}
              {run.skipped && <span class="autox-script-skipped muted">{run.skipped}</span>}
            </li>
          );
        })}
      </ol>
      <div class="autox-script-tools">
        <button class="small" disabled={off} title={off ? BUSY_TITLE : undefined} data-script-run-all
          onClick={() => session.send({cmd: 'autox.run_script'})}>{runAll}</button>
      </div>
    </details>
  );
}
