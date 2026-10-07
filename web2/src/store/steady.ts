import type {HelloEvent, NumericsField} from '../protocol/types';

/** the `key`s of the `numerics` event's fields the run controls read (docs/protocol.md "numerics") */
export const NUM_TOTAL = 'total', NUM_DT = 'dt', NUM_METHOD = 'method';
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

/** the continue field before the user types: the run duration more, or (until) the core time plus it */
export function continueDefault(mode: 'extra' | 'until', total: number, time: number | undefined): string {
  return String(mode === 'extra' ? total : (time ?? 0) + total);
}
