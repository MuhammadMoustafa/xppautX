/* AUTO's settings as data (docs/ui-v2.md T22, docs/protocol.md "AUTO's
   settings as data"): the Numerics, AUTO's parameters, the axes and the
   Mark values, from the `autosettings` event, edited in the page's own
   forms at any time. An edit made while the core computes waits here
   (`queued`, shown as pending) and goes out in one `auto` `set` when the
   command ends; one sent is `sent` until that set's idle, when the event
   has brought the core's values (or a `message` `error` said why not).
   Also the settings file (Save/Load settings, T21), now written from and
   read into these data. Pure: no DOM, no I/O. */
import type {Command} from '../protocol/types';
import {fieldError, type FieldSpec} from './fieldKinds';

export type NumKey = 'ntst' | 'nmx' | 'npr' | 'ncol' | 'ds' | 'dsmin' | 'dsmax' | 'rl0' | 'rl1' | 'a0' | 'a1'
  | 'epsl' | 'epsu' | 'epss' | 'iad' | 'mxbf' | 'iid' | 'itmx' | 'itnw' | 'nwtn' | 'iads' | 'suppbp';

type Rule = {kind: 'any'} | {kind: 'positive'} | {kind: 'nonzero'} | {kind: 'range'; lo: number; hi: number | null};

export interface NumField {
  key: NumKey;
  /** the core's form label (and the settings file's name) */
  label: string;
  /** the field's name in the page: plain words and AUTO's short name (T23), "Max points (NMX)" */
  name: string;
  integer: boolean;
  rule: Rule;
  /** its tooltip: what it does, its valid values and what each option means (AUTO's manual) */
  help: string;
}

const atLeast = (lo: number): Rule => ({kind: 'range', lo, hi: null});

/** the Numerics form's fields in its order (core/auto_settings.cpp num_fields, the same rules) */
export const NUM_FIELDS: NumField[] = [
  {key: 'ntst', label: 'Ntst', name: 'Mesh intervals (NTST)', integer: true, rule: atLeast(1),
    help: 'The mesh intervals a periodic orbit (or a boundary value solution) is split into. More follow a sharp orbit '
      + 'better but take longer: raise it when a periodic branch looks wrong or does not converge. Following a period '
      + 'doubling doubles it, so set it back after. A whole number, at least 1.'},
  {key: 'nmx', label: 'Nmax', name: 'Max points (NMX)', integer: true, rule: atLeast(1),
    help: 'The most points a branch may have: it ends (EP, "reached Max points") when it has this many. '
      + 'A whole number, at least 1.'},
  {key: 'npr', label: 'NPr', name: 'Label every (NPR)', integer: true, rule: atLeast(1),
    help: 'Besides the special points, label and save the whole solution every NPR points along a branch, so it can be '
      + 'grabbed. A whole number, at least 1.'},
  {key: 'ncol', label: 'Ncol', name: 'Collocation points (NCOL)', integer: true, rule: {kind: 'range', lo: 2, hi: 7},
    help: 'The collocation points in each mesh interval of a periodic orbit or boundary value solution. '
      + 'A whole number from 2 to 7; 4 is usual.'},
  {key: 'ds', label: 'Ds', name: 'First step (DS)', integer: false, rule: {kind: 'nonzero'},
    help: 'The first step along the branch. Its sign is the direction: positive makes the main parameter go up, '
      + 'negative down. The step adapts after it, so it is a suggestion. A number other than 0, from DSMIN to DSMAX '
      + 'in size.'},
  {key: 'dsmin', label: 'Dsmin', name: 'Smallest step (DSMIN)', integer: false, rule: {kind: 'positive'},
    help: 'The smallest step. When a point does not converge the step is halved and tried again; below this the '
      + 'branch ends with MX (no convergence). A number above 0, at most DSMAX.'},
  {key: 'dsmax', label: 'Dsmax', name: 'Largest step (DSMAX)', integer: false, rule: {kind: 'positive'},
    help: 'The largest step. Too large a step can jump over folds and Hopf points; too small a one makes a long run. '
      + 'A number above 0, at least DSMIN.'},
  {key: 'rl0', label: 'Par Min', name: 'Par Min (RL0)', integer: false, rule: {kind: 'any'},
    help: 'The lowest value of the main parameter: a branch that goes below it ends (EP, "parameter reached Par Min"). '
      + 'A number below Par Max.'},
  {key: 'rl1', label: 'Par Max', name: 'Par Max (RL1)', integer: false, rule: {kind: 'any'},
    help: 'The highest value of the main parameter: a branch that goes above it ends (EP, "parameter reached Par Max"). '
      + 'A number above Par Min.'},
  {key: 'a0', label: 'Norm Min', name: 'Norm Min (A0)', integer: false, rule: {kind: 'any'},
    help: 'The lowest norm of the solution (its L2 norm, what the Norm axes plot): a branch whose norm goes below it '
      + 'ends (EP). A number below Norm Max.'},
  {key: 'a1', label: 'Norm Max', name: 'Norm Max (A1)', integer: false, rule: {kind: 'any'},
    help: 'The highest norm of the solution: a branch whose norm goes above it ends (EP). A number above Norm Min.'},
  {key: 'epsl', label: 'EPSL', name: 'Parameter tolerance (EPSL)', integer: false, rule: {kind: 'positive'},
    help: "Newton's convergence tolerance for the parameters, relative. Smaller is more accurate and fails sooner. "
      + 'A number above 0, often 1e-4 to 1e-7.'},
  {key: 'epsu', label: 'EPSU', name: 'Solution tolerance (EPSU)', integer: false, rule: {kind: 'positive'},
    help: "Newton's convergence tolerance for the solution, relative. Smaller is more accurate and fails sooner. "
      + 'A number above 0, often 1e-4 to 1e-7.'},
  {key: 'epss', label: 'EPSS', name: 'Special point tolerance (EPSS)', integer: false, rule: {kind: 'positive'},
    help: 'How closely special points (folds, Hopf and branch points, period doublings, tori) are located, relative '
      + 'to the step; usually 100 to 1000 times EPSL and EPSU. A number above 0.'},
  {key: 'iad', label: 'IAD', name: 'Adapt mesh every (IAD)', integer: true, rule: atLeast(0),
    help: 'Adapt the mesh of a periodic orbit to its shape every IAD steps; 0 keeps the mesh fixed. 3 is usual. '
      + 'A whole number, 0 or more.'},
  {key: 'mxbf', label: 'MXBF', name: 'Branch switches (MXBF)', integer: true, rule: {kind: 'any'},
    help: 'For steady states: at how many branch points AUTO follows the other branch by itself. Positive: in both '
      + 'directions; negative: in one direction only; 0: none. A whole number.'},
  {key: 'iid', label: 'IID', name: 'Output detail (IID)', integer: true, rule: {kind: 'range', lo: 0, hi: 5},
    help: 'How much AUTO writes to its diagnostics (the .d file): 0 almost nothing, 1 a little, 2 the usual, '
      + '3 also the Jacobian and residuals of the start, 4 and 5 very much (for debugging). A whole number from 0 to 5.'},
  {key: 'itmx', label: 'ITMX', name: 'Locate iterations (ITMX)', integer: true, rule: atLeast(1),
    help: 'The most iterations spent locating a special point (a fold, a Hopf or branch point ...). '
      + 'A whole number, at least 1.'},
  {key: 'itnw', label: 'ITNW', name: 'Newton iterations (ITNW)', integer: true, rule: atLeast(1),
    help: 'The most Newton iterations for a point. When they do not converge the step is halved (with IADS above 0) '
      + 'or the branch ends with MX. A whole number, at least 1.'},
  {key: 'nwtn', label: 'NWTN', name: 'Full Newton steps (NWTN)', integer: true, rule: atLeast(1),
    help: 'After this many Newton iterations the Jacobian is kept (the chord method), which is cheaper. '
      + 'A whole number, at least 1.'},
  {key: 'iads', label: 'IADS', name: 'Adapt step every (IADS)', integer: true, rule: atLeast(0),
    help: 'Adapt the step size every IADS steps. 0 keeps it at DS, and then a point that does not converge ends the '
      + 'branch with MX. 1 is usual. A whole number, 0 or more.'},
  {key: 'suppbp', label: 'SuppBP', name: 'Skip branch points (SuppBP)', integer: true, rule: {kind: 'range', lo: 0, hi: 1},
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

/** the `autosettings` event's settings */
export interface AutoSettings {
  numerics: Record<NumKey, number>;
  pars: (string | null)[];
  axes: AutoAxesSettings;
  marks: [string, number][];
}

/** what an `auto` `set` changes (docs/protocol.md), each part only when given */
export interface AutoSettingsPatch {
  numerics?: Partial<Record<NumKey, number>>;
  pars?: string[];
  axes?: Partial<AutoAxesSettings> & {fit?: boolean};
  marks?: [string, number][];
}

export interface AutoSettingsState {
  /** the core's, from the last event */
  core: AutoSettings | null;
  /** edits made while the core computed, sent when it is done */
  queued: AutoSettingsPatch | null;
  /** the set sent, until its idle */
  sent: AutoSettingsPatch | null;
  /** why the core refused the last set */
  error: string | null;
}

export const initialAutoSettings: AutoSettingsState = {core: null, queued: null, sent: null, error: null};

export type AutoSettingsAction =
  | {type: 'event'; ev: AutoSettings}
  | {type: 'queue'; patch: AutoSettingsPatch}
  /** the queued edits (or `patch`) went out in one set */
  | {type: 'sent'; patch: AutoSettingsPatch}
  /** a command ended: the set sent has been applied or refused */
  | {type: 'settled'}
  | {type: 'error'; text: string};

/** b over a: the later edit of a field wins, the lists whole */
export function mergePatch(a: AutoSettingsPatch | null, b: AutoSettingsPatch): AutoSettingsPatch {
  if (!a) return b;
  const out: AutoSettingsPatch = {...a};
  if (b.numerics) out.numerics = {...a.numerics, ...b.numerics};
  if (b.pars) out.pars = b.pars;
  if (b.axes) out.axes = {...a.axes, ...b.axes, fit: !!(a.axes?.fit || b.axes.fit)};
  if (b.marks) out.marks = b.marks;
  return out;
}

export function reduceAutoSettings(s: AutoSettingsState, a: AutoSettingsAction): AutoSettingsState {
  switch (a.type) {
    case 'event':
      return {...s, core: a.ev};
    case 'queue':
      return {...s, queued: mergePatch(s.queued, a.patch), error: null};
    case 'sent':
      return {...s, sent: a.patch, queued: null, error: null};
    case 'settled':
      return s.sent ? {...s, sent: null} : s;
    case 'error':
      return s.sent ? {...s, error: a.text} : s;
    default:
      return s;
  }
}

function apply(base: AutoSettings, p: AutoSettingsPatch | null): AutoSettings {
  if (!p) return base;
  const pars = p.pars ? base.pars.map((n, i) => (p.pars![i] ? p.pars![i] : n)) : base.pars;
  const {fit: _fit, ...axes} = p.axes ?? {};
  return {
    numerics: {...base.numerics, ...p.numerics},
    pars,
    axes: {...base.axes, ...axes},
    marks: p.marks ?? base.marks,
  };
}

/** what the forms show: the core's settings with the edits not yet applied over them */
export function shownSettings(s: AutoSettingsState): AutoSettings | null {
  return s.core ? apply(apply(s.core, s.sent), s.queued) : null;
}

/** the fields an edit waits on ("numerics.nmx", "axes.plot", "pars", "marks") */
export function pendingFields(s: AutoSettingsState): Set<string> {
  const out = new Set<string>();
  for (const p of [s.sent, s.queued]) {
    if (!p) continue;
    for (const k of Object.keys(p.numerics ?? {})) out.add(`numerics.${k}`);
    for (const k of Object.keys(p.axes ?? {})) if (k !== 'fit') out.add(`axes.${k}`);
    if (p.pars) out.add('pars');
    if (p.marks) out.add('marks');
  }
  return out;
}

export function setCommand(p: AutoSettingsPatch): Command {
  return {cmd: 'auto', op: 'set', ...p};
}

/** what a Numerics field takes (store/fieldKinds.ts): a whole number or a number, and its rule */
export function numSpec(key: NumKey): FieldSpec {
  const f = fieldOf(key), r = f.rule;
  const min = r.kind === 'range' ? r.lo : undefined, max = r.kind === 'range' && r.hi !== null ? r.hi : undefined;
  if (f.integer) return {kind: 'integer', min, max};
  return {kind: 'number', min, max, positive: r.kind === 'positive', nonzero: r.kind === 'nonzero'};
}

/** the error a Numerics field's text has, as the core would refuse it, or null (named as the page names the field) */
export function numError(key: NumKey, text: string): string | null {
  const f = fieldOf(key), phrase = fieldError(numSpec(key), text);
  if (!phrase) return null;
  const t = text.trim();
  if (f.integer && phrase === 'a whole number' && t && Number.isFinite(Number(t))) return `${f.name} must be a whole number, not ${t}`;
  return `${f.name} must be ${phrase}`;
}

/** the values that must agree with each other, as the core checks them, by the field whose message it is */
export function pairErrors(v: Record<NumKey, number>): Partial<Record<NumKey, string>> {
  const out: Partial<Record<NumKey, string>> = {};
  if (v.dsmin > v.dsmax) out.dsmin = 'DSMIN must be at most DSMAX';
  else if (!(Math.abs(v.ds) >= v.dsmin && Math.abs(v.ds) <= v.dsmax)) out.ds = 'DS must be from DSMIN to DSMAX in size';
  if (!(v.rl0 < v.rl1)) out.rl0 = 'Par Min must be below Par Max';
  if (!(v.a0 < v.a1)) out.a0 = 'Norm Min must be below Norm Max';
  return out;
}

/** the first of pairErrors, or null */
export function pairError(v: Record<NumKey, number>): string | null {
  return Object.values(pairErrors(v))[0] ?? null;
}

/* ---- the settings file (T21's, version 2 adds AUTO's parameters and the Mark values) ----

     {"xppautX": "auto-settings", "version": 2,
      "numerics": {"Ntst": 15, "Nmax": 200, ...},
      "plot": 2, "axes": {"Y-axis": "V", "Main Parm": "iapp", "Secnd Parm": "phi", "Xmin": ..., ...},
      "pars": ["iapp", "phi", ...], "marks": [["iapp", 0.25], ["T", 30]]}

   Names are the core's form labels, case not mattering; a name the file
   lacks keeps the setting, a name it does not know is ignored. Values may
   be numbers or text (version 1 wrote the forms' text). */

/** Auto.plot -> its key in the Axes menu ("Plot Type": hI-lo is i, ...), the plot types there are */
export const PLOT_KEYS: Record<number, string> = {0: 'h', 1: 'n', 2: 'i', 3: 'p', 4: 't', 10: 'r', 11: 'a'};

const AXES_NAMES: [string, 'var' | 'par1' | 'par2' | 'xmin' | 'ymin' | 'xmax' | 'ymax'][] = [
  ['Y-axis', 'var'], ['Main Parm', 'par1'], ['Secnd Parm', 'par2'], ['Xmin', 'xmin'], ['Ymin', 'ymin'], ['Xmax', 'xmax'],
  ['Ymax', 'ymax'],
];

/** a form field's name without the core's list marker ("*1Y-axis" -> "Y-axis") */
export function plainName(name: string): string {
  return name.replace(/^\*\d/, '').trim();
}

export function formatSettings(s: AutoSettings): string {
  const axes: Record<string, string | number> = {};
  for (const [label, k] of AXES_NAMES) axes[label] = s.axes[k] ?? '';
  return `${JSON.stringify({
    xppautX: 'auto-settings', version: 2,
    numerics: Object.fromEntries(NUM_FIELDS.map(f => [f.label, s.numerics[f.key]])),
    plot: s.axes.plot,
    axes,
    pars: s.pars,
    marks: s.marks,
  }, null, 1)}\n`;
}

const isMap = (o: unknown): o is Record<string, unknown> => !!o && typeof o === 'object' && !Array.isArray(o);
const numberOf = (v: unknown): number | null => {
  if (typeof v === 'number') return Number.isFinite(v) ? v : null;
  if (typeof v === 'string' && v.trim()) {
    const n = Number(v);
    return Number.isFinite(n) ? n : null;
  }
  return null;
};

/** a settings file as the set that loads it, or what is wrong with it */
export function parseSettings(text: string): {patch: AutoSettingsPatch | null; error: string | null} {
  const bad = (why: string) => ({patch: null, error: `Not an AUTO settings file: ${why}.`});
  let o: Record<string, unknown>;
  try {
    o = JSON.parse(text);
  } catch {
    return bad('it is not JSON');
  }
  if (!isMap(o) || o.xppautX !== 'auto-settings') return bad('it lacks "xppautX": "auto-settings"');
  if ((o.numerics !== undefined && !isMap(o.numerics)) || (o.axes !== undefined && !isMap(o.axes)))
    return bad('"numerics" and "axes" must map names to values');
  const patch: AutoSettingsPatch = {};
  const byName = (m: Record<string, unknown>) => new Map(Object.entries(m).map(([k, v]) => [plainName(k).toLowerCase(), v]));
  if (isMap(o.numerics)) {
    const m = byName(o.numerics), numerics: Partial<Record<NumKey, number>> = {};
    for (const f of NUM_FIELDS) {
      if (!m.has(f.label.toLowerCase())) continue;
      const v = numberOf(m.get(f.label.toLowerCase()));
      if (v === null) return bad(`${f.label} is not a number`);
      numerics[f.key] = v;
    }
    if (Object.keys(numerics).length) patch.numerics = numerics;
  }
  const axes: AutoSettingsPatch['axes'] = {};
  if (o.plot !== undefined) {
    const plot = Number(o.plot);
    if (!(plot in PLOT_KEYS)) return bad(`"plot" ${String(o.plot)} is not one of AUTO's plot types`);
    axes.plot = plot;
  }
  if (isMap(o.axes)) {
    const m = byName(o.axes);
    for (const [label, k] of AXES_NAMES) {
      if (!m.has(label.toLowerCase())) continue;
      const v = m.get(label.toLowerCase());
      if (k === 'var' || k === 'par1' || k === 'par2') {
        if (typeof v !== 'string') return bad(`${label} is not a name`);
        if (v.trim()) axes[k] = v.trim();
      } else {
        const n = numberOf(v);
        if (n === null) return bad(`${label} is not a number`);
        axes[k] = n;
      }
    }
  }
  if (Object.keys(axes).length) patch.axes = axes;
  if (o.pars !== undefined) {
    if (!Array.isArray(o.pars) || o.pars.some(p => p !== null && typeof p !== 'string')) return bad('"pars" must be names');
    patch.pars = (o.pars as (string | null)[]).map(p => p ?? '');
  }
  if (o.marks !== undefined) {
    const ok = Array.isArray(o.marks) && o.marks.every(m => Array.isArray(m) && typeof m[0] === 'string' && numberOf(m[1]) !== null);
    if (!ok) return bad('"marks" must be [name, value] pairs');
    patch.marks = (o.marks as [string, unknown][]).map(([n, v]) => [n, numberOf(v)!]);
  }
  if (!Object.keys(patch).length) return bad('it holds no values');
  return {patch, error: null};
}
