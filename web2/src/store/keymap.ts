/* The user's keymap (W211, docs/protocol.md "Keymap"): what the core says the keys are, from hello and
   from each `keymap` reply, kept as it came, and the keymap editor's state (W212). The core owns the
   file, the grammar and the checks (core/xpp_keymap.h); this slice holds the effective keymap for the
   dispatcher (ui/hotkeys.ts), the editor and the toolbar, and turns each edit the user makes into the
   differences a `set` sends: all of them pure functions of the keymap the core last sent, so the page
   keeps no second copy of the user's settings. Pure: no DOM, no I/O. */
import type {CommandRow} from '../protocol/kinds';
import type {KeymapInfo, UserKeymap} from '../protocol/types';

/** a key being recorded: command `id`'s key number `index` (`index` = its key count: a new second binding) */
export interface Recording {
  id: string;
  index: number;
}

/** a recorded key another command has (docs/command-design.md: "Ctrl+B is Run from last state. Replace / Cancel") */
export interface Conflict {
  id: string;
  index: number;
  key: string;
  /** the command that has it */
  other: string;
}

/** the editor's state, in the store so a check can read it (window.__xpp.state()) */
export interface KeymapEditor {
  open: boolean;
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

const closedEditor: KeymapEditor = {open: false, query: '', recording: null, conflict: null, refusal: '', confirmingReset: false};
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
      return {...state, editor: {...closedEditor, open: true}};
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

/** the effective keys of command `row` (its table keys before hello, or for a row the keymap lacks) */
export function effectiveKeys(info: KeymapInfo | null, row: Pick<CommandRow, 'id' | 'default_keys'>): string[] {
  return (info && commandsById(info).get(row.id)?.keys) ?? row.default_keys;
}

/** whether command `id`'s keys are the user's */
export function isChanged(info: KeymapInfo | null, id: string): boolean {
  return !!info && commandsById(info).get(id)?.source === 'user';
}

/** the user's differences as a `set` sends them: the commands whose source is `user`, the pinned ones
    and the preset. A keymap the core marked as an error has no user part (its defaults are shown) */
export function userKeymap(info: KeymapInfo): UserKeymap {
  const bindings: Record<string, string[]> = {};
  for (const c of info.commands) if (c.source === 'user') bindings[c.id] = c.keys;
  return {preset: info.preset, pinned: info.pinned, bindings};
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
  return withKeys(info, row.id, keys);
}

/** the keymap with command `id`'s key number `index` removed */
export function withoutKey(info: KeymapInfo, row: CommandRow, index: number): UserKeymap {
  return withKeys(info, row.id, effectiveKeys(info, row).filter((_, i) => i !== index));
}

/** the keymap with command `id` back on the table's keys */
export function withoutBinding(info: KeymapInfo, id: string): UserKeymap {
  const user = userKeymap(info);
  return {...user, bindings: Object.fromEntries(Object.entries(user.bindings).filter(([bound]) => bound !== id))};
}

/** the keymap with `pinned` the quick-access toolbar's commands, in order */
export function withPinned(info: KeymapInfo, pinned: string[]): UserKeymap {
  return {...userKeymap(info), pinned};
}

export function withPreset(info: KeymapInfo, preset: UserKeymap['preset']): UserKeymap {
  return {...userKeymap(info), preset};
}

/** a key is the start of another, or the other of it (a chord's first part): the core's keys_clash */
export function keysClash(a: string, b: string): boolean {
  const pa = a.split(' '), pb = b.split(' ');
  const n = Math.min(pa.length, pb.length);
  return pa.slice(0, n).every((part, i) => part === pb[i]);
}

/** the command other than `id` that has a key clashing with `key`, if any */
export function conflictOf(info: KeymapInfo, rows: readonly CommandRow[], id: string, key: string): CommandRow | undefined {
  return rows.find(row => row.id !== id && effectiveKeys(info, row).some(k => keysClash(k, key)));
}

/** the keymap with `key` given to command `row` as its key number `index`, and taken from every other
    command that has it (what Replace sends: the conflicting command loses the key, `row` gets it) */
export function replaceKey(info: KeymapInfo, rows: readonly CommandRow[], row: CommandRow, index: number, key: string): UserKeymap {
  const user = withKey(info, row, index, key);
  const bindings = {...user.bindings};
  for (const other of rows) {
    const keys = effectiveKeys(info, other);
    if (other.id !== row.id && keys.some(k => keysClash(k, key))) bindings[other.id] = keys.filter(k => !keysClash(k, key));
  }
  return {...user, bindings};
}

/** where a command's keys come from, as the editor says it: the table's (default), the user's own for a
    command that has none by default (user), or the user's instead of the table's (changed) */
export function sourceLabel(info: KeymapInfo | null, row: Pick<CommandRow, 'id' | 'default_keys'>): 'default' | 'changed' | 'user' {
  return !isChanged(info, row.id) ? 'default' : row.default_keys.length ? 'changed' : 'user';
}

/** what a recorded key is refused for, or null: a key the system keeps, or one more than a command may have */
export function keyProblem(info: KeymapInfo, row: CommandRow, index: number, key: string): string | null {
  if (info.reserved.includes(key))
    return `${key} is kept by the system or the browser; choose another key.`;
  if (index >= effectiveKeys(info, row).length && effectiveKeys(info, row).length >= info.limits.keys)
    return `${row.label} has the most keys it may (${info.limits.keys}); remove one first.`;
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
  if (name === 'Esc') return {kind: 'cancel'};
  if (name === 'Backspace') return {kind: 'clear'};
  const row = rows.find(r => r.id === rec.id);
  if (!row) return {kind: 'cancel'};
  const problem = keyProblem(info, row, rec.index, name);
  if (problem) return {kind: 'refused', text: problem};
  const other = conflictOf(info, rows, rec.id, name);
  if (other) return {kind: 'conflict', conflict: {...rec, key: name, other: other.id}};
  return {kind: 'set', key: name};
}

/** The rows the editor lists for `query`: every word matches the label, description, category, id or
    one of the keys (typed as "Ctrl+Enter" or "ctrl enter": the words are matched one by one), in the table's order. The
    shortcut-layer switches take no keys and are not listed. */
export function editorRows(info: KeymapInfo | null, rows: readonly CommandRow[], query: string): CommandRow[] {
  const words = query.toLocaleLowerCase().split(/[\s+]+/).filter(Boolean);
  return rows.filter(row => row.category !== 'layer' && words.every(word =>
    `${row.label} ${row.description} ${row.category} ${row.id} ${effectiveKeys(info, row).join(' ')}`.toLocaleLowerCase().replace(/\+/g, ' ').includes(word)));
}

/** the quick-access toolbar: the pinned commands that exist and may be pinned, in order. The ids of
    `info.pinned` are the core's, already checked; a row the table lacks cannot be there */
export function pinnedRows(info: KeymapInfo | null, rows: readonly CommandRow[]): CommandRow[] {
  return (info?.pinned ?? []).flatMap(id => rows.filter(row => row.id === id && row.pinnable));
}

/** `pinned` with `id` moved to position `to` (dragging a button before or after another) */
export function movePinned(pinned: readonly string[], id: string, to: number): string[] {
  const from = pinned.indexOf(id);
  if (from < 0) return pinned.slice();
  const rest = pinned.filter(p => p !== id);
  rest.splice(Math.max(0, Math.min(to, rest.length)), 0, id);
  return rest;
}

/** `pinned` with `id` added at the end, or removed when it is there */
export function togglePinned(pinned: readonly string[], id: string): string[] {
  return pinned.includes(id) ? pinned.filter(p => p !== id) : [...pinned, id];
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
