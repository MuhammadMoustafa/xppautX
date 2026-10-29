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
import {choiceLabel, fieldError, type FieldSpec} from './fieldKinds';
import type {AutoProjection} from './diagram';

/** the Numerics fields in the core's order (core/auto_settings.cpp num_fields): named for what they set,
    AUTO's own names (NTST, DS, MXBF ...) are a hover note only */
export const NUM_KEYS = ['mesh_intervals', 'max_steps', 'output_every', 'collocation_points', 'step_size', 'min_step_size',
  'max_step_size', 'parameter_min', 'parameter_max', 'norm_min', 'norm_max', 'parameter_tolerance', 'state_tolerance',
  'special_point_tolerance', 'mesh_adapt_every', 'max_branch_switches', 'switch_one_direction', 'output_detail',
  'max_location_iterations', 'max_newton_iterations', 'full_newton_after', 'step_adapt_every', 'suppress_branch_points'] as const;
export type NumKey = typeof NUM_KEYS[number];
/** the fields that are true or false, and the one that is one of named levels */
export type BooleanKey = 'switch_one_direction' | 'suppress_branch_points';
export type ChoiceKey = 'output_detail';
type NumericKey = Exclude<NumKey, BooleanKey | ChoiceKey>;

/** what a Numerics value takes (the event's `rules`: core/auto_settings.cpp num_fields), and the
    message the core refuses another with; each flag only when it holds */
export interface NumRule {
  /** the core's form label */
  label: string;
  integer?: boolean;
  boolean?: boolean;
  /** the names of a choice, in the order of AUTO's levels */
  choices?: string[];
  positive?: boolean;
  nonzero?: boolean;
  min?: number;
  max?: number;
  message: string;
}

/** two values that must be in order: lo below hi (strict) or at most hi */
export interface NumPair {
  lo: NumericKey;
  hi: NumericKey;
  strict: boolean;
  message: string;
}

/** the first step `key` within the step sizes `lo` to `hi`, whichever its sign */
export interface NumStep {
  key: NumericKey;
  lo: NumericKey;
  hi: NumericKey;
  message: string;
}

export interface NumField {
  key: NumKey;
  /** AUTO's own name for it: shown in the hover, a search word, never typed */
  auto: string;
  /** its tooltip: what it does, its valid values and what each option means (AUTO's manual) */
  help: string;
}

/** the field's name in the page: its key, underscores as spaces and a capital first letter (W381) */
export const fieldName = (f: NumField): string => choiceLabel(f.key);

/** the Numerics form's fields in its order (core/auto_settings.cpp num_fields) */
export const NUM_FIELDS: NumField[] = [
  {key: 'mesh_intervals', auto: 'NTST',
    help: 'The mesh intervals a periodic orbit (or a boundary value solution) is split into. More follow a sharp orbit '
      + 'better but take longer: raise it when a periodic branch looks wrong or does not converge. Following a period '
      + 'doubling doubles it, so set it back after. A whole number, at least 1.'},
  {key: 'max_steps', auto: 'NMX',
    help: 'The most steps a branch may take: it ends (EP, "reached max_steps") when it has this many points. '
      + 'A whole number, at least 1.'},
  {key: 'output_every', auto: 'NPR',
    help: 'Besides the special points, label and save the whole solution every this many points along a branch, so it can be '
      + 'grabbed. A whole number, at least 1.'},
  {key: 'collocation_points', auto: 'NCOL',
    help: 'The collocation points in each mesh interval of a periodic orbit or boundary value solution. '
      + 'A whole number from 2 to 7; 4 is usual.'},
  {key: 'step_size', auto: 'DS',
    help: 'The first step along the branch. Its sign is the direction: positive makes the main parameter go up, '
      + 'negative down. The step adapts after it, so it is a suggestion. A number other than 0, from the min to the max '
      + 'step size in size.'},
  {key: 'min_step_size', auto: 'DSMIN',
    help: 'The smallest step. When a point does not converge the step is halved and tried again; below this the '
      + 'branch ends with MX (no convergence). A number above 0, at most the max step size.'},
  {key: 'max_step_size', auto: 'DSMAX',
    help: 'The largest step. Too large a step can jump over folds and Hopf points; too small a one makes a long run. '
      + 'A number above 0, at least the min step size.'},
  {key: 'parameter_min', auto: 'RL0',
    help: 'The lowest value of the main parameter: a branch that goes below it ends (EP, "reached parameter_min"). '
      + 'A number below the parameter max.'},
  {key: 'parameter_max', auto: 'RL1',
    help: 'The highest value of the main parameter: a branch that goes above it ends (EP, "reached parameter_max"). '
      + 'A number above the parameter min.'},
  {key: 'norm_min', auto: 'A0',
    help: 'The lowest norm of the solution (its L2 norm, what the Norm axes plot): a branch whose norm goes below it '
      + 'ends (EP). A number below the norm max.'},
  {key: 'norm_max', auto: 'A1',
    help: 'The highest norm of the solution: a branch whose norm goes above it ends (EP). A number above the norm min.'},
  {key: 'parameter_tolerance', auto: 'EPSL',
    help: "Newton's convergence tolerance for the parameters, relative. Smaller is more accurate and fails sooner. "
      + 'A number above 0, often 1e-4 to 1e-7.'},
  {key: 'state_tolerance', auto: 'EPSU',
    help: "Newton's convergence tolerance for the solution, relative. Smaller is more accurate and fails sooner. "
      + 'A number above 0, often 1e-4 to 1e-7.'},
  {key: 'special_point_tolerance', auto: 'EPSS',
    help: 'How closely special points (folds, Hopf and branch points, period doublings, tori) are located, relative '
      + 'to the step; usually 100 to 1000 times the parameter and state tolerances. A number above 0.'},
  {key: 'mesh_adapt_every', auto: 'IAD',
    help: 'Adapt the mesh of a periodic orbit to its shape every this many steps; 0 keeps the mesh fixed. 3 is usual. '
      + 'A whole number, 0 or more.'},
  {key: 'max_branch_switches', auto: 'MXBF',
    help: 'For steady states: at how many branch points AUTO follows the other branch by itself; 0: none. '
      + 'A whole number, 0 or more.'},
  {key: 'switch_one_direction', auto: 'MXBF below 0',
    help: 'When checked, the branches AUTO switches to by itself are followed in one direction only; unchecked: in both.'},
  {key: 'output_detail', auto: 'IID',
    help: 'How much AUTO writes to its diagnostics (the .d file): none, little, normal (the usual), start jacobian '
      + '(also the Jacobian and residuals of the start), extensive, debugging.'},
  {key: 'max_location_iterations', auto: 'ITMX',
    help: 'The most iterations spent locating a special point (a fold, a Hopf or branch point ...). '
      + 'A whole number, at least 1.'},
  {key: 'max_newton_iterations', auto: 'ITNW',
    help: 'The most Newton iterations for a point. When they do not converge the step is halved (with step adapt every '
      + 'above 0) or the branch ends with MX. A whole number, at least 1.'},
  {key: 'full_newton_after', auto: 'NWTN',
    help: 'After this many Newton iterations the Jacobian is kept (the chord method), which is cheaper. '
      + 'A whole number, at least 1.'},
  {key: 'step_adapt_every', auto: 'IADS',
    help: 'Adapt the step size every this many steps. 0 keeps it at the first step, and then a point that does not '
      + 'converge ends the branch with MX. 1 is usual. A whole number, 0 or more.'},
  {key: 'suppress_branch_points', auto: 'SuppBP',
    help: 'When checked, do not look for branch points (faster, and no false ones); for periodic orbits there are then no '
      + 'Floquet multipliers, period doublings or tori either. Unchecked: look for them.'},
];

/** the Numerics dialog's groups, as the core's form has its columns */
export const NUM_GROUPS: {title: string; keys: NumKey[]}[] = [
  {title: 'Mesh and steps', keys: ['mesh_intervals', 'max_steps', 'output_every', 'collocation_points', 'step_size', 'min_step_size', 'max_step_size']},
  {title: 'Limits and tolerances', keys: ['parameter_min', 'parameter_max', 'norm_min', 'norm_max', 'parameter_tolerance', 'state_tolerance', 'special_point_tolerance']},
  {title: 'Solver', keys: ['mesh_adapt_every', 'max_branch_switches', 'switch_one_direction', 'output_detail', 'max_location_iterations', 'max_newton_iterations', 'full_newton_after', 'step_adapt_every', 'suppress_branch_points']},
];

export const fieldOf = (key: NumKey): NumField => NUM_FIELDS.find(f => f.key === key)!;

export interface AutoAxesSettings {
  plot: AutoProjection;
  var: string | null;
  par1: string | null;
  par2: string | null;
  xmin: number; xmax: number; ymin: number; ymax: number;
}

/** the `autosettings` event's settings, with what the Numerics take */
export const CODIM_KINDS = ['bogdanov_takens', 'cusp', 'zero_hopf', 'generalized_hopf', 'resonance_1_1', 'resonance_1_2', 'resonance_1_3', 'resonance_1_4',
  'fold_flip', 'fold_neimark_sacker', 'flip_neimark_sacker', 'double_neimark_sacker'] as const;
export type CodimKind = typeof CODIM_KINDS[number];
/** every kind set to one value */
export const allCodim = (on: boolean) => Object.fromEntries(CODIM_KINDS.map(k => [k, on])) as Record<CodimKind, boolean>;

export interface AutoSettings {
  special_points: Record<CodimKind, boolean>;
  numerics: Record<NumericKey, number> & Record<BooleanKey, boolean> & Record<ChoiceKey, string>;
  axes: AutoAxesSettings;
  marks: [string, number][];
  /** AUTO's thread count, a program preference (W267): "auto" or the count */
  threads: 'auto' | number;
  /** the count "auto" gives now, and the most a count may be (the computer's logical processors) */
  threads_auto: number;
  threads_max: number;
  rules: Record<NumKey, NumRule>;
  pairs: NumPair[];
  step: NumStep;
}

/** what an `auto` `set` changes (docs/protocol.md), each part only when given */
export interface AutoSettingsPatch {
  special_points?: Partial<Record<CodimKind, boolean>>;
  numerics?: Partial<AutoSettings['numerics']>;
  /** `view`: the view of the diagram whose axes these are (W50; the active one without it) */
  axes?: Partial<AutoAxesSettings> & {fit?: boolean; view?: string};
  marks?: [string, number][];
  threads?: 'auto' | number;
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
  const {fit: _fit, view: _view, ...axes} = p.axes ?? {};
  return {
    ...base,
    special_points: {...base.special_points, ...p.special_points},
    numerics: {...base.numerics, ...p.numerics},
    axes: {...base.axes, ...axes},
    marks: p.marks ?? base.marks,
    threads: p.threads ?? base.threads,
  };
}

/** what the forms show: the core's settings with the edits sent and not yet applied over them */
export function shownSettings(s: AutoSettingsState): AutoSettings | null {
  return s.core ? s.inflight.reduce((base, f) => apply(base, f.patch), s.core) : null;
}

/** the fields an edit sent is not yet applied to ("numerics.nmx", "axes.plot", "marks") */
export function pendingFields(s: AutoSettingsState): Set<string> {
  const out = new Set<string>();
  for (const {patch: p} of s.inflight) {
    for (const k of Object.keys(p.numerics ?? {})) out.add(`numerics.${k}`);
    for (const k of Object.keys(p.axes ?? {})) if (k !== 'fit' && k !== 'view') out.add(`axes.${k}`);
    if (p.special_points) for (const k of Object.keys(p.special_points)) out.add(`special_points.${k}`);
    if (p.marks) out.add('marks');
    if (p.threads !== undefined) out.add('threads');
  }
  return out;
}

/** what a patch is as registry commands (docs/protocol.md auto.configure_numerics, ...): one for each part it has, in the order of its parts */
export function autoSettingsCommands(p: AutoSettingsPatch): {command: Command; patch: AutoSettingsPatch}[] {
  const out: {command: Command; patch: AutoSettingsPatch}[] = [];
  if (p.numerics || p.special_points) {
    out.push({
      command: {cmd: 'auto.configure_numerics', ...p.numerics, ...(p.special_points ? {special_points: p.special_points} : {})},
      patch: {numerics: p.numerics, special_points: p.special_points},
    });
  }
  if (p.axes) {
    const {plot, var: variable, par1, par2, xmin, xmax, ymin, ymax, fit, view} = p.axes;
    const given = {plot_type: plot, variable, parameter1: par1, parameter2: par2, x_min: xmin, x_max: xmax, y_min: ymin, y_max: ymax, fit, view};
    out.push({
      command: {cmd: 'auto.configure_plot', ...Object.fromEntries(Object.entries(given).filter(([, v]) => v !== undefined && v !== null))},
      patch: {axes: p.axes},
    });
  }
  if (p.marks) {
    out.push({command: {cmd: 'auto.configure_marks', parameters: p.marks.map(m => m[0]), values: p.marks.map(m => m[1])}, patch: {marks: p.marks}});
  }
  if (p.threads !== undefined) {
    out.push({command: {cmd: 'auto.set_threads', ...(p.threads === 'auto' ? {automatic: true} : {threads: p.threads})}, patch: {threads: p.threads}});
  }
  return out;
}

/** what a Numerics field takes (store/fieldKinds.ts): a whole number or a number, and its rule */
export function numSpec(r: NumRule): FieldSpec {
  if (r.boolean) return {kind: 'boolean'};
  if (r.choices) return {kind: 'choice', choices: r.choices};
  if (r.integer) return {kind: 'integer', min: r.min, max: r.max};
  return {kind: 'number', min: r.min, max: r.max, positive: !!r.positive, nonzero: !!r.nonzero};
}

/** the error a Numerics field's text has: the core's message for its rule, or null */
export function numError(r: NumRule, text: string): string | null {
  return fieldError(numSpec(r), text) ? r.message : null;
}

/** the values that must agree with each other, as the core checks them, by the field whose message it is */
export function pairErrors(s: Pick<AutoSettings, 'pairs' | 'step'>, v: Record<NumKey, number | boolean | string>): Partial<Record<NumKey, string>> {
  const out: Partial<Record<NumKey, string>> = {};
  const n = (k: NumericKey): number => Number(v[k]);
  for (const p of s.pairs) if (!(p.strict ? n(p.lo) < n(p.hi) : n(p.lo) <= n(p.hi))) out[p.lo] = p.message;
  const {key, lo, hi, message} = s.step;
  if (!out[lo] && !out[hi] && !(Math.abs(n(key)) >= n(lo) && Math.abs(n(key)) <= n(hi))) out[key] = message;
  return out;
}

/** the first of pairErrors, or null */
export function pairError(s: Pick<AutoSettings, 'pairs' | 'step'>, v: Record<NumKey, number | boolean | string>): string | null {
  return Object.values(pairErrors(s, v))[0] ?? null;
}


/** Track settings from the common command form and specialised plot gestures alike. */
export function commandSettings(command: Command): AutoSettingsPatch | null {
  const {cmd, ...values} = command;
  if (cmd === 'auto.configure_numerics') {
    const {special_points, ...numerics} = values;
    return {numerics: numerics as AutoSettingsPatch['numerics'], special_points: special_points as AutoSettingsPatch['special_points']};
  }
  if (cmd === 'auto.configure_plot') {
    const names: Record<string,string> = {plot_type:'plot', variable:'var', parameter1:'par1', parameter2:'par2', x_min:'xmin', x_max:'xmax', y_min:'ymin', y_max:'ymax', fit:'fit', view:'view'};
    return {axes: Object.fromEntries(Object.entries(values).filter(([key]) => key in names).map(([key,value]) => [names[key],value]))};
  }
  if (cmd === 'auto.configure_marks') return {marks: (command.parameters as string[] ?? []).map((name,i) => [name, (command.values as number[])[i]])};
  if (cmd === 'auto.set_threads') return {threads: command.automatic === true ? 'auto' : command.threads as number};
  return null;
}
