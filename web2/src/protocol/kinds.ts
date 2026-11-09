/* What an action is, and so whether it may start now (W95, docs/protocol.md
   "Action kinds"). The core defines every command's kind once, in the registry
   (core/command_def.h), and sends it in `hello.commands`. Only a running computation (the
   core's `computing` event, until the command's done) or an open question
   disables anything; the page's own catch-up commands never do. */
import type {Command, HelloEvent, StateEvent} from './types';

export interface RegistryGroup {rule: string; arguments: string[]}

export interface RegistryArgument {
  name: string; type: string; required: boolean; choices: string[];
  names?: string; element?: string; max_items?: number; max_length?: number;
  min?: number; max?: number; multiline?: boolean;
  /** the key of the option row (in the command's panel group) that sets it: its panel shows it, the form leaves it out */
  option?: string;
  /** what the argument is, under its field in a form (the row's argument description) */
  description?: string;
}

/** control (Abort, Quit, an answer): always; view (only changes what is
    shown) and setting (a parameter, an initial or boundary condition, a
    delay, the numerics, autoX's forms: W106, applied when a computation
    ends, to the next one): also while a computation runs; data (saves,
    loads) and computation (starts one): only when none runs */
export type Kind = 'control' | 'view' | 'setting' | 'data' | 'computation';

const OF_LETTER: Record<string, Kind> = {c: 'control', v: 'view', s: 'setting', d: 'data', x: 'computation'};

/** a window with a toolbar of its own (navigation.ts toolbarCommand) */
export type LayerWindow = 'autox' | 'data' | 'ani' | 'aplot';

/** a command with its kind (hello.commands) and whether it is
    a step of the user's (what a recording records, files.ts's run record); op absent: the
    command's other lines */
export interface CommandKind {
  arguments?: RegistryArgument[];
  groups?: RegistryGroup[];
  file_mode?: string;
  file_wild?: string;
  label?: string;
  menu_path?: string;
  /** the option group whose generated panel opens in place of this command's own form (core Command::panel) */
  panel?: string;
  action?: string;
  description?: string;
  category?: string;
  default_keys?: string[];
  cmd: string;
  op?: string;
  kind: string;
  step: boolean;
}

/** a registry command as every list the page shows it in (navigation.ts registryRows) */
export interface CommandRow {
  arguments?: RegistryArgument[];
  groups?: RegistryGroup[];
  file_mode?: string;
  file_wild?: string;
  /** the groups above the label in the menu bar (`File/Sets`), none for a row with no menu place */
  menu_path?: string;
  /** the option group whose panel opens in place of a form */
  panel?: string;
  /** the action's text in a form's footer, named for what it does (the row's own, never Run or OK) */
  action?: string;
  /** stable: the command's name */
  id: string;
  label: string;
  description: string;
  /** one kind letter (OF_LETTER) */
  kind: string;
  /** a CommandCategory id */
  category: string;
  pinnable: boolean;
  /** keys that run it from the page ("Ctrl+O": Ctrl, or Cmd on macOS) */
  default_keys: string[];
}

/** the table's entry for `cmd` (the first with its op, else the one with none) */
function commandOf(hello: HelloEvent, cmd: Command): CommandKind | undefined {
  return hello.commands.find(c => c.cmd === cmd.cmd && (c.op === undefined || c.op === cmd.op));
}

/** whether `cmd` is a step of the user's (hello.commands) */
export function isStep(hello: HelloEvent, cmd: Command): boolean {
  return commandOf(hello, cmd)?.step ?? false;
}

/** whether `id` is undo or redo with no value edit to take back or put back (the state's can_undo, can_redo) */
export function noValueEdit(core: StateEvent | null, id: string): boolean {
  return !!core && (id === 'value.undo' && !core.can_undo || id === 'value.redo' && !core.can_redo);
}

/** the row's label of a computation (what the status line says is running), null for any other command */
export function computationLabel(hello: HelloEvent | null, cmd: Command): string | null {
  const entry = hello && commandOf(hello, cmd);
  return entry && OF_LETTER[entry.kind] === 'computation' ? entry.label ?? null : null;
}

/** The page resolves physical keys through the keymap before sending registry commands. */
export function kindOf(hello: HelloEvent | null, cmd: Command): Kind | null {
  if (!hello) return cmd.cmd === 'answer' || cmd.cmd === 'run.stop' || cmd.cmd === 'session.quit' ? 'control' : null;
  const entry = commandOf(hello, cmd);
  return entry ? OF_LETTER[entry.kind] ?? null : null;
}

/** whether an action of `kind` may start: control and a setting always (a setting sent
    while a question is open or a computation runs applies when the core can: at once
    before the command computes, else when it ends); while a question is open nothing
    else (only answering or cancelling it applies); while a computation runs a view
    only. An unknown kind counts as a computation. */
export function mayStart(kind: Kind | null, computing: boolean, asking: boolean): boolean {
  if (kind === 'control' || kind === 'setting') return true;
  if (asking) return false;
  return kind === 'view' || !computing;
}
