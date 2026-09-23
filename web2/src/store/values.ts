/* Parameters, initial conditions, boundary conditions and delays
   (docs/protocol.md `set`, `slide`, `default`; docs/ui-v2.md T3): the
   values themselves stay in AppState.core (the `state` event), sent by the
   core. This slice is only what the page adds: which field an edit is
   pending on (so a `message` `error` can be shown on that field, not as a
   modal, A11/A14), the undo history (A12: session.ts's undoValue()
   sends `set` again with the previous text), the edits made while a
   command runs (GitHub #18: the latest per field, sent as one `set` when
   it ends, with at most one run), the model's defaults, the sliders under
   the plot (store/sliders.ts) and the last Save (store/valueFiles.ts).
   Pure: no DOM, no I/O. */
import type {Command} from '../protocol/types';
import {presetSliders, type SliderDef} from './sliders';

export type ValueKind = 'par' | 'ic' | 'bc' | 'delay';

/** a value to send: `name` for par/ic, `index` for bc/delay (docs/protocol.md `set`) */
export interface ValueSet {
  kind: ValueKind;
  name?: string;
  index?: number;
  text: string;
}

/** one committed edit, kept for Undo; `name` for par/ic, `index` for bc/delay (docs/protocol.md `set`) */
export interface ValueEdit {
  kind: ValueKind;
  name?: string;
  index?: number;
  /** the field's text before this edit, to send back on Undo */
  previous: string;
}

export interface ValuesState {
  /** field id -> the error the core sent for it, until the next edit on it or a clean idle */
  errors: Record<string, string>;
  /** the field id the next `message` `error` is attributed to (the edit most recently sent, until its `idle`) */
  pending: string | null;
  /** committed edits, oldest first */
  history: ValueEdit[];
  /** edits made while a command runs: the latest per field, in the order last changed */
  queue: ValueSet[];
  /** a run follows the queued edits once they are sent */
  queueRerun: boolean;
  /** the model file's values by field key (hello.defaults, else the first state) */
  defaults: Record<string, number> | null;
  /** the sliders under the plot, and the id the next one gets */
  sliders: SliderDef[];
  nextSlider: number;
  /** the text the last Save wrote (tests read it; the browser downloads it) */
  lastSaved: {kind: 'par' | 'ic'; text: string} | null;
  /** a parameter or IC edit integrates again (the panel's "Run on change") */
  runOnChange: boolean;
}

export const initialValues: ValuesState = {
  errors: {}, pending: null, history: [], queue: [], queueRerun: false, defaults: null, sliders: [], nextSlider: 1,
  lastSaved: null, runOnChange: true,
};

const HISTORY_KEEP = 50;

/** a field's identity as a store key: names fold case, as the core matches them (docs/protocol.md `set`) */
export function fieldKey(kind: ValueKind, nameOrIndex: string | number): string {
  return `${kind}:${typeof nameOrIndex === 'number' ? nameOrIndex : nameOrIndex.toLowerCase()}`;
}

function omit(o: Record<string, string>, key: string): Record<string, string> {
  if (!(key in o)) return o;
  const rest = {...o};
  delete rest[key];
  return rest;
}

export type ValuesAction =
  | {type: 'edit'; edit: ValueEdit}
  | {type: 'undo'}
  | {type: 'error'; text: string}
  | {type: 'settled'}
  /** the Default button: values from the ODE file (docs/protocol.md `default`); not itself undoable (A12) */
  | {type: 'defaulted'; kind: ValueKind}
  /** values to send once the running command ends; `rerun`: a run follows them */
  | {type: 'queue'; set: ValueSet; rerun: boolean}
  /** the queue went out (flushCommand) */
  | {type: 'flushed'}
  /** the model's values: from hello.defaults, or (`ifUnset`) the first state's */
  | {type: 'defaults'; pars: [string, number][]; ics: [string, number][]; ifUnset?: boolean}
  /** the model's `@ s1=..` presets, on a (re)connection: the list starts with them when it is empty */
  | {type: 'presetSliders'; defs: {name: string; lo: number; hi: number}[]}
  | {type: 'addSlider'}
  | {type: 'setSlider'; id: number; patch: Partial<Omit<SliderDef, 'id'>>}
  | {type: 'removeSlider'; id: number}
  | {type: 'saved'; kind: 'par' | 'ic'; text: string}
  | {type: 'runOnChange'; on: boolean};

export function reduceValues(state: ValuesState, action: ValuesAction): ValuesState {
  switch (action.type) {
    case 'edit': {
      const field = fieldKey(action.edit.kind, action.edit.index ?? action.edit.name!);
      const history = [...state.history, action.edit].slice(-HISTORY_KEEP);
      return {...state, errors: omit(state.errors, field), pending: field, history};
    }
    case 'undo': {
      if (!state.history.length) return state;
      const last = state.history[state.history.length - 1];
      const field = fieldKey(last.kind, last.index ?? last.name!);
      return {...state, errors: omit(state.errors, field), pending: field, history: state.history.slice(0, -1)};
    }
    case 'error':
      return state.pending ? {...state, errors: {...state.errors, [state.pending]: action.text}} : state;
    case 'settled':
      return state.pending ? {...state, pending: null} : state;
    case 'defaulted': {
      const prefix = `${action.kind}:`;
      const keys = Object.keys(state.errors).filter(k => k.startsWith(prefix));
      if (!keys.length) return state;
      const errors = {...state.errors};
      for (const k of keys) delete errors[k];
      return {...state, errors};
    }
    case 'queue':
      return {...state, queue: queueSet(state.queue, action.set), queueRerun: state.queueRerun || action.rerun};
    case 'flushed':
      return state.queue.length || state.queueRerun ? {...state, queue: [], queueRerun: false} : state;
    case 'defaults': {
      if (action.ifUnset && state.defaults) return state;
      const defaults: Record<string, number> = {};
      for (const [n, v] of action.pars) defaults[fieldKey('par', n)] = v;
      for (const [n, v] of action.ics) defaults[fieldKey('ic', n)] = v;
      return {...state, defaults};
    }
    case 'presetSliders':
      if (state.sliders.length || !action.defs.length) return state;
      return {...state, sliders: presetSliders(action.defs, state.nextSlider), nextSlider: state.nextSlider + action.defs.length};
    case 'addSlider':
      return {...state, sliders: [...state.sliders, {id: state.nextSlider, name: '', lo: '', hi: ''}], nextSlider: state.nextSlider + 1};
    case 'setSlider':
      return {...state, sliders: state.sliders.map(s => (s.id === action.id ? {...s, ...action.patch} : s))};
    case 'removeSlider':
      return {...state, sliders: state.sliders.filter(s => s.id !== action.id)};
    case 'saved':
      return {...state, lastSaved: {kind: action.kind, text: action.text}};
    case 'runOnChange':
      return state.runOnChange === action.on ? state : {...state, runOnChange: action.on};
  }
}

function setKey(s: ValueSet): string {
  return fieldKey(s.kind, s.index ?? s.name!);
}

/** `queue` with `s`: an earlier value of the same field is dropped (only the latest goes out) */
export function queueSet(queue: ValueSet[], s: ValueSet): ValueSet[] {
  const key = setKey(s);
  return [...queue.filter(q => setKey(q) !== key), s];
}

/** whether field `key` has a value waiting for the running command to end */
export function isQueued(queue: ValueSet[], key: string): boolean {
  return queue.some(q => setKey(q) === key);
}

function setMembers(s: ValueSet): Record<string, unknown> {
  return s.index !== undefined ? {kind: s.kind, index: s.index, text: s.text} : {kind: s.kind, name: s.name, text: s.text};
}

/** the one command that sends `sets` (docs/protocol.md `set`: one value, or `values`), then runs when `rerun` */
export function setCommand(sets: ValueSet[], rerun: boolean): Command | null {
  if (!sets.length) return null;
  const run = rerun ? {rerun: 1} : {};
  return sets.length === 1 ? {cmd: 'set', ...setMembers(sets[0]), ...run} : {cmd: 'set', values: sets.map(setMembers), ...run};
}

/** display precision (A14): six significant digits */
export function sixSig(n: number): string {
  return Number.isFinite(n) ? String(Number(n.toPrecision(6))) : String(n);
}
