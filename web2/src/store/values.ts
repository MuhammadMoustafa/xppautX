/* Parameters, initial conditions, boundary conditions and delays
   (docs/protocol.md `set`, `slide`, `default`; docs/ui-v2.md T3,
   GitHub #117): the values themselves stay in AppState.core (the `state`
   event), sent by the core. This slice is only what the page adds: the
   edits not yet sent (sliders, value fields, Default/Reset, a loaded
   .par/.ic: the latest per field, shown as pending until session.ts sends
   them all in one `set` right before the next command that computes, so
   every computation uses what the panel shows -- nothing is sent on a
   plain edit, busy or idle), which field an edit is attributed to (so a
   `message` `error` can be shown on that field, not as a modal, A11/A14),
   the model's defaults, the sliders under the plot (store/sliders.ts) and
   the last Save (store/valueFiles.ts). No undo (GitHub #110): Reset (one
   field, or every field of a section) is the way back, through the
   model's own values. Pure: no DOM, no I/O. */
import type {Command} from '../protocol/types';
import {presetSliders, type SliderDef} from './sliders';

export type ValueKind = 'par' | 'ic' | 'bc' | 'delay';

/** an edit not yet sent, or a value to send: `name` for par/ic, `index`
    for bc/delay (docs/protocol.md `set`) */
export interface ValueSet {
  kind: ValueKind;
  name?: string;
  index?: number;
  text: string;
}

export interface ValuesState {
  /** field id -> the error the core sent for it, until the next edit on it or a clean idle */
  errors: Record<string, string>;
  /** the field id the next `message` `error` is attributed to (the edit
      most recently flushed, until its `idle`) */
  attributing: string | null;
  /** edits not yet sent: the latest per field, in the order last changed */
  pending: ValueSet[];
  /** the model file's values by field key (hello.defaults, else the first state) */
  defaults: Record<string, number> | null;
  /** the sliders under the plot, and the id the next one gets */
  sliders: SliderDef[];
  nextSlider: number;
  /** the text the last Save wrote (tests read it; the browser downloads it) */
  lastSaved: {kind: 'par' | 'ic'; text: string} | null;
}

export const initialValues: ValuesState = {
  errors: {}, attributing: null, pending: [], defaults: null, sliders: [], nextSlider: 1, lastSaved: null,
};

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
  /** a field's edit: kept pending (not sent) until the next flush */
  | {type: 'edit'; set: ValueSet}
  | {type: 'error'; text: string}
  /** Escape dropped a draft that carried the core's refusal for `field` (WF-001, ui/Field.tsx
      onDropError): the box goes back to what the core has, so its error is forgotten too,
      without sending anything */
  | {type: 'clearError'; field: string}
  | {type: 'settled'}
  /** the Default or Reset button: pending edits from the ODE file's values, not itself undoable */
  | {type: 'defaulted'; kind: ValueKind}
  /** the pending edits went out as one `set`, attributed to `field` when exactly one was sent */
  | {type: 'flushed'; field: string | null}
  /** the model's values: from hello.defaults, or (`ifUnset`) the first state's */
  | {type: 'defaults'; pars: [string, number][]; ics: [string, number][]; ifUnset?: boolean}
  /** the model's `@ s1=..` presets, on a (re)connection: the list starts with them when it is empty */
  | {type: 'presetSliders'; defs: {name: string; lo: number; hi: number}[]}
  | {type: 'addSlider'}
  /** the Add/Edit slider dialog's OK, adding one fully formed (SliderDialog.tsx) */
  | {type: 'addSliderWith'; def: Omit<SliderDef, 'id'>}
  | {type: 'setSlider'; id: number; patch: Partial<Omit<SliderDef, 'id'>>}
  | {type: 'removeSlider'; id: number}
  | {type: 'saved'; kind: 'par' | 'ic'; text: string};

export function reduceValues(state: ValuesState, action: ValuesAction): ValuesState {
  switch (action.type) {
    case 'edit': {
      const field = setKey(action.set);
      return {...state, errors: omit(state.errors, field), pending: queueSet(state.pending, action.set)};
    }
    case 'error':
      return state.attributing ? {...state, errors: {...state.errors, [state.attributing]: action.text}} : state;
    case 'clearError': {
      const errors = omit(state.errors, action.field);
      return errors === state.errors ? state : {...state, errors};
    }
    case 'settled':
      return state.attributing ? {...state, attributing: null} : state;
    case 'defaulted': {
      const prefix = `${action.kind}:`;
      const keys = Object.keys(state.errors).filter(k => k.startsWith(prefix));
      if (!keys.length) return state;
      const errors = {...state.errors};
      for (const k of keys) delete errors[k];
      return {...state, errors};
    }
    case 'flushed':
      return state.pending.length || state.attributing !== action.field
        ? {...state, pending: [], attributing: action.field} : state;
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
      return {...state, sliders: [...state.sliders, {id: state.nextSlider, name: '', lo: '', hi: '', step: ''}],
        nextSlider: state.nextSlider + 1};
    case 'addSliderWith':
      return {...state, sliders: [...state.sliders, {id: state.nextSlider, ...action.def}], nextSlider: state.nextSlider + 1};
    case 'setSlider':
      return {...state, sliders: state.sliders.map(s => (s.id === action.id ? {...s, ...action.patch} : s))};
    case 'removeSlider':
      return {...state, sliders: state.sliders.filter(s => s.id !== action.id)};
    case 'saved':
      return {...state, lastSaved: {kind: action.kind, text: action.text}};
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

/** whether field `key` has an edit still waiting to be sent */
export function isPending(queue: ValueSet[], key: string): boolean {
  return queue.some(q => setKey(q) === key);
}

function setMembers(s: ValueSet): Record<string, unknown> {
  return s.index !== undefined ? {kind: s.kind, index: s.index, text: s.text} : {kind: s.kind, name: s.name, text: s.text};
}

/** the one command that sends every pending edit (docs/protocol.md `set`: one value, or `values`) */
export function setCommand(sets: ValueSet[]): Command | null {
  if (!sets.length) return null;
  return sets.length === 1 ? {cmd: 'set', ...setMembers(sets[0])} : {cmd: 'set', values: sets.map(setMembers)};
}

/** display precision (A14): six significant digits */
export function sixSig(n: number): string {
  return Number.isFinite(n) ? String(Number(n.toPrecision(6))) : String(n);
}
