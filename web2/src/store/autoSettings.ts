/* AUTO's settings as data (docs/ui-v2.md T22, docs/protocol.md "AUTO's
   settings as data"): the Numerics, AUTO's parameters, the axes and the
   Mark values, from the `autosettings` event, edited in the page's own
   forms at any time. They are settings (W106): an edit goes out at once
   as an `auto` `set`, during a run too, when the core keeps it for the
   run's end (the run in progress keeps what it started with). Each one
   sent is `inflight` until that set's own idle, when the event has
   brought the core's values (or a `message` `error` said why not).
   What each value takes, and the messages a refusal says, are the core's
   (the event's `rules`, `pairs` and `step`); the settings file is the
   core's too (AUTO's File menu, W118). Pure: no DOM, no I/O. */
import type {Command} from '../protocol/types';
import {fieldError, type FieldSpec} from './fieldKinds';

export type NumKey = 'ntst' | 'nmx' | 'npr' | 'ncol' | 'ds' | 'dsmin' | 'dsmax' | 'rl0' | 'rl1' | 'a0' | 'a1'
  | 'epsl' | 'epsu' | 'epss' | 'iad' | 'mxbf' | 'iid' | 'itmx' | 'itnw' | 'nwtn' | 'iads' | 'suppbp';

/** what a Numerics value takes (the event's `rules`: core/auto_settings.cpp num_fields), and the
    message the core refuses another with; each flag only when it holds */
export interface NumRule {
  /** the core's form label */
  label: string;
  integer?: boolean;
  positive?: boolean;
  nonzero?: boolean;
  min?: number;
  max?: number;
  message: string;
}

/** two values that must be in order: lo below hi (strict) or at most hi */
export interface NumPair {
  lo: NumKey;
  hi: NumKey;
  strict: boolean;
  message: string;
}

/** the first step `key` within the step sizes `lo` to `hi`, whichever its sign */
export interface NumStep {
  key: NumKey;
  lo: NumKey;
  hi: NumKey;
  message: string;
}

export interface NumField {
  key: NumKey;
  /** the field's name in the page: plain words and AUTO's short name (T23), "Max points (NMX)" */
  name: string;
  /** its tooltip: what it does, its valid values and what each option means (AUTO's manual) */
  help: string;
}

/** the Numerics form's fields in its order (core/auto_settings.cpp num_fields) */
export const NUM_FIELDS: NumField[] = [
  {key: 'ntst', name: 'Mesh intervals (NTST)',
    help: 'The mesh intervals a periodic orbit (or a boundary value solution) is split into. More follow a sharp orbit '
      + 'better but take longer: raise it when a periodic branch looks wrong or does not converge. Following a period '
      + 'doubling doubles it, so set it back after. A whole number, at least 1.'},
  {key: 'nmx', name: 'Max points (NMX)',
    help: 'The most points a branch may have: it ends (EP, "reached Max points") when it has this many. '
      + 'A whole number, at least 1.'},
  {key: 'npr', name: 'Label every (NPR)',
    help: 'Besides the special points, label and save the whole solution every NPR points along a branch, so it can be '
      + 'grabbed. A whole number, at least 1.'},
  {key: 'ncol', name: 'Collocation points (NCOL)',
    help: 'The collocation points in each mesh interval of a periodic orbit or boundary value solution. '
      + 'A whole number from 2 to 7; 4 is usual.'},
  {key: 'ds', name: 'First step (DS)',
    help: 'The first step along the branch. Its sign is the direction: positive makes the main parameter go up, '
      + 'negative down. The step adapts after it, so it is a suggestion. A number other than 0, from DSMIN to DSMAX '
      + 'in size.'},
  {key: 'dsmin', name: 'Smallest step (DSMIN)',
    help: 'The smallest step. When a point does not converge the step is halved and tried again; below this the '
      + 'branch ends with MX (no convergence). A number above 0, at most DSMAX.'},
  {key: 'dsmax', name: 'Largest step (DSMAX)',
    help: 'The largest step. Too large a step can jump over folds and Hopf points; too small a one makes a long run. '
      + 'A number above 0, at least DSMIN.'},
  {key: 'rl0', name: 'Par Min (RL0)',
    help: 'The lowest value of the main parameter: a branch that goes below it ends (EP, "parameter reached Par Min"). '
      + 'A number below Par Max.'},
  {key: 'rl1', name: 'Par Max (RL1)',
    help: 'The highest value of the main parameter: a branch that goes above it ends (EP, "parameter reached Par Max"). '
      + 'A number above Par Min.'},
  {key: 'a0', name: 'Norm Min (A0)',
    help: 'The lowest norm of the solution (its L2 norm, what the Norm axes plot): a branch whose norm goes below it '
      + 'ends (EP). A number below Norm Max.'},
  {key: 'a1', name: 'Norm Max (A1)',
    help: 'The highest norm of the solution: a branch whose norm goes above it ends (EP). A number above Norm Min.'},
  {key: 'epsl', name: 'Parameter tolerance (EPSL)',
    help: "Newton's convergence tolerance for the parameters, relative. Smaller is more accurate and fails sooner. "
      + 'A number above 0, often 1e-4 to 1e-7.'},
  {key: 'epsu', name: 'Solution tolerance (EPSU)',
    help: "Newton's convergence tolerance for the solution, relative. Smaller is more accurate and fails sooner. "
      + 'A number above 0, often 1e-4 to 1e-7.'},
  {key: 'epss', name: 'Special point tolerance (EPSS)',
    help: 'How closely special points (folds, Hopf and branch points, period doublings, tori) are located, relative '
      + 'to the step; usually 100 to 1000 times EPSL and EPSU. A number above 0.'},
  {key: 'iad', name: 'Adapt mesh every (IAD)',
    help: 'Adapt the mesh of a periodic orbit to its shape every IAD steps; 0 keeps the mesh fixed. 3 is usual. '
      + 'A whole number, 0 or more.'},
  {key: 'mxbf', name: 'Branch switches (MXBF)',
    help: 'For steady states: at how many branch points AUTO follows the other branch by itself. Positive: in both '
      + 'directions; negative: in one direction only; 0: none. A whole number.'},
  {key: 'iid', name: 'Output detail (IID)',
    help: 'How much AUTO writes to its diagnostics (the .d file): 0 almost nothing, 1 a little, 2 the usual, '
      + '3 also the Jacobian and residuals of the start, 4 and 5 very much (for debugging). A whole number from 0 to 5.'},
  {key: 'itmx', name: 'Locate iterations (ITMX)',
    help: 'The most iterations spent locating a special point (a fold, a Hopf or branch point ...). '
      + 'A whole number, at least 1.'},
  {key: 'itnw', name: 'Newton iterations (ITNW)',
    help: 'The most Newton iterations for a point. When they do not converge the step is halved (with IADS above 0) '
      + 'or the branch ends with MX. A whole number, at least 1.'},
  {key: 'nwtn', name: 'Full Newton steps (NWTN)',
    help: 'After this many Newton iterations the Jacobian is kept (the chord method), which is cheaper. '
      + 'A whole number, at least 1.'},
  {key: 'iads', name: 'Adapt step every (IADS)',
    help: 'Adapt the step size every IADS steps. 0 keeps it at DS, and then a point that does not converge ends the '
      + 'branch with MX. 1 is usual. A whole number, 0 or more.'},
  {key: 'suppbp', name: 'Skip branch points (SuppBP)',
    help: '1: do not look for branch points (faster, and no false ones); for periodic orbits there are then no '
      + 'Floquet multipliers, period doublings or tori either. 0: look for them. 0 or 1.'},
];

/** the Numerics dialog's groups, as the core's form has its columns */
export const NUM_GROUPS: {title: string; keys: NumKey[]}[] = [
  {title: 'Mesh and steps', keys: ['ntst', 'nmx', 'npr', 'ncol', 'ds', 'dsmin', 'dsmax']},
  {title: 'Limits and tolerances', keys: ['rl0', 'rl1', 'a0', 'a1', 'epsl', 'epsu', 'epss']},
  {title: 'Solver', keys: ['iad', 'mxbf', 'iid', 'itmx', 'itnw', 'nwtn', 'iads', 'suppbp']},
];

export const fieldOf = (key: NumKey): NumField => NUM_FIELDS.find(f => f.key === key)!;

export interface AutoAxesSettings {
  /** 0 hi, 1 norm, 2 hi and lo, 3 period, 4 two parameters, 10 frequency, 11 average */
  plot: number;
  var: string | null;
  par1: string | null;
  par2: string | null;
  xmin: number; xmax: number; ymin: number; ymax: number;
}

/** the `autosettings` event's settings, with what the Numerics take */
export interface AutoSettings {
  numerics: Record<NumKey, number>;
  pars: (string | null)[];
  axes: AutoAxesSettings;
  marks: [string, number][];
  rules: Record<NumKey, NumRule>;
  pairs: NumPair[];
  step: NumStep;
}

/** what an `auto` `set` changes (docs/protocol.md), each part only when given */
export interface AutoSettingsPatch {
  numerics?: Partial<Record<NumKey, number>>;
  pars?: string[];
  /** `view`: the view of the diagram whose axes these are (W50; the active one without it) */
  axes?: Partial<AutoAxesSettings> & {fit?: boolean; view?: number};
  marks?: [string, number][];
}

/** a set sent, until its own idle: `ahead` other commands' idles come first */
export interface AutoSettingsInFlight {
  patch: AutoSettingsPatch;
  ahead: number;
}

export interface AutoSettingsState {
  /** the core's, from the last event */
  core: AutoSettings | null;
  /** the sets sent whose command has not ended, in the order sent */
  inflight: AutoSettingsInFlight[];
  /** why the core refused the last set */
  error: string | null;
}

export const initialAutoSettings: AutoSettingsState = {core: null, inflight: [], error: null};

export type AutoSettingsAction =
  | {type: 'event'; ev: AutoSettings}
  /** `patch` went out as one set, `ahead` idles before its own */
  | {type: 'sent'; patch: AutoSettingsPatch; ahead: number}
  /** a command ended: the set whose command it was has been applied or refused */
  | {type: 'idle'}
  /** a new connection: nothing sent is waited for any more */
  | {type: 'reset'}
  | {type: 'error'; text: string};

export function reduceAutoSettings(s: AutoSettingsState, a: AutoSettingsAction): AutoSettingsState {
  switch (a.type) {
    case 'event':
      return {...s, core: a.ev};
    case 'sent':
      return {...s, inflight: [...s.inflight, {patch: a.patch, ahead: a.ahead}], error: null};
    case 'idle':
      return s.inflight.length
        ? {...s, inflight: s.inflight.filter(f => f.ahead > 0).map(f => ({...f, ahead: f.ahead - 1}))} : s;
    case 'reset':
      return s.inflight.length ? {...s, inflight: []} : s;
    case 'error':
      return s.inflight.some(f => f.ahead === 0) ? {...s, error: a.text} : s;
    default:
      return s;
  }
}

function apply(base: AutoSettings, p: AutoSettingsPatch | null): AutoSettings {
  if (!p) return base;
  const pars = p.pars ? base.pars.map((n, i) => (p.pars![i] ? p.pars![i] : n)) : base.pars;
  const {fit: _fit, view: _view, ...axes} = p.axes ?? {};
  return {
    ...base,
    numerics: {...base.numerics, ...p.numerics},
    pars,
    axes: {...base.axes, ...axes},
    marks: p.marks ?? base.marks,
  };
}

/** what the forms show: the core's settings with the edits sent and not yet applied over them */
export function shownSettings(s: AutoSettingsState): AutoSettings | null {
  return s.core ? s.inflight.reduce((base, f) => apply(base, f.patch), s.core) : null;
}

/** the fields an edit sent is not yet applied to ("numerics.nmx", "axes.plot", "pars", "marks") */
export function pendingFields(s: AutoSettingsState): Set<string> {
  const out = new Set<string>();
  for (const {patch: p} of s.inflight) {
    for (const k of Object.keys(p.numerics ?? {})) out.add(`numerics.${k}`);
    for (const k of Object.keys(p.axes ?? {})) if (k !== 'fit' && k !== 'view') out.add(`axes.${k}`);
    if (p.pars) out.add('pars');
    if (p.marks) out.add('marks');
  }
  return out;
}

export function setCommand(p: AutoSettingsPatch): Command {
  return {cmd: 'auto', op: 'set', ...p};
}

/** what a Numerics field takes (store/fieldKinds.ts): a whole number or a number, and its rule */
export function numSpec(r: NumRule): FieldSpec {
  if (r.integer) return {kind: 'integer', min: r.min, max: r.max};
  return {kind: 'number', min: r.min, max: r.max, positive: !!r.positive, nonzero: !!r.nonzero};
}

/** the error a Numerics field's text has: the core's message for its rule, or null */
export function numError(r: NumRule, text: string): string | null {
  return fieldError(numSpec(r), text) ? r.message : null;
}

/** the values that must agree with each other, as the core checks them, by the field whose message it is */
export function pairErrors(s: Pick<AutoSettings, 'pairs' | 'step'>, v: Record<NumKey, number>): Partial<Record<NumKey, string>> {
  const out: Partial<Record<NumKey, string>> = {};
  for (const p of s.pairs) if (!(p.strict ? v[p.lo] < v[p.hi] : v[p.lo] <= v[p.hi])) out[p.lo] = p.message;
  const {key, lo, hi, message} = s.step;
  if (!out[lo] && !out[hi] && !(Math.abs(v[key]) >= v[lo] && Math.abs(v[key]) <= v[hi])) out[key] = message;
  return out;
}

/** the first of pairErrors, or null */
export function pairError(s: Pick<AutoSettings, 'pairs' | 'step'>, v: Record<NumKey, number>): string | null {
  return Object.values(pairErrors(s, v))[0] ?? null;
}
