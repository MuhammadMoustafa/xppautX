/* The words and commands of the page's batch runner (session.ts batchRun, store/batch.ts): what File > Export > Several and a pin of
   several steps both send and say. A batch is ordinary commands, one after another, each awaited to its done; the core knows nothing of
   it (docs/protocol.md: a recording of a batch holds the commands). Pure. */
import type {Command} from './protocol/types';
import type {BatchOutcome} from './store/batch';

/** a step's command as the runner sends it: `base` (the command and its arguments), the file it writes (a base name in the model's folder,
    or a full path in a folder picked), the batch's id for its done, and the replace answer when there is one (1: replace an existing file;
    left out: the file is new, or asked about as any save) */
export function stepCommand(base: {cmd: string} & Record<string, unknown>, file: string | null, id: string, replace?: 1): Command {
  return {...base, ...(file !== null && {file}), id, ...(file !== null && replace !== undefined && {replace})} as Command;
}

/** a step of a batch as the runner takes it: what the page shows of it, the command (without id and replace answer) and the file it
    writes (a base name in the model's folder, or null) */
export interface BatchStep {
  label: string;
  command: {cmd: string} & Record<string, unknown>;
  file: string | null;
}

/** `3` as "3 steps", `1` as "1 step" */
const steps = (n: number): string => (n === 1 ? '1 step' : `${n} steps`);

/** the line of Messages when step `i` (from 0) of `n` starts */
export function startLine(title: string, i: number, n: number, step: string): string {
  return `${title}: step ${i + 1} of ${n}, ${step}`;
}

/** the line of Messages when it ends: what it did, or the core's reason */
export function endLine(title: string, i: number, n: number, outcome: BatchOutcome): string {
  return `${title}: step ${i + 1} of ${n} ${outcome.status}${outcome.status === 'failed' && outcome.text ? `: ${outcome.text}` : ''}`;
}

/** the one message of a batch that ended before its last step (a failure or Stop): the batch, the step (n of N and its command), the
    reason (Stop pressed between two steps has no failed outcome), the steps done and those not run; null when every step was run */
export function stoppedMessage(title: string, all: string[], outcomes: BatchOutcome[]): string | null {
  const ended = outcomes.findIndex(o => o.status === 'failed');
  const at = ended >= 0 ? ended : outcomes.length;
  if (at >= all.length) return null;
  const list = (from: number, to: number) => all.slice(from, to).map((_, k) => from + k + 1).join(', ');
  const done = outcomes.slice(0, at).filter(o => o.status !== 'skipped').length;
  const why = ended >= 0 ? outcomes[ended].text || 'it failed' : 'Stop was pressed';
  return `${title} stopped at step ${at + 1} of ${all.length}, ${all[at]}: ${why}. Done: ${done ? `${steps(done)} (${list(0, at)})` : 'none'}. Not run: ${at + 1 < all.length ? list(at + 1, all.length) : 'none'}.`;
}

/** the message of a batch that ran every step */
export function doneMessage(title: string, n: number): string {
  return `${title}: ${steps(n)} done.`;
}
