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
   once, like File/Import settings -- not staged here as a pending edit, since
   the page no longer parses the file itself. No undo (GitHub #110):
   Reset (one field, or every field of a section) is the way back,
   through the model's own values. Pure: no DOM, no I/O. */
import type {Command, HelloEvent} from '../protocol/types';
import {stateSliders, type SliderDef} from './sliders';

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
  /** the authoritative definitions from every state event */
  | {type: 'stateSliders'; defs: {name: string; lo: number; hi: number; step?: number}[]};

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
    case 'stateSliders':
      return {...state, sliders: stateSliders(action.defs, 1),
        nextSlider: (action.defs.findIndex(d => !d.name) + 1) || action.defs.length + 1};

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

/** a command and the edits it sends */
export interface ValueCommand {
  command: Command;
  sets: ValueSet[];
}

/** hello's registry argument types of a command (docs/protocol.md "Argument types"), by name */
function argumentTypes(hello: HelloEvent | null, name: string): Record<string, string> {
  const row = hello?.commands.find(c => c.cmd === name);
  return Object.fromEntries((row?.arguments ?? []).map(a => [a.name, a.type]));
}

/** a numerics edit's text as the registry argument's type takes it */
function argumentValue(type: string | undefined, text: string): unknown {
  if (type === 'real' || type === 'integer') return Number(text);
  if (type === 'boolean') return text === 'true';
  return text;
}

/** The commands that send these edits (docs/protocol.md): parameters, boundary conditions, delays and the
    numerics no registry command takes go in one `set` (one value, or `values`); initial conditions in
    `ic.set` (a number) or `ic.formula` (a %formula, one each); the run's and the shooting's numerics in
    `run.configure` and `bvp.configure`. */
/** the equilibrium.configure arguments the Numerics sIng pt settings are (a numerics key to its argument) */
const EQUILIBRIUM_ARGUMENTS: Record<string, string> = {max_iterations: 'max_iterates', newton_tolerance: 'newton_tolerance', jacobian_step: 'jacobian_epsilon', search_limit: 'search_limit'};

export function valueCommands(hello: HelloEvent | null, sets: ValueSet[]): ValueCommand[] {
  const run = argumentTypes(hello, 'run.configure'), bvp = argumentTypes(hello, 'bvp.configure');
  const BVP = 'bvp_';
  const meshes: ValueSet[] = [], equilibria: ValueSet[] = [], plain: ValueSet[] = [], ics: ValueSet[] = [], formulas: ValueSet[] = [], runs: ValueSet[] = [], bvps: ValueSet[] = [];
  for (const s of sets) {
    if (s.kind === 'ic') (s.text.trim().startsWith('%') ? formulas : ics).push(s);
    else if (s.kind === 'num' && s.name !== undefined && s.name in run) runs.push(s);
    else if (s.kind === 'num' && s.name?.startsWith(BVP) && s.name.slice(BVP.length) in bvp) bvps.push(s);
    else if (s.kind === 'num' && s.name === 'nullcline_mesh') meshes.push(s);
    else if (s.kind === 'num' && s.name !== undefined && s.name in EQUILIBRIUM_ARGUMENTS) equilibria.push(s);
    else plain.push(s);
  }
  const result: ValueCommand[] = [];
  if (plain.length) result.push({command: plain.length === 1 ? {cmd: 'set', ...setMembers(plain[0])} : {cmd: 'set', values: plain.map(setMembers)}, sets: plain});
  if (ics.length) result.push({command: {cmd: 'ic.set', values: Object.fromEntries(ics.map(s => [s.name!, Number(s.text)]))}, sets: ics});
  for (const s of formulas)
    result.push({command: {cmd: 'ic.formula', variable: s.name!, formula: s.text.trim().slice(1)}, sets: [s]});
  for (const s of meshes) result.push({command: {cmd: 'nullcline.set_mesh', mesh: Number(s.text)}, sets: [s]});
  if (equilibria.length) result.push({command: {cmd: 'equilibrium.configure', ...Object.fromEntries(equilibria.map(s => [EQUILIBRIUM_ARGUMENTS[s.name!], Number(s.text)]))}, sets: equilibria});
  if (runs.length) result.push({command: {cmd: 'run.configure', ...Object.fromEntries(runs.map(s => [s.name!, argumentValue(run[s.name!], s.text)]))}, sets: runs});
  if (bvps.length) result.push({command: {cmd: 'bvp.configure', ...Object.fromEntries(bvps.map(s => [s.name!.slice(BVP.length), argumentValue(bvp[s.name!.slice(BVP.length)], s.text)]))}, sets: bvps});
  return result;
}

/** display precision (A14): six significant digits */
export function sixSig(n: number): string {
  return significant(n, 6);
}

/** Inspection uses ten significant digits; editing retains the full double. */
export function inspectNumber(n: number): string {
  if (Number.isFinite(n) && n !== 0 && (Math.abs(n) >= 1e9 || Math.abs(n) < 1e-4)) {
    return n.toExponential(9).replace(/\.?0+(?=e)/, '');
  }
  return significant(n, 10);
}

function significant(n: number, digits: number): string {
  return Number.isFinite(n) ? String(Number(n.toPrecision(digits))) : String(n);
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
