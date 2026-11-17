/* A pin's steps (W775): what each step is run as and what its check says, always shown under it: runs, writes a file, the file exists
   (the replace question will come), cannot run now. A step is a command value in W772's written form; the core reads it
   (keymap.check_step, hello.keymap.values), and this module only works out from that command what it writes, by the core's output names
   (hello.output_names through the export items): the file named in the step, else the item's own. Pure. */
import type {HelloEvent, KeymapInfo, Pin} from './protocol/types';
import type {ExportItem} from './exportSeveral';
import type {BatchStep} from './batch';
import {unavailableReason} from './ui/navigation';

/** the command a step is as the core read it (`run` of keymap.check_step and hello.keymap.values) */
export type StepRun = {cmd: string} & Record<string, string | boolean>;

/** a step worked out: the command the runner sends, the file it writes (a base name) or null, and what stops it from running at all */
export interface StepPlan {
  command: {cmd: string} & Record<string, unknown>;
  file: string | null;
  problem: string | null;
}

/** whether export item `item` is what `args` (a step's arguments without its file) say */
function sameItem(item: ExportItem, args: Record<string, string | boolean>): boolean {
  const keys = Object.keys(args);
  return keys.length === Object.keys(item.args).length && keys.every(key => String(args[key]) === item.args[key]);
}

/** step `text` as it runs: `run` is the core's reading of it (null when no reading is known); `items` every export item the session can
    write. No step asks: a step that would ask for its file takes the item's own name, or is a problem when there is none */
export function planStep(text: string, run: StepRun | null, items: readonly ExportItem[]): StepPlan {
  if (!run) return {command: {cmd: text}, file: null, problem: 'the core has not read this step'};
  const {cmd, file, file_picker: picker, ...args} = run;
  if (typeof file === 'string') return {command: {cmd, ...args}, file, problem: null};
  const item = items.find(i => i.cmd === cmd && sameItem(i, args));
  if (item) return {command: {cmd, ...args}, file: item.file, problem: null};
  if (picker) return {command: {cmd, ...args}, file: null, problem: `it asks for its file: write --file "name"`};
  return {command: {cmd, ...args}, file: null, problem: null};
}

/** the core's reading of a saved step: its entry in hello.keymap.values (a command with arguments), or the bare command; null for neither */
export function stepRun(info: KeymapInfo | null, hello: HelloEvent | null, step: string): StepRun | null {
  return info?.values.find(v => v.value === step)?.run ?? (hello?.commands?.some(c => c.cmd === step) ? {cmd: step} : null);
}

/** a step's check line: its kind (for the colour and the mark) and its words */
export interface StepStatus {
  kind: 'ok' | 'warn' | 'no';
  text: string;
}

export function stepStatus(plan: StepPlan, unavailable: {cmd: string; reason: string}[], exists: boolean): StepStatus {
  if (plan.problem) return {kind: 'no', text: `✕ ${plan.problem}`};
  const why = unavailableReason(unavailable, plan.command.cmd);
  if (why) return {kind: 'no', text: `✕ cannot run now: ${why}`};
  if (plan.file && exists) return {kind: 'warn', text: `! writes ${plan.file}, which exists: Replace / Skip those / Cancel will be asked`};
  return {kind: 'ok', text: plan.file ? `✓ writes ${plan.file}` : '✓ runs'};
}

/** the batch of pin `pin` for the runner: one step per command, each with the file it writes; problems are the first step's that has one */
export function pinSteps(pin: Pin, runOf: (step: string) => StepRun | null, items: readonly ExportItem[]): {steps: BatchStep[]; problem: string | null} {
  let problem: string | null = null;
  const steps = pin.steps.map((text, i) => {
    const plan = planStep(text, runOf(text), items);
    if (plan.problem && !problem) problem = `step ${i + 1} of ${pin.steps.length}, ${text}: ${plan.problem}`;
    return {label: text, command: plan.command, file: plan.file};
  });
  return {steps, problem};
}
