/* The user's keymap (W211, docs/protocol.md "Keymap"): what the core says the keys are, from hello and
   from each `keymap` reply, kept as it came, and the keymap editor's state (W212). The core owns the
   file, the grammar and the checks (core/xpp_keymap.h); this slice holds the effective keymap for the
   dispatcher (ui/hotkeys.ts), the editor and the toolbar, and turns each edit the user makes into the
   differences a `set` sends: all of them pure functions of the keymap the core last sent, so the page
   keeps no second copy of the user's settings. Pure: no DOM, no I/O. */
import type {CommandRow} from '../protocol/kinds';
import type {KeymapInfo, Pin, UserKeymap} from '../protocol/types';

/** a key being recorded: the key number `index` of command value `id` (a row's `binding`; `index` = its key count: a new second binding) */
export interface Recording {
  id: string;
  index: number;
}

/** a recorded key another command has (docs/command-design.md: "Ctrl+B is Run from last state. Replace / Cancel") */
export interface Conflict {
  id: string;
  index: number;
  key: string;
  /** the command value (row binding) that has it */
  other: string;
}

/** the keymap pane's state (the Settings window's Keymap group), in the store so a check can read it (window.__xpp.state()); `open` and `close` clear it */
export interface KeymapEditor {
  query: string;
  recording: Recording | null;
  conflict: Conflict | null;
  /** what the last recorded key was refused for (a reserved key, too many keys), announced to a screen reader */
  refusal: string;
  /** Reset all waits for its confirmation */
  confirmingReset: boolean;
}

export interface KeymapState {
  /** the effective keymap, null before hello */
  info: KeymapInfo | null;
  editor: KeymapEditor;
}

const closedEditor: KeymapEditor = {query: '', recording: null, conflict: null, refusal: '', confirmingReset: false};
export const initialKeymap: KeymapState = {info: null, editor: closedEditor};

export type KeymapAction =
  | {type: 'info'; info: KeymapInfo}
  | {type: 'open'}
  | {type: 'close'}
  | {type: 'query'; query: string}
  | {type: 'record'; recording: Recording | null}
  | {type: 'conflict'; conflict: Conflict | null}
  | {type: 'refuse'; text: string}
  | {type: 'confirmReset'; on: boolean};

export function reduceKeymap(state: KeymapState, action: KeymapAction): KeymapState {
  const {editor} = state;
  switch (action.type) {
    case 'info':
      /* an answer from the core ends what was waiting on it: the key recorded, the conflict, the reset asked */
      return action.info === state.info ? state
        : {info: action.info, editor: {...editor, recording: null, conflict: null, refusal: '', confirmingReset: false}};
    case 'open':
    case 'close':
      return {...state, editor: closedEditor};
    case 'query':
      return {...state, editor: {...editor, query: action.query}};
    case 'record':
      return {...state, editor: {...editor, recording: action.recording, conflict: null, refusal: '', confirmingReset: false}};
    case 'conflict':
      return {...state, editor: {...editor, conflict: action.conflict, recording: null, refusal: ''}};
    case 'refuse':
      return {...state, editor: {...editor, refusal: action.text, conflict: null}};
    case 'confirmReset':
      return {...state, editor: {...editor, confirmingReset: action.on}};
  }
}

/** a keymap's effective keys by command id (built once per keymap sent) */
const keysById = new WeakMap<KeymapInfo, Map<string, KeymapInfo['commands'][number]>>();
function commandsById(info: KeymapInfo): Map<string, KeymapInfo['commands'][number]> {
  let map = keysById.get(info);
  if (!map) keysById.set(info, map = new Map(info.commands.map(c => [c.id, c])));
  return map;
}

/** the effective keys of `row`, a command or one of its items, by its binding (its table keys before hello, or for a row the keymap lacks) */
export function effectiveKeys(info: KeymapInfo | null, row: Pick<CommandRow, 'binding' | 'default_keys'>): string[] {
  return (info && commandsById(info).get(row.binding)?.keys) ?? row.default_keys;
}

/** whether command `id`'s keys are the user's */
export function isChanged(info: KeymapInfo | null, id: string): boolean {
  return !!info && commandsById(info).get(id)?.source === 'user';
}

/** the user's differences as a `set` sends them: the commands whose source is `user`, the pinned ones
    A keymap the core marked as an error has no user part (its defaults are shown) */
export function userKeymap(info: KeymapInfo): UserKeymap {
  const bindings: Record<string, string[]> = {};
  for (const c of info.commands) if (c.source === 'user') bindings[c.id] = c.keys;
  return {pinned: info.pinned, bindings};
}

/** the keymap with command `id`'s keys replaced (the other commands' bindings and the pins as they are) */
export function withKeys(info: KeymapInfo, id: string, keys: string[]): UserKeymap {
  const user = userKeymap(info);
  return {...user, bindings: {...user.bindings, [id]: keys}};
}

/** the keymap with command `id`'s key number `index` set to `key` (`index` = the key count adds one) */
export function withKey(info: KeymapInfo, row: CommandRow, index: number, key: string): UserKeymap {
  const keys = effectiveKeys(info, row).slice();
  keys[index] = key;
  return withKeys(info, row.binding, keys);
}

/** the keymap with command `id`'s key number `index` removed */
export function withoutKey(info: KeymapInfo, row: CommandRow, index: number): UserKeymap {
  return withKeys(info, row.binding, effectiveKeys(info, row).filter((_, i) => i !== index));
}

/** the keymap with command `id` back on the table's keys */
export function withoutBinding(info: KeymapInfo, id: string): UserKeymap {
  const user = userKeymap(info);
  return {...user, bindings: Object.fromEntries(Object.entries(user.bindings).filter(([bound]) => bound !== id))};
}

/** the keymap with `pinned` the quick-access toolbar's pins, in order */
export function withPinned(info: KeymapInfo, pinned: Pin[]): UserKeymap {
  const user = userKeymap(info);
  /* a pin's keys go with it: the core refuses a binding of a pin that is gone (or no longer of several steps) */
  const kept = new Set(pinned.filter(pin => pin.steps.length > 1).map(pin => pinBindingId(pin.name)));
  const bindings = Object.fromEntries(Object.entries(user.bindings).filter(([id, _]) => !id.startsWith('pin ') || kept.has(id)));
  return {...user, pinned, bindings};
}

/** the keymap with pin `from` replaced by `pin` (a rename keeps its keys: the binding moves to the new name) */
export function withRenamedPin(info: KeymapInfo, from: string, pin: Pin): UserKeymap {
  const user = withPinned(info, replacePin(info.pinned, from, pin));
  const old = userKeymap(info).bindings[pinBindingId(from)];
  return old && pin.steps.length > 1 ? {...user, bindings: {...user.bindings, [pinBindingId(pin.name)]: old}} : user;
}

/** the command other than `id` that has `key`, if any */
export function conflictOf(info: KeymapInfo, rows: readonly CommandRow[], id: string, key: string): CommandRow | undefined {
  return rows.find(row => row.binding !== id && effectiveKeys(info, row).includes(key));
}

/** the keymap with `key` given to command `row` as its key number `index`, and taken from every other
    command that has it (what Replace sends: the conflicting command loses the key, `row` gets it) */
export function replaceKey(info: KeymapInfo, rows: readonly CommandRow[], row: CommandRow, index: number, key: string): UserKeymap {
  const user = withKey(info, row, index, key);
  const bindings = {...user.bindings};
  for (const other of rows) {
    const keys = effectiveKeys(info, other);
    if (other.binding !== row.binding && keys.includes(key)) bindings[other.binding] = keys.filter(k => k !== key);
  }
  return {...user, bindings};
}

/** where a command's keys come from, as the editor says it: the table's (default), the user's own for a
    command that has none by default (user), or the user's instead of the table's (changed) */
export function sourceLabel(info: KeymapInfo | null, row: Pick<CommandRow, 'binding' | 'default_keys'>): 'default' | 'changed' | 'user' {
  return !isChanged(info, row.binding) ? 'default' : row.default_keys.length ? 'changed' : 'user';
}

/** what a recorded key is refused for, or null: a key the system keeps, or one more than a command may have */
export function keyProblem(info: KeymapInfo, row: CommandRow, index: number, key: string): string | null {
  if (info.reserved.includes(key))
    return `${key} is kept by the system or the browser; choose another key.`;
  if (index >= effectiveKeys(info, row).length && effectiveKeys(info, row).length >= info.limits.keys)
    return `${row.name} has the most keys it may (${info.limits.keys}); remove one first.`;
  return null;
}

/** What a recorded key does in the editor: the key, then "set it", or the reason not to. The key
    pressed is `name` (hotkeys.ts keyName); Esc cancels the recording and Backspace clears the key. */
export type Recorded =
  | {kind: 'cancel'}
  | {kind: 'clear'}
  | {kind: 'refused'; text: string}
  | {kind: 'conflict'; conflict: Conflict}
  | {kind: 'set'; key: string};

export function recordKey(info: KeymapInfo, rows: readonly CommandRow[], rec: Recording, name: string): Recorded {
  if (name === 'Escape') return {kind: 'cancel'};
  if (name === 'Backspace') return {kind: 'clear'};
  const row = rows.find(r => r.binding === rec.id);
  if (!row) return {kind: 'cancel'};
  const problem = keyProblem(info, row, rec.index, name);
  if (problem) return {kind: 'refused', text: problem};
  const other = conflictOf(info, rows, rec.id, name);
  if (other) return {kind: 'conflict', conflict: {...rec, key: name, other: other.binding}};
  return {kind: 'set', key: name};
}

/** The rows the editor lists for `query`: every word matches the label, description, category, id or
    one of the keys (typed as "Ctrl+Enter" or "ctrl enter": the words are matched one by one), in the table's order. */
export function editorRows(info: KeymapInfo | null, rows: readonly CommandRow[], query: string): CommandRow[] {
  const words = query.toLocaleLowerCase().split(/[\s+]+/).filter(Boolean);
  return rows.filter(row => words.every(word =>
    `${row.name} ${row.description} ${row.category} ${row.binding} ${effectiveKeys(info, row).join(' ')}`.toLocaleLowerCase().replace(/\+/g, ' ').includes(word)));
}

/** the keymap's binding id of pin `name` (core keymap::pin_binding_id): `pin "Name"`, the name as JSON text */
export const pinBindingId = (name: string): string => `pin ${JSON.stringify(name)}`;

/** the pins of several steps as rows beside the commands, bound and conflict-checked as they are (a pin of one step is its command's row):
    named `Pin "Name"`, running the pin, with no keys of their own until the user gives one */
export function pinRows(info: KeymapInfo | null): CommandRow[] {
  return (info?.pinned ?? []).filter(pin => pin.steps.length > 1).map(pin => ({
    id: pinBindingId(pin.name), binding: pinBindingId(pin.name), pin: pin.name, label: pin.name, name: `Pin "${pin.name}"`,
    description: `Runs ${pin.steps.length} steps: ${pin.steps.join(', ')}`, kind: 'd', category: 'pins', pinnable: false, default_keys: [],
  }));
}

/** the pin of one step that is exactly command value `binding` (what the Pin button of a command makes), if any */
export function pinOf(pinned: readonly Pin[], binding: string): Pin | undefined {
  return pinned.find(pin => pin.steps.length === 1 && pin.steps[0] === binding);
}

/** `base` as a pin's name that no pin of `pinned` has: `base`, then `base (2)`, `base (3)`... */
export function freshName(pinned: readonly Pin[], base: string): string {
  const taken = new Set(pinned.map(pin => pin.name));
  let name = base;
  for (let n = 2; taken.has(name); n++) name = `${base} (${n})`;
  return name;
}

/** `pinned` with command `row` pinned (a pin of its one step, named as the command is) or, when it is, unpinned */
export function togglePinned(pinned: readonly Pin[], row: Pick<CommandRow, 'binding' | 'name'>): Pin[] {
  const pin = pinOf(pinned, row.binding);
  return pin ? pinned.filter(p => p !== pin) : [...pinned, {name: freshName(pinned, row.name), steps: [row.binding]}];
}

/** `pinned` with pin `name` moved to position `to` (dragging a button before or after another) */
export function movePinned(pinned: readonly Pin[], name: string, to: number): Pin[] {
  const from = pinned.findIndex(p => p.name === name);
  if (from < 0) return pinned.slice();
  const rest = pinned.filter(p => p.name !== name);
  rest.splice(Math.max(0, Math.min(to, rest.length)), 0, pinned[from]);
  return rest;
}

/** `pinned` with pin `name` replaced by `pin` (the same place), or removed when `pin` is null */
export function replacePin(pinned: readonly Pin[], name: string, pin: Pin | null): Pin[] {
  return pinned.flatMap(p => p.name !== name ? [p] : pin ? [pin] : []);
}

/** `steps` with step `from` moved to position `to` */
export function moveStep(steps: readonly string[], from: number, to: number): string[] {
  const rest = steps.filter((_, i) => i !== from);
  rest.splice(Math.max(0, Math.min(to, rest.length)), 0, steps[from]);
  return rest;
}

/** the effective keys of a pin: those of its command when it has one step (a key runs that command), its own for a sequence */
export function pinKeys(info: KeymapInfo | null, rows: readonly CommandRow[], pin: Pin): string[] {
  const row = rows.find(r => r.binding === (pin.steps.length === 1 ? pin.steps[0] : pinBindingId(pin.name)));
  return row ? effectiveKeys(info, row) : [];
}

/** How many of the pinned commands show in a row `room` wide, the others going into the overflow menu
    (never dropped). `widths` are the buttons' widths, `more` the overflow button's, `gap` between buttons.
    When all fit, no overflow button is needed; at least one stays visible only if it fits. */
export function visibleCount(widths: readonly number[], room: number, more: number, gap: number): number {
  const total = widths.reduce((sum, w) => sum + w + gap, 0);
  if (total <= room) return widths.length;
  let used = more + gap, n = 0;
  for (const w of widths) {
    if (used + w + gap > room) break;
    used += w + gap;
    n++;
  }
  return n;
}

/** whether an error event is the answer to a live check of a pin's step (keymap.check_step): the step shows it under itself, so it is
    no message, toast or dialog (a bad step is the user still typing). `source` is the command line the core refused */
export function isStepCheck(ev: {source?: string}): boolean {
  return !!ev.source && /^\s*\{\s*"cmd"\s*:\s*"keymap\.check_step"/.test(ev.source);
}
