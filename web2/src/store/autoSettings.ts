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

export type NumKey = 'ntst' | 'nmx' | 'npr' | 'ncol' | 'ds' | 'dsmin' | 'dsmax' | 'rl0' | 'rl1' | 'a0' | 'a1'
  | 'epsl' | 'epsu' | 'epss' | 'iad' | 'mxbf' | 'iid' | 'itmx' | 'itnw' | 'nwtn' | 'iads' | 'suppbp';

type Rule = {kind: 'any'} | {kind: 'positive'} | {kind: 'nonzero'} | {kind: 'range'; lo: number; hi: number | null};

export interface NumField {
  key: NumKey;
  /** the core's form label (and the settings file's name) */
  label: string;
  integer: boolean;
  rule: Rule;
  /** what it does, for the field's title */
  hint: string;
}

const atLeast = (lo: number): Rule => ({kind: 'range', lo, hi: null});

/** the Numerics form's fields in its order (core/auto_settings.cpp num_fields, the same rules) */
export const NUM_FIELDS: NumField[] = [
  {key: 'ntst', label: 'Ntst', integer: true, rule: atLeast(1), hint: 'mesh intervals of a periodic orbit'},
  {key: 'nmx', label: 'Nmax', integer: true, rule: atLeast(1), hint: 'the most points a run computes'},
  {key: 'npr', label: 'NPr', integer: true, rule: atLeast(1), hint: 'a label every this many points'},
  {key: 'ncol', label: 'Ncol', integer: true, rule: {kind: 'range', lo: 2, hi: 7}, hint: 'collocation points per interval'},
  {key: 'ds', label: 'Ds', integer: false, rule: {kind: 'nonzero'}, hint: 'the first step (its sign is the direction)'},
  {key: 'dsmin', label: 'Dsmin', integer: false, rule: {kind: 'positive'}, hint: 'the smallest step'},
  {key: 'dsmax', label: 'Dsmax', integer: false, rule: {kind: 'positive'}, hint: 'the largest step'},
  {key: 'rl0', label: 'Par Min', integer: false, rule: {kind: 'any'}, hint: 'a run stops below it'},
  {key: 'rl1', label: 'Par Max', integer: false, rule: {kind: 'any'}, hint: 'a run stops above it'},
  {key: 'a0', label: 'Norm Min', integer: false, rule: {kind: 'any'}, hint: 'a run stops at a norm below it'},
  {key: 'a1', label: 'Norm Max', integer: false, rule: {kind: 'any'}, hint: 'a run stops at a norm above it'},
  {key: 'epsl', label: 'EPSL', integer: false, rule: {kind: 'positive'}, hint: "Newton's tolerance for the parameters"},
  {key: 'epsu', label: 'EPSU', integer: false, rule: {kind: 'positive'}, hint: "Newton's tolerance for the solution"},
  {key: 'epss', label: 'EPSS', integer: false, rule: {kind: 'positive'}, hint: 'the tolerance locating special points'},
  {key: 'iad', label: 'IAD', integer: true, rule: atLeast(0), hint: 'adapt the mesh every this many steps (0 never)'},
  {key: 'mxbf', label: 'MXBF', integer: true, rule: {kind: 'any'}, hint: 'branch switches at most (negative: one way)'},
  {key: 'iid', label: 'IID', integer: true, rule: {kind: 'range', lo: 0, hi: 5}, hint: 'how much AUTO prints (0 to 5)'},
  {key: 'itmx', label: 'ITMX', integer: true, rule: atLeast(1), hint: 'iterations locating a special point'},
  {key: 'itnw', label: 'ITNW', integer: true, rule: atLeast(1), hint: 'Newton iterations'},
  {key: 'nwtn', label: 'NWTN', integer: true, rule: atLeast(1), hint: 'Newton iterations before the Jacobian is frozen'},
  {key: 'iads', label: 'IADS', integer: true, rule: atLeast(0), hint: 'adapt the step every this many steps (0 never)'},
  {key: 'suppbp', label: 'SuppBP', integer: true, rule: {kind: 'range', lo: 0, hi: 1}, hint: '1: do not look for branch points'},
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

/** the error a Numerics field's text has, as the core would refuse it, or null */
export function numError(key: NumKey, text: string): string | null {
  const f = fieldOf(key), v = Number(text);
  const whole = f.integer ? 'a whole number' : 'a number';
  if (!text.trim() || !Number.isFinite(v) || (f.integer && !Number.isInteger(v))) return `${f.label} must be ${whole}`;
  const r = f.rule;
  if (r.kind === 'positive' && !(v > 0)) return `${f.label} must be a number above 0`;
  if (r.kind === 'nonzero' && v === 0) return `${f.label} must be a number other than 0`;
  if (r.kind === 'range' && (v < r.lo || (r.hi !== null && v > r.hi)))
    return r.hi === null ? `${f.label} must be ${whole} of at least ${r.lo}` : `${f.label} must be ${whole} from ${r.lo} to ${r.hi}`;
  return null;
}

/** the pairs that must be in order, as the core checks them: the first error, or null */
export function pairError(v: Record<NumKey, number>): string | null {
  if (v.dsmin > v.dsmax) return 'Dsmin must be at most Dsmax';
  if (!(v.rl0 < v.rl1)) return 'Par Min must be below Par Max';
  if (!(v.a0 < v.a1)) return 'Norm Min must be below Norm Max';
  return null;
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
