/* Parameters, initial conditions, boundary conditions and delays
   (docs/protocol.md `value.set_parameters`, `set`; docs/ui-v2.md T3,
   GitHub #155): the values themselves stay in AppState.core (the `state`
   event), sent by the core. They are settings (W106): an edit (a slider,
   a value field, Default/Reset, a numerics field) goes to the core at
   once as a `set`, busy or idle, and during a computation the core keeps
   it for when that ends (the run in progress keeps the values it started
   with). This slice is only what the page adds: the edits sent whose
   `set` has not ended yet (`inflight`: each field shows its latest one
   as its value until the core's own state takes over after that set's done -- no "pending" state, the value shown is the value), which of
   them a `message` `error` belongs to (the one whose command runs, so it
   is shown on that field, not as a modal, A11/A14), the model's defaults
   and the sliders under the plot (store/sliders.ts).
   Load of a section's XPPAUT .par/.ic goes through the core (W66, then
   the W66 review, an import since W321: nothing of ours writes these
   files): the `value.import_file` command (session.ts loadValues), applying at
   once, like File/Import XPPAUT session state -- not staged here as a pending
   edit, since the page no longer parses the file itself. No undo (GitHub #110):
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

/** an edit sent as its own `set`, until that command's done: `ahead` is
    how many other commands' dones come first (0: its own command is the
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
  /** a field's edit, sent as its own `set`, `ahead` dones before its own */
  | {type: 'sent'; set: ValueSet; ahead: number}
  /** a command ended: the set whose command it was is done, the others one done closer */
  | {type: 'ended'}
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
    case 'ended':
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

/** a parameter's typed text as the registry's `real` takes it: the number it is, else the text itself, which
    the core refuses naming the argument (a blank is no 0) */
function parameterValue(text: string): number | string {
  const trimmed = text.trim();
  return trimmed !== '' && Number.isFinite(Number(trimmed)) ? Number(trimmed) : text;
}

/** the keys of the equilibria group's rows (the settings panel's, hello.options): the numerics the Sing pt settings are */
const EQUILIBRIA_GROUP = 'equilibria';
const equilibriaKeys = (hello: HelloEvent | null): string[] => (hello?.options ?? []).find(g => g.group === EQUILIBRIA_GROUP)?.rows.map(r => r.key) ?? [];

/** The commands that send these edits (docs/protocol.md): parameters in `value.set_parameters` (a number) or
    `value.set_by_formula` (a %formula, one each); boundary conditions, delays and the numerics no registry
    command takes go in one `set` (one value, or `values`); initial conditions in `initial_conditions.set` (a number) or
    `initial_conditions.set_by_formula` (a %formula, one each); the run's and the shooting's numerics in `run.configure` and
    `boundary_value.configure`; the nullcline mesh and the equilibria's settings in their own. */
export function valueCommands(hello: HelloEvent | null, sets: ValueSet[]): ValueCommand[] {
  const run = argumentTypes(hello, 'run.configure'), bvp = argumentTypes(hello, 'boundary_value.configure'), equilibriumKeys = equilibriaKeys(hello);
  const BVP = 'bvp_';
  const meshes: ValueSet[] = [], equilibria: ValueSet[] = [], plain: ValueSet[] = [], pars: ValueSet[] = [], par_formulas: ValueSet[] = [],
    ics: ValueSet[] = [], formulas: ValueSet[] = [], runs: ValueSet[] = [], bvps: ValueSet[] = [];
  for (const s of sets) {
    if (s.kind === 'par') (s.text.trim().startsWith('%') ? par_formulas : pars).push(s);
    else if (s.kind === 'ic') (s.text.trim().startsWith('%') ? formulas : ics).push(s);
    else if (s.kind === 'num' && s.name !== undefined && s.name in run) runs.push(s);
    else if (s.kind === 'num' && s.name?.startsWith(BVP) && s.name.slice(BVP.length) in bvp) bvps.push(s);
    else if (s.kind === 'num' && s.name === 'nullcline_mesh') meshes.push(s);
    else if (s.kind === 'num' && s.name !== undefined && equilibriumKeys.includes(s.name)) equilibria.push(s);
    else plain.push(s);
  }
  const result: ValueCommand[] = [];
  if (plain.length) result.push({command: plain.length === 1 ? {cmd: 'set', ...setMembers(plain[0])} : {cmd: 'set', values: plain.map(setMembers)}, sets: plain});
  if (pars.length) result.push({command: {cmd: 'value.set_parameters', values: Object.fromEntries(pars.map(s => [s.name!, parameterValue(s.text)]))}, sets: pars});
  for (const s of par_formulas)
    result.push({command: {cmd: 'value.set_by_formula', name: s.name!, formula: s.text.trim().slice(1)}, sets: [s]});
  if (ics.length) result.push({command: {cmd: 'initial_conditions.set', values: Object.fromEntries(ics.map(s => [s.name!, Number(s.text)]))}, sets: ics});
  for (const s of formulas)
    result.push({command: {cmd: 'initial_conditions.set_by_formula', variable: s.name!, formula: s.text.trim().slice(1)}, sets: [s]});
  for (const s of meshes) result.push({command: {cmd: 'nullcline.set_mesh', mesh: Number(s.text)}, sets: [s]});
  if (equilibria.length) result.push({command: {cmd: 'options.configure', group: EQUILIBRIA_GROUP, values: Object.fromEntries(equilibria.map(s => [s.name!, s.text.trim()]))}, sets: equilibria});
  if (runs.length) result.push({command: {cmd: 'run.configure', ...Object.fromEntries(runs.map(s => [s.name!, argumentValue(run[s.name!], s.text)]))}, sets: runs});
  if (bvps.length) result.push({command: {cmd: 'boundary_value.configure', ...Object.fromEntries(bvps.map(s => [s.name!.slice(BVP.length), argumentValue(bvp[s.name!.slice(BVP.length)], s.text)]))}, sets: bvps});
  return result;
}

/* ---- number text: the one owner of how the page writes a number (W640) ----
   Every read-only number goes through shownNumber (six significant digits, A14; an exponent outside
   EXPONENT_FROM_BELOW..EXPONENT_FROM; a flag saying digits were dropped, which ui/CutText.tsx marks with a muted
   … and answers with the full value in a tip). Nothing else in web2/src calls toPrecision, toExponential or
   toFixed (tools/sourcecheck.sh's numbercheck). */

/** a read-only number's significant digits (A14) */
export const SHOWN_DIGITS = 6;
/** a number whose size is below this, or from EXPONENT_FROM up, is written with an exponent: 0.0001 is the smallest plain number */
export const EXPONENT_BELOW = 1e-4;
/** ... and 1000000 the first one written with an exponent */
export const EXPONENT_FROM = 1e6;

/** a number as the page shows it: `head` and `tail` make the short text (the tail is the exponent, "e+8", else ""),
    `cut` says digits were dropped, `full` is the exact value as the shortest text that reads back as the number */
export interface ShownNumber {
  head: string;
  tail: string;
  cut: boolean;
  full: string;
}

export function shownNumber(n: number): ShownNumber {
  const full = String(n);
  if (!Number.isFinite(n)) return {head: full, tail: '', cut: false, full};
  const a = Math.abs(n);
  if (n !== 0 && (a < EXPONENT_BELOW || a >= EXPONENT_FROM)) {
    const [mantissa, exponent] = n.toExponential(SHOWN_DIGITS - 1).split('e');
    const head = mantissa.includes('.') ? mantissa.replace(/\.?0+$/, '') : mantissa, tail = `e${exponent}`;
    return {head, tail, cut: Number(head + tail) !== n, full};
  }
  const head = String(Number(n.toPrecision(SHOWN_DIGITS)));
  return {head, tail: '', cut: Number(head) !== n, full};
}

/** the short text of `n` in one string (a title, an aria label, a sentence), without the cut mark */
export function sixSig(n: number): string {
  const {head, tail} = shownNumber(n);
  return head + tail;
}

/** the text an input box shows while it is not being edited: ten significant digits (editing keeps the full double) */
export function inputText(n: number): string {
  if (Number.isFinite(n) && n !== 0 && (Math.abs(n) >= 1e9 || Math.abs(n) < EXPONENT_BELOW)) {
    return n.toExponential(9).replace(/\.?0+(?=e)/, '');
  }
  return String(Number(n.toPrecision(10)));
}

/** `n` as a field takes it back: the fewest digits from six on that read back as exactly `n` */
export function exactText(n: number): string {
  for (let p = SHOWN_DIGITS; p <= 17; p++) {
    const t = String(Number(n.toPrecision(p)));
    if (Number(t) === n) return t;
  }
  return String(n);
}

/** `v` rounded to the decimals of `step`, so 0.1 + 0.2 shows as 0.3 */
export function steppedText(v: number, step: number): string {
  const decimals = Math.max(0, -Math.floor(Math.log10(Math.abs(step))) + 1);
  return String(Number(v.toFixed(Math.min(decimals, 15))));
}

/** `n` with exactly `decimals` decimals (a duration's tenth of a second) */
export function fixedText(n: number, decimals: number): string {
  return n.toFixed(decimals);
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
