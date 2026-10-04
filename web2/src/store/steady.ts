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
