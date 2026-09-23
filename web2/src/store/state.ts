/* The application state and its reducer. Protocol events come in as
   {type:'event'}; the UI's own facts (connection, commands sent, the plot's
   viewport and hover, notifications, the menu drawer) as their own actions.
   Pure: no DOM, no I/O, no clock. */
import type {AskEvent, Command, HelloEvent, StateEvent, XppEvent} from '../protocol/types';
import {appendRows, seriesFromEvent, type PlotSeries} from './series';
import {initialValues, reduceValues, type ValuesAction, type ValuesState} from './values';

export type Theme = 'light' | 'dark' | 'system';

export interface Range {
  min: number;
  max: number;
}

/** What the plot shows: null ranges mean the core's view (Viewaxes/Window) */
export interface Viewport {
  x: Range | null;
  y: Range | null;
}

export interface Hover {
  curve: number;
  row: number;
  x: number;
  y: number;
  t: number | null;
}

export interface LogEntry {
  kind: 'log' | 'error' | 'info';
  text: string;
}

/** a non-blocking notification (errors, the core's alerts) */
export interface Toast {
  id: number;
  kind: 'error' | 'info';
  text: string;
}

export interface AppState {
  connected: boolean;
  exited: number | null;
  hello: HelloEvent | null;
  core: StateEvent | null;
  busy: boolean;
  /** Abort was pressed; the run ends at its idle */
  stopping: boolean;
  ask: AskEvent | null;
  progress: {n: number; of: number} | null;
  bottom: string;
  title: string;
  series: PlotSeries | null;
  /** how many full series events arrived: tests wait on it */
  seriesCount: number;
  /** how many appends (rows of a running integration) went into the series */
  seriesAppends: number;
  viewport: Viewport;
  /** earlier viewports, for Undo zoom (newest last) */
  viewportHistory: Viewport[];
  hover: Hover | null;
  log: LogEntry[];
  toasts: Toast[];
  nextToast: number;
  theme: Theme;
  /** the command menu, a drawer on narrow screens */
  drawerOpen: boolean;
  /** parameters, ICs, BCs, delays, sliders (T3): pending/error/undo, see store/values.ts */
  values: ValuesState;
  /** the values panel, a full-screen sheet on narrow screens */
  valuesOpen: boolean;
}

export type Action =
  | {type: 'event'; ev: XppEvent}
  | {type: 'connection'; open: boolean}
  | {type: 'sent'; cmd: Command}
  | {type: 'aborting'}
  /** push: remember the viewport it replaces (the start of a gesture), for undo */
  | {type: 'viewport'; viewport: Viewport; push?: boolean}
  | {type: 'undoViewport'}
  | {type: 'hover'; hover: Hover | null}
  | {type: 'toast'; kind: Toast['kind']; text: string}
  | {type: 'dismiss'; id: number}
  | {type: 'drawer'; open: boolean}
  | {type: 'theme'; theme: Theme}
  | {type: 'values'; action: ValuesAction}
  | {type: 'valuesPanel'; open: boolean};

const HOME: Viewport = {x: null, y: null};

export const initialState: AppState = {
  connected: false,
  exited: null,
  hello: null,
  core: null,
  busy: false,
  stopping: false,
  ask: null,
  progress: null,
  bottom: '',
  title: '',
  series: null,
  seriesCount: 0,
  seriesAppends: 0,
  viewport: HOME,
  viewportHistory: [],
  hover: null,
  log: [],
  toasts: [],
  nextToast: 1,
  theme: 'system',
  drawerOpen: false,
  values: initialValues,
  valuesOpen: false,
};

const LOG_KEEP = 200, TOASTS_KEEP = 4, HISTORY_KEEP = 50;

function addLog(state: AppState, entry: LogEntry): AppState {
  const log = state.log.length >= LOG_KEEP ? state.log.slice(1 - LOG_KEEP) : state.log.slice();
  log.push(entry);
  return {...state, log};
}

function addToast(state: AppState, kind: Toast['kind'], text: string): AppState {
  const toasts = [...state.toasts, {id: state.nextToast, kind, text}].slice(-TOASTS_KEEP);
  return {...state, toasts, nextToast: state.nextToast + 1};
}

/* commands with no idle of their own (abort) or none at all (quit) */
const NO_IDLE = new Set(['abort', 'quit']);

function sameCurves(a: PlotSeries | null, b: PlotSeries): boolean {
  return !!a && a.win === b.win && JSON.stringify(a.curves) === JSON.stringify(b.curves);
}

function onEvent(state: AppState, ev: XppEvent): AppState {
  switch (ev.ev) {
    case 'hello':
      return {...state, hello: ev, title: ev.title};
    case 'state':
      return {...state, core: ev};
    case 'series': {
      if (ev.op === 'append') {
        const series = state.series && appendRows(state.series, ev);
        if (!series) return state; /* not ours to continue: the full series follows */
        const hover = state.hover && state.hover.row < ev.from ? state.hover : null;
        return {...state, series, seriesAppends: state.seriesAppends + 1, hover};
      }
      const series = seriesFromEvent(ev);
      /* another window or other curves: the user's zoom does not apply to it */
      const keep = sameCurves(state.series, series);
      return {
        ...state, series, seriesCount: state.seriesCount + 1, hover: null,
        viewport: keep ? state.viewport : HOME, viewportHistory: keep ? state.viewportHistory : [],
      };
    }
    case 'ask':
      return {...state, ask: ev};
    case 'idle':
      return {
        ...state, busy: false, stopping: false, ask: null, progress: null,
        values: reduceValues(state.values, {type: 'settled'}),
      };
    case 'progress':
      return {...state, progress: ev.of > 0 ? {n: ev.n, of: ev.of} : null};
    case 'title':
      return {...state, title: ev.text};
    case 'message':
      if (ev.error !== undefined) {
        const withLog = addToast(addLog({...state, bottom: ev.error}, {kind: 'error', text: ev.error}), 'error', ev.error);
        /* a rejected `set`/`slide`: shown as that field's error too (A11), not only the toast */
        return {...withLog, values: reduceValues(withLog.values, {type: 'error', text: ev.error})};
      }
      if (ev.bottom !== undefined) return {...state, bottom: ev.bottom};
      return state;
    case 'log':
      return addLog(state, {kind: 'log', text: ev.text});
    case 'exit':
      return {...state, exited: ev.code, busy: false, stopping: false};
    case 'bye':
      return addLog(state, {kind: 'info', text: 'XPP has exited.'});
    default:
      return state;
  }
}

function sameViewport(a: Viewport, b: Viewport): boolean {
  return JSON.stringify(a) === JSON.stringify(b);
}

export function reduce(state: AppState, action: Action): AppState {
  switch (action.type) {
    case 'event':
      return onEvent(state, action.ev);
    case 'connection':
      return action.open === state.connected ? state : {...state, connected: action.open};
    case 'sent':
      if (action.cmd.cmd === 'answer') return {...state, ask: null};
      return NO_IDLE.has(action.cmd.cmd) ? state : {...state, busy: true};
    case 'aborting':
      return state.busy ? {...state, stopping: true} : state;
    case 'viewport': {
      if (sameViewport(action.viewport, state.viewport)) return state;
      const history = action.push
        ? [...state.viewportHistory, state.viewport].slice(-HISTORY_KEEP) : state.viewportHistory;
      return {...state, viewport: action.viewport, viewportHistory: history};
    }
    case 'undoViewport': {
      if (!state.viewportHistory.length) return state;
      const history = state.viewportHistory.slice(0, -1);
      return {...state, viewport: state.viewportHistory[state.viewportHistory.length - 1], viewportHistory: history};
    }
    case 'hover':
      return {...state, hover: action.hover};
    case 'toast':
      return addToast(state, action.kind, action.text);
    case 'dismiss':
      return {...state, toasts: state.toasts.filter(t => t.id !== action.id)};
    case 'drawer':
      return action.open === state.drawerOpen ? state : {...state, drawerOpen: action.open};
    case 'theme':
      return {...state, theme: action.theme};
    case 'values':
      return {...state, values: reduceValues(state.values, action.action)};
    case 'valuesPanel':
      return action.open === state.valuesOpen ? state : {...state, valuesOpen: action.open};
  }
}
