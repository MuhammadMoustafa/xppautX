import type {HelloEvent, NumericsField, StateEvent} from '../protocol/types';

/** the `key`s of the `numerics` event's fields the run controls read (docs/protocol.md "numerics") */
export const NUM_TOTAL = 'total', NUM_DT = 'dt', NUM_METHOD = 'method', NUM_STORE_EVERY = 'store_every';
/** the solver the core names for difference equations (core/solver.cpp's method table, the one entry
    listed in the method field's `choices`); a map advances one step per iteration, so its interval is
    one whatever Dt holds */
export const DISCRETE_METHOD = 'Discrete';
export const DISCRETE_INTERVAL = 1;

/** the numerics field `key` names, undefined before the `numerics` event */
export function numericsField(numerics: NumericsField[] | null, key: string): NumericsField | undefined {
  return numerics?.find(f => f.key === key);
}

/** the time between stored points of a run: one for a map, else Dt (NaN before the numerics arrive) */
export function runInterval(numerics: NumericsField[] | null): number {
  const method = numericsField(numerics, NUM_METHOD);
  return method?.choices?.[Number(method.value)] === DISCRETE_METHOD ? DISCRETE_INTERVAL : Number(numericsField(numerics, NUM_DT)?.value);
}

export interface SteadyInput {decimals: string; hold: string; maximum: string}

/** Cross-field checks for the inline settings; the core independently
    validates the command before changing any run state. */
export function steadyError(input: SteadyInput, interval: number, maxDecimals: number): string | null {
  const decimals = Number(input.decimals), hold = Number(input.hold), maximum = Number(input.maximum);
  if (!input.decimals.trim() || !Number.isInteger(decimals) || decimals < 0 || decimals > maxDecimals)
    return `Choose 0–${maxDecimals} decimal places.`;
  if (!Number.isFinite(interval) || interval <= 0) return 'Choose a positive Dt.';
  if (!input.hold.trim() || !Number.isFinite(hold) || hold < interval) return 'Hold duration must be at least Dt.';
  if (!input.maximum.trim() || !Number.isFinite(maximum) || maximum < hold) return 'Maximum duration must cover the hold duration.';
  return null;
}

/** the inline settings before the user edits any, from hello's steady limits and the run's own
    duration and interval, so a changed Run duration or Dt never leaves an old limit behind */
export function steadyDefaults(steady: HelloEvent['steady'], total: number, interval: number): SteadyInput {
  return {decimals: String(steady.default_decimals),
    hold: String(Math.min(total, Math.max(interval, steady.default_hold))), maximum: String(total)};
}

/** the time between the rows a continuation stores: Store every N steps of the run interval (the stride the core keeps) */
export function outputInterval(numerics: NumericsField[] | null): number {
  const every = numericsField(numerics, NUM_STORE_EVERY)?.value;
  return runInterval(numerics) * (typeof every === 'number' && every >= 1 ? every : 1);
}

/** where Continue until ends: the core rounds the end asked for up to the next output interval after the
    core time, a value within the hello's tolerance of an interval counting as on it, and at least
    one interval (core/integrate.cpp continue_to); undefined while the input cannot run */
export function continueEnd(until: number, time: number, interval: number, tolerance: number): number | undefined {
  const duration = until - time;
  if (!Number.isFinite(duration) || duration <= 0 || !Number.isFinite(interval) || interval <= 0) return undefined;
  return time + Math.max(Math.ceil(duration / interval - tolerance), 1) * interval;
}

/** the continue field before the user types: the run duration more, or (until) the core time plus it */
export function continueDefault(mode: 'extra' | 'until', total: number, time: number | undefined): string {
  return String(mode === 'extra' ? total : (time ?? 0) + total);
}

/** what the user chose for Continue: how the time is read, and what was typed (null: not edited yet) */
export interface ContinueInput {mode: 'extra' | 'until'; text: string | null}
export const CONTINUE_INPUT_DEFAULT: ContinueInput = {mode: 'extra', text: null};

/** Continue as the page shows and sends it, one for the toolbar button and the command's key (W213) */
export interface ContinuePlan {
  /** the field's text: what was typed, else the default from Run duration and the core time (null before the numerics arrive) */
  text: string | null;
  /** the command to send; null while the input cannot run (problem says why, null before a first run) */
  command: {cmd: 'continue'; extra?: number; until?: number} | null;
  problem: string | null;
  /** the time the run will end at (until mode): the end rounded up to the output grid */
  end: number | undefined;
}

export function continuePlan(input: ContinueInput, numerics: NumericsField[] | null, core: StateEvent | null, hello: HelloEvent | null): ContinuePlan {
  const total = numericsField(numerics, NUM_TOTAL)?.value;
  const text = input.text ?? (typeof total === 'number' ? continueDefault(input.mode, total, core?.time) : null);
  const interval = runInterval(numerics), time = Number(core?.time);
  const value = Number(text);
  const duration = input.mode === 'extra' ? value : value - time;
  const problem = !core?.now || text === null ? null
    : !text.trim() || !Number.isFinite(duration) || duration < interval ? 'Choose at least one Dt of additional time.' : null;
  const end = input.mode === 'until' && hello && core?.now && text !== null && !problem
    ? continueEnd(value, time, outputInterval(numerics), hello.continue.grid_tolerance) : undefined;
  return {text, problem, end, command: core?.now && text !== null && !problem ? {cmd: 'continue', [input.mode]: value} : null};
}
