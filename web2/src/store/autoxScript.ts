/* autoX's .autox script as the page holds it (docs/protocol.md "autoX scripts"): the open script's runs in order, each
   with what it starts from and whether it has run. The core sends it as the `autox.script_runs` event (core/json_autox.cpp),
   at every change of the script or its runs. Pure: no DOM, no I/O. */

/** a run's start: the `index`-th point of `type` that run `run` stored */
export interface AutoxScriptStart {run: string; type: string; index: number}

export interface AutoxScriptRun {
  name: string;
  /** the stored point it starts from; null for the current state */
  from: AutoxScriptStart | null;
  /** whether it has run (its points are still the diagram's) */
  done: boolean;
}

/** the open script; `open` false when none is (then the other fields are empty) */
export interface AutoxScript {open: boolean; file: string; model: string; runs: AutoxScriptRun[]}

export const initialAutoxScript: AutoxScript = {open: false, file: '', model: '', runs: []};

/** what a run starts from, in words */
export function startText(from: AutoxScriptStart | null): string {
  return from ? `starts at ${from.type} ${from.index} of "${from.run}"` : 'starts from the current state';
}

/** the run `run` starts from, when that run has not run yet (the core refuses it then): its name, else null */
export function unfinishedStart(runs: AutoxScriptRun[], run: AutoxScriptRun): string | null {
  if (!run.from) return null;
  const source = runs.find(r => r.name === run.from?.run);
  return source && !source.done ? source.name : null;
}
