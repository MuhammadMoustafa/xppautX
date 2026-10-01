/* Parameters, initial conditions, boundary conditions and delays
   (docs/protocol.md `set`, `slide`, `default`; docs/ui-v2.md T3,
   GitHub #155): the values themselves stay in AppState.core (the `state`
   event), sent by the core. They are settings (W106): an edit (a slider,
   a value field, Default/Reset, a numerics field) goes to the core at
   once as a `set`, busy or idle, and during a computation the core keeps
   it for when that ends (the run in progress keeps the values it started
   with). This slice is only what the page adds: the edits sent whose
   `set` has not ended yet (`inflight`: each field shows its latest one
   as its value until the core's own state takes over after that set's
   idle -- no "pending" state, the value shown is the value), which of
   them a `message` `error` belongs to (the one whose command runs, so it
   is shown on that field, not as a modal, A11/A14), the model's defaults
   and the sliders under the plot (store/sliders.ts).
   Save and Load of a section's .par/.ic go through the core (W66, then
   the W66 review): both are the `values` command (session.ts
   saveValues/loadValues), Save's write delivered like any other file the
   core writes (session.ts's `pendingSave`), Load's read applying at
   once, like File/Read set -- not staged here as a pending edit, since
   the page no longer parses the file itself. No undo (GitHub #110):
   Reset (one field, or every field of a section) is the way back,
   through the model's own values. Pure: no DOM, no I/O. */
import type {Command} from '../protocol/types';
import {presetSliders, type SliderDef} from './sliders';

export type ValueKind = 'par' | 'ic' | 'bc' | 'delay' | 'num';

/** a value to send: `name` for par/ic and a numerics field's key (num),
    `index` for bc/delay (docs/protocol.md `set`) */
export interface ValueSet {
  kind: ValueKind;
  name?: string;
  index?: number;
  text: string;
}

/** an edit sent as its own `set`, until that command's idle: `ahead` is
    how many other commands' idles come first (0: its own command is the
    one running or the next) */
export interface InFlight {
  set: ValueSet;
  ahead: number;
}

export interface ValuesState {
  /** field id -> the error the core sent for it, until the next edit on it */
  errors: Record<string, string>;
  /** the edits sent whose `set` has not ended, in the order sent */
  inflight: InFlight[];
  /** the model file's values by field key (hello.defaults, by the first state's names) */
  defaults: Record<string, number> | null;
  /** the sliders under the plot, and the id the next one gets */
  sliders: SliderDef[];
  nextSlider: number;
}

export const initialValues: ValuesState = {
  errors: {}, inflight: [], defaults: null, sliders: [], nextSlider: 1,
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
  /** a field's edit, sent as its own `set`, `ahead` idles before its own */
  | {type: 'sent'; set: ValueSet; ahead: number}
  /** a command ended: the set whose command it was is done, the others one idle closer */
  | {type: 'idle'}
  /** a `message` `error`: its `field` (the core says which value of a `set` it refused), else the
      running set's, when a set runs */
  | {type: 'error'; text: string; field?: string}
  /** Escape dropped a draft that carried the core's refusal for `field` (WF-001, ui/Field.tsx
      onDropError): the box goes back to what the core has, so its error is forgotten too,
      without sending anything */
  | {type: 'clearError'; field: string}
  /** a new connection: nothing sent is waited for any more */
  | {type: 'settled'}
  /** the Default or Reset button: the section's errors go (its edits are sent as any) */
  | {type: 'defaulted'; kind: ValueKind}
  /** the model's values: hello.defaults by name */
  | {type: 'defaults'; pars: [string, number][]; ics: [string, number][]}
  /** the model's `@ s1=..` presets, on a (re)connection: the list starts with them when it is empty */
  | {type: 'presetSliders'; defs: {name: string; lo: number; hi: number}[]}
  | {type: 'addSlider'}
  /** the Add/Edit slider dialog's OK, adding one fully formed (SliderDialog.tsx) */
  | {type: 'addSliderWith'; def: Omit<SliderDef, 'id'>}
  | {type: 'setSlider'; id: number; patch: Partial<Omit<SliderDef, 'id'>>}
  | {type: 'removeSlider'; id: number};

export function reduceValues(state: ValuesState, action: ValuesAction): ValuesState {
  switch (action.type) {
    case 'sent':
      return {...state, errors: omit(state.errors, setKey(action.set)),
        inflight: [...state.inflight, {set: action.set, ahead: action.ahead}]};
    case 'idle':
      return state.inflight.length
        ? {...state, inflight: state.inflight.filter(f => f.ahead > 0).map(f => ({...f, ahead: f.ahead - 1}))} : state;
    case 'error': {
      const running = state.inflight.find(f => f.ahead === 0);
      if (!running) return state;
      const key = action.field || setKey(running.set);
      return {...state, errors: {...state.errors, [key]: action.text}};
    }
    case 'clearError': {
      const errors = omit(state.errors, action.field);
      return errors === state.errors ? state : {...state, errors};
    }
    case 'settled':
      return state.inflight.length ? {...state, inflight: []} : state;
    case 'defaulted': {
      const prefix = `${action.kind}:`;
      const keys = Object.keys(state.errors).filter(k => k.startsWith(prefix));
      if (!keys.length) return state;
      const errors = {...state.errors};
      for (const k of keys) delete errors[k];
      return {...state, errors};
    }
    case 'defaults': {
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
  }
}

function setKey(s: ValueSet): string {
  return fieldKey(s.kind, s.index ?? s.name!);
}

/** the text of field `key`'s latest edit whose `set` has not ended, or
    null: what the field shows until the core's state takes over */
export function sentText(inflight: InFlight[], key: string): string | null {
  for (let i = inflight.length - 1; i >= 0; i--) if (setKey(inflight[i].set) === key) return inflight[i].set.text;
  return null;
}

function setMembers(s: ValueSet): Record<string, unknown> {
  return s.index !== undefined ? {kind: s.kind, index: s.index, text: s.text} : {kind: s.kind, name: s.name, text: s.text};
}

/** the one command that sends these values (docs/protocol.md `set`: one value, or `values`) */
export function valueSetCommand(sets: ValueSet[]): Command | null {
  if (!sets.length) return null;
  return sets.length === 1 ? {cmd: 'set', ...setMembers(sets[0])} : {cmd: 'set', values: sets.map(setMembers)};
}

/** display precision (A14): six significant digits */
export function sixSig(n: number): string {
  return Number.isFinite(n) ? String(Number(n.toPrecision(6))) : String(n);
}

/* the Values panel's folding (W99): a section that starts folded (the model's boundary
   conditions) is remembered by "+id" when the person opened it, any other by "id" when
   they folded it */
export const foldKey = (id: string, startFolded = false): string => (startFolded ? '+' + id : id);
export const isFolded = (stored: readonly string[], id: string, startFolded = false): boolean =>
  stored.includes(foldKey(id, startFolded)) !== startFolded;

/* the boundary-conditions section shows only for a model that defines some: the core sends
   `bcs` only then (docs/protocol.md) */
export const showsBcSection = (bcs: readonly unknown[] | undefined): boolean => (bcs?.length ?? 0) > 0;
