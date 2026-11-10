/* The application state and its reducer. Protocol events come in as
   {type:'event'}; the UI's own facts (connection, commands sent, the plots'
   viewports, the tab shown, hover, notifications, the menu drawer) as their
   own actions. Pure: no DOM, no I/O, no clock. */
import {withDock} from './dock';
import {setPointTypes} from './pointTypes';
import {setFileFormats} from './fieldKinds';
import {computationName} from '../protocol/kinds';
import {pickModeOf, startPick, type PickState} from '../plot/pick';
import type {AskEvent, Command, ErrorAction, HelloEvent, LoadErrorEvent, NumericsField, StateEvent, View, WindowListEntry, XppEvent} from '../protocol/types';
import {errorPlace, errorText, unreadFile, type ErrorPlace} from '../protocol/errors';
import {CONTINUE_INPUT_DEFAULT, type ContinueInput} from './steady';
import {
  coreMoved, initialPlots, onAppend, onDfield, onEnd, onMarks, onNullclines, onPlots, onSeries, onWindowRuns,
  rotate3d, select, setViewport, showRuns, windowOf, type PlotsState,
  type Viewport,
} from './plots';
import {initialAplot, reduceAplot, type AplotAction, type AplotState} from './aplot';
import {initialAni, reduceAni, type AniAction, type AniState} from './ani';
import {initialKinescope, reduceKinescope, snapshotWindow, type KinescopeAction, type KinescopeState} from './kinescope';
import {initialFiles, onSent, reduceFiles, type FilesAction, type FilesState, type RunRecord} from './files';
import {
  diagramSettled, initialDiagram, reduceDiagram, type AutoxInfoEvent, type AutoxJobs, type AutoxJoined, type DiagramAction, type DiagramEvent, type DiagramState,
} from './diagram';
import {initialTable, reduceTable, type TableAction, type TableState} from './table';
import {appendLog, emptyLog, lastEntry, type Log, type LogEntry} from './log';
import {initialText, reduceText, type TextAction, type TextState} from './text';
import {initialHelp, reduceHelp, type HelpAction, type HelpState} from './help';
import {initialPlayer, reducePlayer, type PlayerAction, type PlayerState} from './player';
import {initialKeymap, reduceKeymap, type KeymapAction, type KeymapState} from './keymap';
import type {SteadyInput} from './steady';
import {initialValues, reduceValues, type ValuesAction, type ValuesState} from './values';
import {defaultPanels, type Panels} from './panels';
import {initialAutoxScript, type AutoxScript} from './autoxScript';
import {
  initialAutoxSettings, reduceAutoxSettings, type AutoxSettings, type AutoxSettingsAction, type AutoxSettingsState,
} from './autoxSettings';

export type {Range, Viewport} from './plots';

export type Theme = 'light' | 'dark' | 'system';

export interface Hover {
  curve: number;
  row: number;
  x: number;
  y: number;
  t: number | null;
}

export type {LogEntry} from './log';

/** distinguishes autoX's console output from the rest of the core's log
    (docs/ui-v2.md T16, Messages: "the core's log and autoX output are
    distinguishable"), cheaply and without a protocol change: autoX's table
    header and rows (core/autox_lib1.c xpp_log_autox, "  BR    PT  TY LAB " then
    "%4li%6li  %c%c%4li%14.6E..." rows, the type blank on the rows NPr
    prints, T27) and its handful of fixed messages
    ("Generating starting data", "Hopf point", ...) and why a branch ended
    (core/autox_stop.cpp, "Branch 1 stopped at point 57: ..."). Anything else stays
    plain `log`; this never sees `message` `error` text (handled separately). */
const AUTOX_ROW = /^\s*-?\d+\s+-?\d+\s+([A-Za-z]+\s+)?-?\d+(\s+-?\d+(\.\d+)?([eE][+-]?\d+)?){2,}\s*$/;
const AUTOX_PHRASE = /BR\s+PT\s+TY\s+LAB|Generating starting data|Restart at EP label|Hopf point|Limit point|Periodic point|Max point|End point|NPARX|NCOL=|min_step_size|max_step_size|Division by Zero|Initialization Error CRASH|Restart label|Branch \d+ stopped at point \d+:/;
export function classifyLogText(text: string): 'log' | 'autox' {
  return AUTOX_ROW.test(text) || AUTOX_PHRASE.test(text) ? 'autox' : 'log';
}

/** what a notification offers: "Add file…" for a file the core could not
    open, which uploads it under `name` and runs `run` again (store/files.ts) */
export interface ToastAction {
  kind: 'addFile';
  name: string;
  run: RunRecord | null;
}

/** a notification: an error waits in the error dialog (ErrorDialog.tsx) until OK, the core's alerts are toasts */
export interface Toast {
  id: number;
  kind: 'error' | 'info';
  text: string;
  action?: ToastAction;
  /** an error's place, when the core named one (protocol/errors.ts): the dialog shows it */
  place?: ErrorPlace;
  /** what the core offers to do about the error (W260): a button of the dialog */
  offer?: ErrorAction;
}

/** a part of a file of values (docs/protocol.md `set.list_file_contents`): where it comes from, its set's name, and why it
    cannot be loaded when it cannot */
export interface ValuesPart {from: 'defaults' | 'set' | 'file'; name: string; problem?: string}
/** the file asked about, for which scope, what kind of file it is and its parts that hold that scope's kind */
export interface ValuesFileOffer {file: string; scope: string; format: string; parts: ValuesPart[]}

/** one group's settings as the core last gave them (`options.get`): each row's text the session's method and model use, and the choices the model cannot have */
export interface OptionValues {
  values: Record<string, string>;
  unavailable: Record<string, Record<string, string>>;
}

export interface AppState {
  connected: boolean;
  exited: number | null;
  /** why the model did not load, and where (W63c): the page shows it by its line */
  loadError: LoadErrorEvent | null;
  hello: HelloEvent | null;
  core: StateEvent | null;
  busy: boolean;
  /** the running command computes (the core's `computing` event, until its done): only view
      and control actions start then (protocol/kinds.ts, W95); busy alone disables nothing */
  computing: boolean;
  /** what the busy command runs, for the status line: the menu item last
      answered ("Go"), "autoX" for the autoX view's Run; null when unknown (W68) */
  running: string | null;
  /** Abort was pressed; the run ends at its done */
  stopping: boolean;
  ask: AskEvent | null;
  /** a mouse, rubber or drag ask as a plot mode (plot/pick.ts), while it lasts */
  pick: PickState | null;
  /** the core's hint for the running command ("Click on initial data"), or empty */
  box: string;
  progress: {n: number; of: number} | null;
  bottom: string;
  bottomHelp: {chapter: string; anchor: string} | null;
  title: string;
  /** the plot windows, each with its series and zoom, and the active one (store/plots.ts) */
  plots: PlotsState;
  /** every window of the core, with whether it is shown (the `windows` event) */
  windows: WindowListEntry[];
  /** the views in windows of their own, by `kind:name` (popout.ts): their tabs say so */
  popouts: readonly string[];
  /** how many whole series arrived (a full series event, or the end of a live run): tests wait on it */
  seriesCount: number;
  /** how many appends (rows of a running integration) went into a series */
  seriesAppends: number;
  /** the point read out on the active window's plot */
  hover: Hover | null;
  log: Log;
  toasts: Toast[];
  /** counts the log lines (the core's warnings) that arrived: the status bar flashes when it changes */
  flash: number;
  nextToast: number;
  theme: Theme;
  /** the side panels' collapsed state and sizes (W226), kept per viewer */
  panels: Panels;
  /** the command palette (Search commands) */
  drawerOpen: boolean;
  /** the menu bar's drawer on a phone (below 48rem; the bar is always shown above that) */
  menuOpen: boolean;
  /** a command's form: the values it starts with, and the arguments the place it was opened from settles (a section's scope), which it leaves out */
  commandForm: {id: string; values: Record<string, unknown>; fixed?: string[]} | null;
  /** the settings panel open as a dialog (ui/OptionsPanel.tsx): the option group it shows */
  optionsPanel: {group: string} | null;
  /** each group's values as the core last gave them, for every place that edits the group's rows (a panel, a command's form, Values' Numerics) */
  optionValues: Record<string, OptionValues>;
  /** the parts of a file of values that hold a section's kind, as `set.list_file_contents` listed them, offered to pick
      one to load with `set.load_file` (W321) */
  valuesFile: ValuesFileOffer | null;
  annotationsOpen: boolean;
  selectedAnnotation: number | null;
  /** the Continue field of the run toolbar, which the Continue command's key reads too (W213) */
  continueInput: ContinueInput;
  /** the steady-state settings the user typed (RunToolbar), over steadyDefaults: kept here so the command
      Run to steady state, from a key or the toolbar, runs with them */
  steadyEdit: Partial<SteadyInput>;
  /** the controls sent (CONTROL_DONE) whose done has not come: each is answered at once, even in the middle of a computation whose own done comes later */
  controlDone: number;
  /** parameters, ICs, BCs, delays, sliders (T3): pending edits and errors, see store/values.ts */
  values: ValuesState;
  /** the values panel, a full-screen sheet on narrow screens */
  dockOpen: boolean;
  /** the data table (T10): the cached page, selection and CSV export, see store/table.ts */
  table: TableState;
  /** text views (T16): equations, source, equilibrium, and their panel's open/tab state */
  text: TextState;
  /** files through the browser's dialogs (T5): listing, replace confirm, uploads, see store/files.ts */
  files: FilesState;
  /** the autoX diagram and its view (T11a), see store/diagram.ts */
  diagram: DiagramState;
  /** autoX's Numerics, parameters, axes and Mark values (T22), see store/autoxSettings.ts */
  autoxSettings: AutoxSettingsState;
  /** the open .autox script's runs (W684), see store/autoxScript.ts */
  autoxScript: AutoxScript;
  /** the main numerics (the `numerics` event, W106), shown in the values panel */
  numerics: NumericsField[] | null;
  /** the array plot (T12): its latest event, color map and panel state, see store/aplot.ts */
  aplot: AplotState;
  /** the animation (T13): its window, player state and last frame, see store/ani.ts */
  ani: AniState;
  /** the kinescope (T15): captured frames and playback, see store/kinescope.ts */
  kinescope: KinescopeState;
  /** the Help view (W12b): open/closed, the chapter and anchor shown, the search query */
  help: HelpState;
  /** a recording in the player (W59b), see store/player.ts */
  player: PlayerState;
  /** the user's keymap: effective keys, pinned commands and preset (W211), see store/keymap.ts */
  keymap: KeymapState;
}

export type Action =
  | {type: 'event'; ev: XppEvent}
  | {type: 'connection'; open: boolean}
  | {type: 'sent'; cmd: Command}
  | {type: 'aborting'}
  /** window `win`'s zoom (the active window's without `win`) */
  | {type: 'viewport'; viewport: Viewport; win?: string}
  /** a 3D window `win` turned to `theta`, `phi` (a drag, arrow keys, or the
      core's own echo of the turn sent for it), docs/ui-v2.md T14 */
  | {type: 'rotate3d'; win: string; theta: number; phi: number}
  /** the user picked a plot window's tab (the core is told with `click`) */
  | {type: 'selectWindow'; win: string}
  /** the legend's "previous runs" toggle of window `win` */
  | {type: 'showRuns'; win: string; show: boolean}
  | {type: 'hover'; hover: Hover | null}
  /** the plot mode's crosshair or corner moved */
  | {type: 'pick'; pick: PickState | null}
  | {type: 'dismissErrors'}
  | {type: 'toast'; kind: Toast['kind']; text: string}
  | {type: 'dismiss'; id: number}
  | {type: 'commandForm'; form: AppState['commandForm']}
  | {type: 'valuesFile'; offer: ValuesFileOffer | null}
  | {type: 'optionsPanel'; panel: AppState['optionsPanel']}
  | {type: 'optionValues'; group: string; values: OptionValues | null}
  | {type: 'annotations'; open: boolean; selected?: number | null}
  | {type: 'drawer'; open: boolean}
  | {type: 'menu'; open: boolean}
  | {type: 'continueInput'; input: ContinueInput}
  | {type: 'steadyEdit'; edit: Partial<SteadyInput>}
  /** the keymap editor (W212) */
  | {type: 'keymapUi'; action: KeymapAction}
  /** the page's own line in the status bar (a key that cannot act now says why) */
  | {type: 'bottom'; text: string}
  | {type: 'theme'; theme: Theme}
  | {type: 'panels'; panels: Partial<Panels>}
  | {type: 'values'; action: ValuesAction}
  | {type: 'dockPanel'; open: boolean}
  | {type: 'popout'; key: string; out: boolean}
  | {type: 'table'; action: TableAction}
  | {type: 'text'; action: TextAction}
  | {type: 'files'; action: FilesAction}
  | {type: 'diagram'; action: DiagramAction}
  | {type: 'autoxSettings'; action: AutoxSettingsAction}
  | {type: 'aplot'; action: AplotAction}
  | {type: 'ani'; action: AniAction}
  | {type: 'kinescope'; action: KinescopeAction}
  | {type: 'help'; action: HelpAction}
  | {type: 'player'; action: PlayerAction}
  /** the page's own leave question (LEAVE_ASK) opened or closed */
  | {type: 'leave'; open: boolean};

export const initialState: AppState = {
  connected: false,
  exited: null,
  loadError: null,
  hello: null,
  core: null,
  busy: false,
  computing: false,
  running: null,
  stopping: false,
  ask: null,
  pick: null,
  box: '',
  progress: null,
  bottom: '',
  bottomHelp: null,
  title: '',
  plots: initialPlots,
  seriesCount: 0,
  seriesAppends: 0,
  hover: null,
  log: emptyLog,
  toasts: [],
  flash: 0,
  nextToast: 1,
  theme: 'system',
  panels: defaultPanels,
  drawerOpen: false, menuOpen: false, commandForm: null, optionsPanel: null, optionValues: {}, valuesFile: null, annotationsOpen: false, selectedAnnotation: null,
  continueInput: CONTINUE_INPUT_DEFAULT,
  steadyEdit: {},
  controlDone: 0,
  values: initialValues,
  dockOpen: false,
  table: initialTable,
  text: initialText,
  files: initialFiles,
  windows: [],
  popouts: [],
  diagram: initialDiagram,
  autoxSettings: initialAutoxSettings,
  autoxScript: initialAutoxScript,
  numerics: null,
  aplot: initialAplot,
  ani: initialAni,
  kinescope: initialKinescope,
  help: initialHelp,
  player: initialPlayer,
  keymap: initialKeymap,
};

const TOASTS_KEEP = 4;

function addLog(state: AppState, entry: LogEntry): AppState {
  return {...state, log: appendLog(state.log, [entry])};
}

/* printed text as it arrives, one entry per line: the core's stderr comes
   in chunks cut anywhere, several lines in one or a line in several, so a
   chunk that ends a line the last entry began joins that entry, and each
   line is classified whole ("Branch 1 stopped at point 5: pa" + "rameter
   ..." is one autoX line, T23) and alone (a row among other lines, T27) */
function addLogText(state: AppState, text: string): AppState {
  let rest = text, joined: LogEntry | undefined;
  const prev = lastEntry(state.log);
  if (prev && (prev.kind === 'log' || prev.kind === 'autox') && !prev.text.endsWith('\n')) {
    const nl = rest.indexOf('\n'), head = nl < 0 ? rest : rest.slice(0, nl + 1), text = prev.text + head;
    joined = {kind: prev.kind === 'autox' || classifyLogText(text) === 'autox' ? 'autox' : 'log', text};
    rest = rest.slice(head.length);
  }
  const lines = rest.match(/[^\n]*\n|[^\n]+$/g) ?? [];
  if (!joined && !lines.length) return state;
  return {...state, log: appendLog(state.log, lines.map(line => ({kind: classifyLogText(line), text: line})), joined)};
}

function addToast(state: AppState, kind: Toast['kind'], text: string, action?: ToastAction, place?: ErrorPlace, offer?: ErrorAction): AppState {
  const toast: Toast = {id: state.nextToast, kind, text, ...(action ? {action} : {}), ...(place ? {place} : {}), ...(offer ? {offer} : {})};
  const toasts = [...state.toasts, toast].slice(-TOASTS_KEEP);
  return {...state, toasts, nextToast: state.nextToast + 1};
}

/* commands with no done of their own (run.stop) or none at all (the plain
   quit; one that asks or saves is a command of its own); `data.page`
   is answered at once too (docs/protocol.md: "answered at once, even
   during a computation"), so the table's
   paging (store/table.ts planRequest) never leaves the status bar stuck
   showing Working */
const NO_DONE = new Set(['run.stop', 'data.page']);
/** the commands the core answers at once at any moment, a done each, even in the middle of a computation: the
    player's controls and the state; the page's busy never waits on them (their done is counted, state.controlDone) */
const CONTROL_DONE = new Set(['recording.play', 'recording.pause', 'recording.step', 'recording.set_speed', 'session.get_state']);
/** whether a command ends without a done of its own (a control line) */
export function noDone(cmd: Command): boolean {
  if (cmd.cmd === 'session.quit') return cmd.save === false;
  return NO_DONE.has(cmd.cmd) || CONTROL_DONE.has(cmd.cmd);
}

/** The id of the page's own leave question (W110): File > Quit's, worded
    by hello's `quit`, asked by the page when the desktop window's close
    box or Quit comes while a command runs (session.ts quitAsked), the run
    going on; never sent to the core, answered in session.ts. An ask like
    the core's, so it is modal as they are; the core's done leaves it open,
    a question of the core's replaces it. */
export const LEAVE_ASK = -1;

/** the leave question, as the core would ask it now (null before hello) */
function leaveAsk(state: AppState): AskEvent | null {
  const q = state.hello?.quit;
  if (!q) return null;
  return {ev: 'ask', id: LEAVE_ASK, kind: 'choice', title: '', question: state.core?.recording ? q.recording : q.question,
    choices: q.choices, keys: q.keys};
}

/** the status while busy: what runs, when no question is open (W68: every
    key but Escape does nothing then, so the line says which one does) */
export function busyText(running: string | null, asking: boolean): string {
  if (asking) return 'Working…';
  return `${running ?? 'Working'}… Esc stops`;
}

/* the name of what an answer or a command starts, for the status line
   (null: not known, or not a new run) */
function runningName(hello: HelloEvent | null, cmd: Command, ask: AskEvent | null): string | null {
  const label = computationName(hello, cmd);
  if (label) return label;
  if (cmd.cmd !== 'answer' || !ask || ask.kind !== 'choice' || typeof cmd.key !== 'string')
    return null;
  const i = (ask.keys ?? '').toLowerCase().indexOf(cmd.key.toLowerCase());
  const items = ask.choices;
  return i >= 0 && items?.[i] ? items[i].replace(/[()]/g, '') : null;
}

/* another window shown: the readout was the old one's */
function withPlots(state: AppState, plots: PlotsState): AppState {
  if (plots === state.plots) return state;
  return {...state, plots, hover: plots.active === state.plots.active ? state.hover : null, selectedAnnotation: plots.active === state.plots.active ? state.selectedAnnotation : null};
}

/** the core's window moved (Viewaxes, Window/Zoom, Fit, a scroll): what it
    shows now is what the user asked for, so the plot goes back to it */
function coreViewMoved(a: View | undefined, b: View): boolean {
  return !!a && a.win === b.win && (a.xlo !== b.xlo || a.xhi !== b.xhi || a.ylo !== b.ylo || a.yhi !== b.yhi);
}

/** the model file's values: hello.defaults by the state's names (one per entry, in their order) */
function defaultsOf(hello: HelloEvent, st: StateEvent): ValuesAction {
  const zip = (named: [string, number][], vals: number[]): [string, number][] => named.map(([n], i) => [n, vals[i]]);
  return {type: 'defaults', pars: zip(st.pars, hello.defaults.pars), ics: zip(st.ics, hello.defaults.ics)};
}


/** what a window event says happened to its window */
const WINDOW_OPS = {'window.created': 'create', 'window.selected': 'select', 'window.destroyed': 'destroy'} as const;

/** the command ended (its done): nothing is running, asked or half-picked any more */
function settled(state: AppState): AppState {
  return {
    ...state, busy: false, computing: false, running: null, stopping: false, pick: null, box: '',
    ask: state.ask?.id === LEAVE_ASK ? state.ask : null,
    progress: null,
    ani: reduceAni(state.ani, {type: 'playing', playing: false}),
    diagram: diagramSettled(state.diagram),
    values: reduceValues(state.values, {type: 'ended'}),
    autoxSettings: reduceAutoxSettings(state.autoxSettings, {type: 'ended'}),
  };
}

function onEvent(state: AppState, ev: XppEvent): AppState {
  switch (ev.ev) {
    case 'hello': {
      /* a (re)connection: defaults and slider definitions come with state */
      const values = reduceValues({...state.values, defaults: null}, {type: 'settled'});
      /* a set sent before gets no done now */
      setPointTypes(ev.point_types);
      setFileFormats(ev.file_formats);
      return {...state, hello: ev, title: ev.title, values, numerics: null, selectedAnnotation: null, commandForm: null, optionsPanel: null, optionValues: {}, valuesFile: null, windows: [],
        autoxSettings: reduceAutoxSettings(state.autoxSettings, {type: 'reset'}),
        keymap: reduceKeymap(state.keymap, {type: 'info', info: ev.keymap})};
    }
    case 'keymap.changed': {
      const keymap = reduceKeymap(state.keymap, {type: 'info', info: ev.keymap});
      return keymap === state.keymap ? state : {...state, keymap};
    }
    case 'job.live_state':
      return state.core ? {...state, core: {...state.core, now: ev.now, time: ev.time, rates: ev.rates}} : state;
    case 'session.state': {
      const moved = ev.view && coreViewMoved(state.core?.view, ev.view);
      /* `auto` is there exactly while autoX is open */
      const diagram = reduceDiagram(state.diagram, {type: 'core', open: !!(ev as {autox?: unknown}).autox});
      return {
        ...state, core: ev, title: ev.view?.title ?? state.title, plots: moved ? coreMoved(state.plots, ev.view.win) : state.plots, diagram,
        player: reducePlayer(state.player, {type: 'core', player: ev.player}),
        values: reduceValues(state.values.defaults || !state.hello ? state.values : reduceValues(state.values, defaultsOf(state.hello, ev)),
          {type: 'stateSliders', defs: ev.sliders}),
      };
    }
    case 'plot.series': {
      const shown = ev.win === state.plots.active;
      if (ev.op === 'append') {
        const plots = onAppend(state.plots, ev);
        if (!plots) return state; /* not ours to continue: the full series follows */
        const hover = state.hover && (!shown || state.hover.row < ev.from) ? state.hover : null;
        return {...state, plots, seriesAppends: state.seriesAppends + 1, hover};
      }
      if (ev.op === 'end') {
        /* the run's rows are all here: a whole series, as a full one would be */
        const plots = onEnd(state.plots, ev);
        return plots ? {...state, plots, seriesCount: state.seriesCount + 1} : state;
      }
      return {
        ...state, plots: onSeries(state.plots, ev), seriesCount: state.seriesCount + 1,
        hover: shown ? null : state.hover,
      };
    }
    case 'window.list': {
      /* the shown flags drive each window's panel: the autoX view is shown when any of its views is, the
         animation and the array plot panels follow their windows */
      const flag = (kind: WindowListEntry['kind']) => {
        const next = ev.windows.find(w => w.kind === kind)?.shown;
        return next === state.windows.find(w => w.kind === kind)?.shown ? undefined : next;
      };
      const diagramViews = ev.windows.filter(w => w.kind === 'diagram');
      let diagram = state.diagram, ani = state.ani, aplot = state.aplot;
      if (diagramViews.length && (!state.windows.some(w => w.kind === 'diagram') || diagramViews.some(w => w.shown) !== state.windows.some(w => w.kind === 'diagram' && w.shown))) diagram = reduceDiagram(diagram, {type: 'show', shown: diagramViews.some(w => w.shown)});
      const animation = flag('animation'), arrayPlot = flag('array_plot');
      if (animation !== undefined) ani = reduceAni(ani, {type: 'open', open: animation});
      if (arrayPlot !== undefined) aplot = reduceAplot(aplot, {type: 'panel', open: arrayPlot});
      return {...state, windows: ev.windows, diagram, ani, aplot};
    }
    case 'window.renamed': {
      const plots={...state.plots,active:state.plots.active===ev.window?ev.new_name:state.plots.active,
        windows:state.plots.windows.map(w=>w.win===ev.window?{...w,win:ev.new_name,info:w.info?{...w.info,win:ev.new_name,title:ev.new_name}:null}:w)};
      const diagram={...state.diagram,views:state.diagram.views.map(v=>v.name===ev.window?{...v,name:ev.new_name}:v)};
      return withPlots({...state,diagram},plots);
    }
    case 'plot.shown':
      return withPlots(state, onPlots(state.plots, ev));
    case 'plot.runs':
      return withPlots(state, onWindowRuns(state.plots, ev));
    case 'nullcline.curves':
      return withPlots(state, onNullclines(state.plots, ev));
    case 'direction_field.field':
      return withPlots(state, onDfield(state.plots, ev));
    case 'plot.marks':
      return withPlots(state, onMarks(state.plots, ev));
    case 'window.created': case 'window.selected': case 'window.destroyed': {
      const op = WINDOW_OPS[ev.ev];
      /* the windows' numbers: hello.window_names (no window comes before hello) */
      const ids = state.hello?.window_names;
      if (!ids) return state;
      if (op === 'select') {
        const front = {...state, diagram: reduceDiagram(state.diagram, {type:'show',shown:ev.kind === 'diagram'}),
          ani: reduceAni(state.ani,{type:'open',open:ev.kind === 'animation'}),
          aplot: reduceAplot(state.aplot,{type:'panel',open:ev.kind === 'array_plot'}),
          table: {...state.table,open:ev.kind === 'data'}};
        if (ev.kind === 'graph') return withPlots(front,select(front.plots,ev.win));
        if (ev.kind === 'diagram') {
          const view = front.diagram.views.findIndex(v => v.name === ev.win);
          return view < 0 ? front : {...front,diagram:reduceDiagram(front.diagram,{type:'activate',view})};
        }
        return front;
      }
      if (ev.kind === 'diagram')
        return {...state, diagram: reduceDiagram(state.diagram, {type: 'window', op: op})};
      if (ev.kind === 'animation')
        return {...state, ani: reduceAni(state.ani, {type: 'window', exists: op === 'create'})};
      /* create selects the new window too; destroy waits for `plots` */
      if (ev.kind === 'array_plot') {
        if (op === 'destroy') return {...state, aplot: reduceAplot(state.aplot, {type: 'window', open: false})};
        if (op === 'create') {
          /* shown at once, like the classic page (web/xpp-client.js onArrayPlot) */
          const aplot = reduceAplot(reduceAplot(state.aplot, {type: 'window', open: true}), {type: 'panel', open: true});
          return {...state, aplot};
        }
        return state;
      }
      return state;
    }
    case 'ask': {
      const ids = state.hello?.window_names;
      const auto = state.diagram.open && state.diagram.views.some(v=>v.name===ev.win);
      const mode = ids ? pickModeOf(ev, state.core?.view, !!windowOf(state.plots, ev.win!)?.series, {...ids,autox:state.diagram.views[state.diagram.active].name}, auto) : null;
      const withAsk = {...state, ask: ev, pick: mode ? startPick(state.pick, ev, mode) : null};
      /* the autoX diagram's asks (a grab, Axes/Zoom's box, Axes/Scroll's drag) are the autoX view's: it is shown */
      if (auto && (mode || ev.kind === 'grab')) {
        const diagram = reduceDiagram(reduceDiagram(state.diagram, {type: 'show', shown: true}),
          {type: 'grabbing', on: ev.kind === 'grab' || state.diagram.grabbing});
        return {...withAsk, diagram};
      }
      /* a plot mode is the core's active window's: its tab is the one shown */
      return mode ? withPlots(withAsk, select(state.plots, ev.win!)) : withAsk;
    }
    case 'done': {
      /* a control's own done settles nothing */
      if (state.controlDone > 0) {
        const counted = {...state, controlDone: state.controlDone - 1};
        return ev.status==='error'?reduce(counted,{type:'event',ev:{...ev,ev:'session.message'}}):counted;
      }
      /* the error first, as a message then the end of the command came: the edit it belongs to (values.ts) is still the one running */
      const failed=ev.status==='error'?reduce(state,{type:'event',ev:{...ev,ev:'session.message'}}):state;
      return settled(failed);
    }
    case 'animation.frame':
      return {...state, ani: reduceAni(state.ani, {type: 'frame', ev})};
    case 'animation.state':
      return {...state, ani: reduceAni(state.ani, {type: 'state', ev})};
    case 'kinescope.film': {
      if (ev.op === 'capture') {
        const w = windowOf(state.plots, ev.win);
        return w ? {...state, kinescope: reduceKinescope(state.kinescope, {type: 'capture', frame: snapshotWindow(w)})}
          : state;
      }
      if (ev.op === 'reset') return {...state, kinescope: reduceKinescope(state.kinescope, {type: 'clear'})};
      /* play/autoplay: the timing is noted here; session.ts's timer steps `shown` (impure, not this reducer) */
      return {...state, kinescope: reduceKinescope(state.kinescope, {type: 'playing', playing: true, cycles: ev.cycles, delay: ev.delay})};
    }
    case 'job.stopped':
      return {...state, diagram: reduceDiagram(state.diagram, {type: 'runStopped'})};
    case 'autox.view':
      return {...state, diagram: reduceDiagram(state.diagram, {type: 'autoxview', ev})};
    case 'autox.info':
      return {...state, diagram: reduceDiagram(state.diagram, {type: 'info', ev: ev as unknown as AutoxInfoEvent})};
    case 'autox.jobs':
      return {...state, diagram: reduceDiagram(state.diagram, {type: 'jobs', ev: ev as unknown as AutoxJobs})};
    case 'autox.joined':
      return {...state, diagram: reduceDiagram(state.diagram, {type: 'joined', ev: ev as unknown as AutoxJoined})};
    case 'autox.settings':
      return {...state, autoxSettings: reduceAutoxSettings(state.autoxSettings, {type: 'event', ev: ev as unknown as AutoxSettings})};
    case 'autox.script_runs':
      return {...state, autoxScript: ev as unknown as AutoxScript};
    case 'run.numerics':
      return {...state, numerics: ev.fields};
    case 'job.progress':
      return {...state, progress: ev.of > 0 ? {n: ev.n, of: ev.of} : null};
    case 'job.computing':
      return {...state, computing: true};
    case 'session.message':
      if (ev.error !== undefined) {
        /* a file the command could not read (the core names it, with no line): the notification offers to add it */
        const unread = unreadFile(ev);
        const action: ToastAction | undefined = unread ? {kind: 'addFile', name: unread, run: state.files.run} : undefined;
        /* Messages and the status line read as the console does (file:line: what), the dialog shows the place apart */
        const text = errorText(ev);
        const files = {...state.files, runFailed: true};
        /* a rejected `set`/`slide` is that field's error (A11), a rejected `auto` `set` the autoX
           form's: the field or form shows it where it was typed and takes the focus back (WF-001),
           so it opens no error dialog (W104 review) -- it is still in Messages */
        const values = reduceValues(state.values, {type: 'error', text: ev.error, field: ev.field});
        const autoxSettings = /^autoX settings: /.test(ev.error)
          ? reduceAutoxSettings(state.autoxSettings, {type: 'error', text: ev.error}) : state.autoxSettings;
        const logged = addLog({...state, bottom: text, bottomHelp: null, files, values, autoxSettings}, {kind: 'error', text});
        const claimed = values !== state.values || autoxSettings !== state.autoxSettings;
        return claimed ? logged : addToast(logged, 'error', ev.error, action, errorPlace(ev), ev.action);
      }
      if (ev.bottom !== undefined) return {...state, bottom: ev.bottom, bottomHelp: ev.help ?? null};
      if (ev.box !== undefined) return {...state, box: ev.box};
      return state;
    case 'array_plot.shown':
      return {...state, aplot: reduceAplot(state.aplot, {type: 'event', ev})};
    case 'data.updated':
      return {...state, table: reduceTable(state.table, {type: 'event', ev})};
    case 'session.equations':
      return {...state, text: reduceText(state.text, {type: 'equations', ev})};
    case 'session.source':
      return {...state, text: reduceText(state.text, {type: 'source', ev})};
    case 'equilibrium.found':
      return {...state, text: reduceText(state.text, {type: 'equilibrium', ev})};
    case 'autox.diagram':
      return {...state, diagram: reduceDiagram(state.diagram, {type: 'event', ev: ev as unknown as DiagramEvent})};
    case 'recording.player':
      return {...state, player: reducePlayer(state.player, {type: 'player', ev})};
    case 'recording.press':
      return {...state, player: reducePlayer(state.player, {type: 'press', ev})};
    case 'session.log': {
      const next = addLogText(state, ev.text);
      /* a warning (the core logs nothing else at its default level): the status bar flashes, no dialog */
      return lastEntry(next.log)?.kind === 'log' && /\S/.test(ev.text) ? {...next, flash: next.flash + 1} : next;
    }
    case 'session.error':
      return {...state, loadError: ev};
    case 'exit':
      return {...state, exited: ev.code, busy: false, computing: false, running: null, stopping: false};
    case 'session.bye':
      return addLog(state, {kind: 'info', text: 'XPP has exited.'});
    default:
      return state;
  }
}

export function reduce(state: AppState, action: Action): AppState {
  return withDock(state, reduceAction(state, action));
}

function reduceAction(state: AppState, action: Action): AppState {
  switch (action.type) {
    case 'event':
      return onEvent(state, action.ev);
    case 'connection':
      /* a new connection is told again when the core computes (its computing event is kept) */
      return action.open === state.connected ? state : {...state, connected: action.open, computing: false};
    case 'sent': {
      const files = onSent(state.files, state.hello, action.cmd, state.ask);
      if (files !== state.files) state = {...state, files};
      if (action.cmd.cmd === 'answer') {
        /* a point or a box is done once answered; a drag is asked again until it ends */
        const p = state.pick, cancelled = action.cmd.ok === 0;
        const running = runningName(state.hello, action.cmd, state.ask) ?? state.running;
        return {...state, ask: null, running, pick: !p || cancelled ? null : p.mode === 'drag' ? p : {...p, waiting: true}};
      }
      if (action.cmd.cmd === 'animation.play')
        state = {...state, ani: reduceAni(state.ani, {type: 'playing', playing: true})};
      if (CONTROL_DONE.has(action.cmd.cmd)) return {...state, controlDone: state.controlDone + 1};
      if (noDone(action.cmd)) return state;
      return {...state, busy: true, running: state.busy ? state.running : runningName(state.hello, action.cmd, null)};
    }
    case 'aborting':
      return state.busy ? {...state, stopping: true} : state;
    case 'leave': {
      if (!action.open) return state.ask?.id === LEAVE_ASK ? {...state, ask: null} : state;
      const ask = leaveAsk(state);
      return ask ? {...state, ask} : state;
    }
    case 'viewport':
      return withPlots(state, setViewport(state.plots, action.win ?? state.plots.active, action.viewport));
    case 'rotate3d':
      return withPlots(state, rotate3d(state.plots, action.win, action.theta, action.phi));
    case 'selectWindow':
      return withPlots(state, select(state.plots, action.win));
    case 'showRuns':
      return withPlots(state, showRuns(state.plots, action.win, action.show));
    case 'hover':
      return {...state, hover: action.hover};
    case 'pick':
      return {...state, pick: action.pick};
    case 'toast':
      return addToast(state, action.kind, action.text);
    case 'dismissErrors':
      return {...state, toasts: state.toasts.filter(t => t.kind !== 'error')};
    case 'dismiss':
      return {...state, toasts: state.toasts.filter(t => t.id !== action.id)};
    case 'bottom':
      return {...state, bottom: action.text, bottomHelp: null};
    case 'steadyEdit':
      return {...state, steadyEdit: action.edit};
    case 'keymapUi': {
      const keymap = reduceKeymap(state.keymap, action.action);
      return keymap === state.keymap ? state : {...state, keymap};
    }
    case 'continueInput':
      return action.input.mode === state.continueInput.mode && action.input.text === state.continueInput.text ? state : {...state, continueInput: action.input};
    case 'commandForm': return {...state, commandForm: action.form};
    case 'valuesFile': return {...state, valuesFile: action.offer};
    case 'optionsPanel': return {...state, optionsPanel: action.panel};
    case 'optionValues': {
      const {[action.group]: _old, ...rest} = state.optionValues;
      return {...state, optionValues: action.values ? {...rest, [action.group]: action.values} : rest};
    }
    case 'annotations': return {...state, annotationsOpen: action.open, selectedAnnotation: action.selected === undefined ? state.selectedAnnotation : action.selected};
    case 'drawer':
      return action.open === state.drawerOpen ? state : {...state, drawerOpen: action.open};
    case 'menu':
      return action.open === state.menuOpen ? state : {...state, menuOpen: action.open};
    case 'theme':
      return {...state, theme: action.theme};
    case 'panels':
      return {...state, panels: {...state.panels, ...action.panels}};
    case 'values':
      return {...state, values: reduceValues(state.values, action.action)};
    case 'dockPanel':
      return action.open === state.dockOpen ? state : {...state, dockOpen: action.open};
    case 'popout':
      return state.popouts.includes(action.key) === action.out ? state
        : {...state, popouts: action.out ? [...state.popouts, action.key] : state.popouts.filter(k => k !== action.key)};
    case 'table':
      return {...state, table: reduceTable(state.table, action.action)};
    case 'text':
      return {...state, text: reduceText(state.text, action.action)};
    case 'files':
      return {...state, files: reduceFiles(state.files, action.action)};
    case 'diagram': {
      const diagram = reduceDiagram(state.diagram, action.action);
      return diagram === state.diagram ? state : {...state, diagram};
    }
    case 'autoxSettings':
      return {...state, autoxSettings: reduceAutoxSettings(state.autoxSettings, action.action)};
    case 'aplot':
      return {...state, aplot: reduceAplot(state.aplot, action.action)};
    case 'ani':
      return {...state, ani: reduceAni(state.ani, action.action)};
    case 'kinescope':
      return {...state, kinescope: reduceKinescope(state.kinescope, action.action)};
    case 'help':
      return {...state, help: reduceHelp(state.help, action.action)};
    case 'player':
      return {...state, player: reducePlayer(state.player, action.action)};
  }
}
