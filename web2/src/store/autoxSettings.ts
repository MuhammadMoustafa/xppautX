/* autoX's settings as data (docs/ui-v2.md T22, docs/protocol.md "autoX's
   settings as data"): the Numerics, autoX's parameters, the axes and the
   Mark values, from the `autoxsettings` event, edited in the page's own
   forms at any time. They are settings (W106): an edit goes out at once
   as an `autox` `set`, during a run too, when the core keeps it for the
   run's end (the run in progress keeps what it started with). Each one
   sent is `inflight` until that set's own done, when the event has
   brought the core's values (or a `message` `error` said why not).
   What each value takes, and the messages a refusal says, are the core's
   (the event's `rules`, `pairs` and `step`); the settings file is the
   core's too (autoX's File menu, W118). Pure: no DOM, no I/O. */
import type {Command} from '../protocol/types';
import type {AutoxProjection} from './diagram';

/** a Numerics field's key: its row's key in the continuation group (`hello.options`), the core's `autox_numeric` one; the page names none */
export type NumKey = string;

/** what a Numerics value takes (the event's `rules`: core/autox_settings.cpp field), and the
    message the core refuses another with; each flag only when it holds */
export interface NumRule {
  /** the core's form label */
  label: string;
  integer?: boolean;
  boolean?: boolean;
  /** the names of a choice, in the order of autoX's levels */
  choices?: string[];
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

export interface AutoxAxesSettings {
  plot: AutoxProjection;
  var: string | null;
  par1: string | null;
  par2: string | null;
  xmin: number; xmax: number; ymin: number; ymax: number;
}

/** the `autoxsettings` event's settings, with what the Numerics take */
export const CODIM_KINDS = ['bogdanov_takens', 'cusp', 'zero_hopf', 'generalized_hopf', 'resonance_1_1', 'resonance_1_2', 'resonance_1_3', 'resonance_1_4',
  'fold_flip', 'fold_neimark_sacker', 'flip_neimark_sacker', 'double_neimark_sacker'] as const;
export type CodimKind = typeof CODIM_KINDS[number];
/** every kind set to one value */
export const allCodim = (on: boolean) => Object.fromEntries(CODIM_KINDS.map(k => [k, on])) as Record<CodimKind, boolean>;

export interface AutoxSettings {
  special_points: Record<CodimKind, boolean>;
  numerics: Record<NumKey, number | boolean | string>;
  axes: AutoxAxesSettings;
  marks: [string, number][];
  /** the thread count of autoX and sweeps, a program preference (W267, W434): "auto" or the count */
  threads: 'auto' | number;
  /** the count "auto" gives now, and the most a count may be (the computer's logical processors) */
  threads_auto: number;
  threads_max: number;
  rules: Record<NumKey, NumRule>;
  pairs: NumPair[];
  step: NumStep;
}

/** what an `autox` `set` changes (docs/protocol.md), each part only when given */
export interface AutoxSettingsPatch {
  special_points?: Partial<Record<CodimKind, boolean>>;
  numerics?: Partial<AutoxSettings['numerics']>;
  /** `view`: the view of the diagram whose axes these are (W50; the active one without it) */
  axes?: Partial<AutoxAxesSettings> & {fit?: boolean; view?: string};
  marks?: [string, number][];
  threads?: 'auto' | number;
}

/** a set sent, until its own done: `ahead` other commands' dones come first */
export interface AutoxSettingsInFlight {
  patch: AutoxSettingsPatch;
  ahead: number;
}

export interface AutoxSettingsState {
  /** the core's, from the last event */
  core: AutoxSettings | null;
  /** the sets sent whose command has not ended, in the order sent */
  inflight: AutoxSettingsInFlight[];
  /** why the core refused the last set */
  error: string | null;
}

export const initialAutoxSettings: AutoxSettingsState = {core: null, inflight: [], error: null};

export type AutoxSettingsAction =
  | {type: 'event'; ev: AutoxSettings}
  /** `patch` went out as one set, `ahead` dones before its own */
  | {type: 'sent'; patch: AutoxSettingsPatch; ahead: number}
  /** a command ended: the set whose command it was has been applied or refused */
  | {type: 'ended'}
  /** a new connection: nothing sent is waited for any more */
  | {type: 'reset'}
  | {type: 'error'; text: string};

export function reduceAutoxSettings(s: AutoxSettingsState, a: AutoxSettingsAction): AutoxSettingsState {
  switch (a.type) {
    case 'event':
      return {...s, core: a.ev};
    case 'sent':
      return {...s, inflight: [...s.inflight, {patch: a.patch, ahead: a.ahead}], error: null};
    case 'ended':
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

function apply(base: AutoxSettings, p: AutoxSettingsPatch | null): AutoxSettings {
  if (!p) return base;
  const {fit: _fit, view: _view, ...axes} = p.axes ?? {};
  return {
    ...base,
    special_points: {...base.special_points, ...p.special_points},
    numerics: {...base.numerics, ...p.numerics} as AutoxSettings['numerics'],
    axes: {...base.axes, ...axes},
    marks: p.marks ?? base.marks,
    threads: p.threads ?? base.threads,
  };
}

/** what the forms show: the core's settings with the edits sent and not yet applied over them */
export function shownSettings(s: AutoxSettingsState): AutoxSettings | null {
  return s.core ? s.inflight.reduce((base, f) => apply(base, f.patch), s.core) : null;
}

/** the fields an edit sent is not yet applied to ("numerics.nmx", "axes.plot", "marks") */
export function pendingFields(s: AutoxSettingsState): Set<string> {
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

/** what a patch is as registry commands (docs/protocol.md autox.configure_numerics, ...): one for each part it has, in the order of its parts */
export function autoxSettingsCommands(p: AutoxSettingsPatch): {command: Command; patch: AutoxSettingsPatch}[] {
  const out: {command: Command; patch: AutoxSettingsPatch}[] = [];
  if (p.numerics || p.special_points) {
    out.push({
      command: {cmd: 'autox.configure_numerics', ...p.numerics, ...(p.special_points ? {special_points: p.special_points} : {})},
      patch: {numerics: p.numerics, special_points: p.special_points},
    });
  }
  if (p.axes) {
    const {plot, var: variable, par1, par2, xmin, xmax, ymin, ymax, fit, view} = p.axes;
    const given = {plot_type: plot, variable, parameter1: par1, parameter2: par2, x_min: xmin, x_max: xmax, y_min: ymin, y_max: ymax, fit, view};
    out.push({
      command: {cmd: 'autox.configure_axes', ...Object.fromEntries(Object.entries(given).filter(([, v]) => v !== undefined && v !== null))},
      patch: {axes: p.axes},
    });
  }
  if (p.marks) {
    out.push({command: {cmd: 'autox.configure_marks', parameters: p.marks.map(m => m[0]), values: p.marks.map(m => m[1])}, patch: {marks: p.marks}});
  }
  if (p.threads !== undefined) {
    out.push({command: {cmd: 'preferences.configure', ...(p.threads === 'auto' ? {automatic: true} : {threads: p.threads})}, patch: {threads: p.threads}});
  }
  return out;
}

/** Track settings from the common command form and specialised plot gestures alike. */
export function commandSettings(command: Command): AutoxSettingsPatch | null {
  const {cmd, ...values} = command;
  if (cmd === 'autox.configure_numerics') {
    const {special_points, ...numerics} = values;
    return {numerics: numerics as AutoxSettingsPatch['numerics'], special_points: special_points as AutoxSettingsPatch['special_points']};
  }
  if (cmd === 'autox.configure_axes') {
    const names: Record<string,string> = {plot_type:'plot', variable:'var', parameter1:'par1', parameter2:'par2', x_min:'xmin', x_max:'xmax', y_min:'ymin', y_max:'ymax', fit:'fit', view:'view'};
    return {axes: Object.fromEntries(Object.entries(values).filter(([key]) => key in names).map(([key,value]) => [names[key],value]))};
  }
  if (cmd === 'autox.configure_marks') return {marks: (command.parameters as string[] ?? []).map((name,i) => [name, (command.values as number[])[i]])};
  if (cmd === 'preferences.configure') return {threads: command.automatic === true ? 'auto' : command.threads as number};
  return null;
}
