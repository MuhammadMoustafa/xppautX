/* The application state and its reducer. Protocol events come in as
   {type:'event'}; the UI's own facts (connection, commands sent, the plots'
   viewports, the tab shown, hover, notifications, the menu drawer) as their
   own actions. Pure: no DOM, no I/O, no clock. */
import {pickModeOf, startPick, type PickState} from '../plot/pick';
import type {AskEvent, Command, HelloEvent, StateEvent, View, XppEvent} from '../protocol/types';
import {
  coreMoved, initialPlots, onAppend, onDfield, onNullclines, onPlots, onSeries, select, setViewport, undoViewport,
  windowOf, type PlotsState,
  type Viewport,
} from './plots';
import {initialFiles, missingFile, onSent, reduceFiles, type FilesAction, type FilesState, type RunRecord} from './files';
import {initialTable, reduceTable, type TableAction, type TableState} from './table';
import {initialText, reduceText, type TextAction, type TextState} from './text';
import {initialValues, reduceValues, type ValuesAction, type ValuesState} from './values';

export type {Range, Viewport} from './plots';

export type Theme = 'light' | 'dark' | 'system';

export interface Hover {
  curve: number;
  row: number;
  x: number;
  y: number;
  t: number | null;
}

/** 'auto' is a best-effort guess (classifyLogText, below): the protocol does
    not tag xpp_log_auto's lines apart from any other stderr text, so this is
    only ever as good as the patterns AUTO's own console table prints. */
export interface LogEntry {
  kind: 'log' | 'error' | 'info' | 'auto';
  text: string;
}

/** distinguishes AUTO's console output from the rest of the core's log
    (docs/ui-v2.md T16, Messages: "the core's log and AUTO output are
    distinguishable"), cheaply and without a protocol change: AUTO's table
    header and rows (core/autlib1.c xpp_log_auto, "  BR    PT  TY LAB " then
    "%4li%6li  %c%c%4li%14.6E..." rows) and its handful of fixed messages
    ("Generating starting data", "Hopf point", ...). Anything else stays
    plain `log`; this never sees `message` `error` text (handled separately). */
const AUTO_ROW = /^\s*-?\d+\s+-?\d+\s+\S\S\s+-?\d+(\s+-?\d+(\.\d+)?([eE][+-]?\d+)?){2,}\s*$/;
const AUTO_PHRASE = /BR\s+PT\s+TY\s+LAB|Generating starting data|Restart at EP label|Hopf point|Limit point|Periodic point|Max point|End point|NPARX|NCOL=|DSMIN|DSMAX|Division by Zero|Initialization Error CRASH|Restart label/;
export function classifyLogText(text: string): 'log' | 'auto' {
  return AUTO_ROW.test(text) || AUTO_PHRASE.test(text) ? 'auto' : 'log';
}

/** what a notification offers: "Add file…" for a file the core could not
    open, which uploads it under `name` and runs `run` again (store/files.ts) */
export interface ToastAction {
  kind: 'addFile';
  name: string;
  run: RunRecord | null;
}

/** a non-blocking notification (errors, the core's alerts) */
export interface Toast {
  id: number;
  kind: 'error' | 'info';
  text: string;
  action?: ToastAction;
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
  /** a mouse, rubber or drag ask as a plot mode (plot/pick.ts), while it lasts */
  pick: PickState | null;
  /** the core's hint for the running command ("Click on initial data"), or empty */
  box: string;
  progress: {n: number; of: number} | null;
  bottom: string;
  title: string;
  /** the plot windows, each with its series and zoom, and the active one (store/plots.ts) */
  plots: PlotsState;
  /** how many full series events arrived: tests wait on it */
  seriesCount: number;
  /** how many appends (rows of a running integration) went into a series */
  seriesAppends: number;
  /** the point read out on the active window's plot */
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
  /** the data table (T10): the cached page, selection and CSV export, see store/table.ts */
  table: TableState;
  /** text views (T16): equations, source, equilibrium, and their panel's open/tab state */
  text: TextState;
  /** files through the browser's dialogs (T5): listing, replace confirm, uploads, see store/files.ts */
  files: FilesState;
}

export type Action =
  | {type: 'event'; ev: XppEvent}
  | {type: 'connection'; open: boolean}
  | {type: 'sent'; cmd: Command}
  | {type: 'aborting'}
  /** window `win`'s zoom (the active window's without `win`); push: remember
      the viewport it replaces (the start of a gesture), for undo */
  | {type: 'viewport'; viewport: Viewport; push?: boolean; win?: number}
  | {type: 'undoViewport'; win?: number}
  /** the user picked a plot window's tab (the core is told with `click`) */
  | {type: 'selectWindow'; win: number}
  | {type: 'hover'; hover: Hover | null}
  /** the plot mode's crosshair or corner moved */
  | {type: 'pick'; pick: PickState | null}
  | {type: 'toast'; kind: Toast['kind']; text: string}
  | {type: 'dismiss'; id: number}
  | {type: 'drawer'; open: boolean}
  | {type: 'theme'; theme: Theme}
  | {type: 'values'; action: ValuesAction}
  | {type: 'valuesPanel'; open: boolean}
  | {type: 'table'; action: TableAction}
  | {type: 'text'; action: TextAction}
  | {type: 'files'; action: FilesAction};

export const initialState: AppState = {
  connected: false,
  exited: null,
  hello: null,
  core: null,
  busy: false,
  stopping: false,
  ask: null,
  pick: null,
  box: '',
  progress: null,
  bottom: '',
  title: '',
  plots: initialPlots,
  seriesCount: 0,
  seriesAppends: 0,
  hover: null,
  log: [],
  toasts: [],
  nextToast: 1,
  theme: 'system',
  drawerOpen: false,
  values: initialValues,
  valuesOpen: false,
  table: initialTable,
  text: initialText,
  files: initialFiles,
};

const LOG_KEEP = 200, TOASTS_KEEP = 4;

function addLog(state: AppState, entry: LogEntry): AppState {
  const log = state.log.length >= LOG_KEEP ? state.log.slice(1 - LOG_KEEP) : state.log.slice();
  log.push(entry);
  return {...state, log};
}

function addToast(state: AppState, kind: Toast['kind'], text: string, action?: ToastAction): AppState {
  const toast: Toast = action ? {id: state.nextToast, kind, text, action} : {id: state.nextToast, kind, text};
  const toasts = [...state.toasts, toast].slice(-TOASTS_KEEP);
  return {...state, toasts, nextToast: state.nextToast + 1};
}

/* commands with no idle of their own (abort) or none at all (quit); a
   `browser` request with `from` is answered at once too (docs/protocol.md:
   "answered at once with browser, even during a prompt"), so the table's
   paging (store/table.ts planRequest) never leaves the status bar stuck
   showing Working */
const NO_IDLE = new Set(['abort', 'quit']);
function noIdle(cmd: Command): boolean {
  return NO_IDLE.has(cmd.cmd) || (cmd.cmd === 'browser' && 'from' in cmd);
}

/* another window shown: the readout was the old one's */
function withPlots(state: AppState, plots: PlotsState): AppState {
  if (plots === state.plots) return state;
  return {...state, plots, hover: plots.active === state.plots.active ? state.hover : null};
}

/** the core's window moved (Viewaxes, Window/Zoom, Fit, a scroll): what it
    shows now is what the user asked for, so the plot goes back to it (the
    zoom it had stays one Undo away) */
function coreViewMoved(a: View | undefined, b: View): boolean {
  return !!a && a.win === b.win && (a.xlo !== b.xlo || a.xhi !== b.xhi || a.ylo !== b.ylo || a.yhi !== b.yhi);
}

function onEvent(state: AppState, ev: XppEvent): AppState {
  switch (ev.ev) {
    case 'hello':
      return {...state, hello: ev, title: ev.title};
    case 'state': {
      const moved = ev.view && coreViewMoved(state.core?.view, ev.view);
      return {...state, core: ev, plots: moved ? coreMoved(state.plots, ev.view.win) : state.plots};
    }
    case 'series': {
      const shown = ev.win === state.plots.active;
      if (ev.op === 'append') {
        const plots = onAppend(state.plots, ev);
        if (!plots) return state; /* not ours to continue: the full series follows */
        const hover = state.hover && (!shown || state.hover.row < ev.from) ? state.hover : null;
        return {...state, plots, seriesAppends: state.seriesAppends + 1, hover};
      }
      return {
        ...state, plots: onSeries(state.plots, ev), seriesCount: state.seriesCount + 1,
        hover: shown ? null : state.hover,
      };
    }
    case 'plots':
      return withPlots(state, onPlots(state.plots, ev));
    case 'nullclines':
      return withPlots(state, onNullclines(state.plots, ev));
    case 'dfield':
      return withPlots(state, onDfield(state.plots, ev));
    case 'window':
      /* create selects the new window too; destroy waits for `plots` */
      return ev.op === 'select' && ev.win <= 10 ? withPlots(state, select(state.plots, ev.win)) : state;
    case 'ask': {
      const mode = pickModeOf(ev, state.core?.view, !!windowOf(state.plots, Number(ev.win))?.series);
      /* a plot mode is the core's active window's: its tab is the one shown */
      const withAsk = {...state, ask: ev, pick: mode ? startPick(state.pick, ev, mode) : null};
      return mode ? withPlots(withAsk, select(state.plots, Number(ev.win))) : withAsk;
    }
    case 'idle':
      return {
        ...state, busy: false, stopping: false, ask: null, pick: null, box: '', progress: null,
        values: reduceValues(state.values, {type: 'settled'}),
      };
    case 'progress':
      return {...state, progress: ev.of > 0 ? {n: ev.n, of: ev.of} : null};
    case 'title':
      return {...state, title: ev.text};
    case 'message':
      if (ev.error !== undefined) {
        /* a file the command could not open: the notification offers to add it */
        const missing = missingFile(ev.error, state.files.run);
        const action: ToastAction | undefined = missing ? {kind: 'addFile', name: missing, run: state.files.run} : undefined;
        const files = {...state.files, runFailed: true};
        const withLog = addToast(addLog({...state, bottom: ev.error, files}, {kind: 'error', text: ev.error}), 'error',
          ev.error, action);
        /* a rejected `set`/`slide`: shown as that field's error too (A11), not only the toast */
        return {...withLog, values: reduceValues(withLog.values, {type: 'error', text: ev.error})};
      }
      if (ev.bottom !== undefined) return {...state, bottom: ev.bottom};
      if (ev.box !== undefined) return {...state, box: ev.box};
      return state;
    case 'browser':
      return {...state, table: reduceTable(state.table, {type: 'event', ev})};
    case 'equations':
      return {...state, text: reduceText(state.text, {type: 'equations', ev})};
    case 'source':
      return {...state, text: reduceText(state.text, {type: 'source', ev})};
    case 'equilibrium':
      return {...state, text: reduceText(state.text, {type: 'equilibrium', ev})};
    case 'log':
      return addLog(state, {kind: classifyLogText(ev.text), text: ev.text});
    case 'exit':
      return {...state, exited: ev.code, busy: false, stopping: false};
    case 'bye':
      return addLog(state, {kind: 'info', text: 'XPP has exited.'});
    default:
      return state;
  }
}

export function reduce(state: AppState, action: Action): AppState {
  switch (action.type) {
    case 'event':
      return onEvent(state, action.ev);
    case 'connection':
      return action.open === state.connected ? state : {...state, connected: action.open};
    case 'sent': {
      const files = onSent(state.files, action.cmd, state.ask, state.core?.menu ?? 0);
      if (files !== state.files) state = {...state, files};
      if (action.cmd.cmd === 'answer') {
        /* a point or a box is done once answered; a drag is asked again until it ends */
        const p = state.pick, cancelled = action.cmd.ok === 0;
        return {...state, ask: null, pick: !p || cancelled ? null : p.mode === 'drag' ? p : {...p, waiting: true}};
      }
      return noIdle(action.cmd) ? state : {...state, busy: true};
    }
    case 'aborting':
      return state.busy ? {...state, stopping: true} : state;
    case 'viewport':
      return withPlots(state, setViewport(state.plots, action.win ?? state.plots.active, action.viewport, action.push));
    case 'undoViewport':
      return withPlots(state, undoViewport(state.plots, action.win ?? state.plots.active));
    case 'selectWindow':
      return withPlots(state, select(state.plots, action.win));
    case 'hover':
      return {...state, hover: action.hover};
    case 'pick':
      return {...state, pick: action.pick};
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
    case 'table':
      return {...state, table: reduceTable(state.table, action.action)};
    case 'text':
      return {...state, text: reduceText(state.text, action.action)};
    case 'files':
      return {...state, files: reduceFiles(state.files, action.action)};
  }
}
