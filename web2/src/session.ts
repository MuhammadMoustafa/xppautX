import {toolbarCommand} from './ui/navigation';
import {registryRows} from './ui/navigation';
/* The session: connects a transport to the store and is the one place that
   sends commands. Components call its methods, never the transport. */
import {nativeFileDialog, nativeFileRequest, offerDownload, writeTo, type SaveHandle} from './pickers';
import {accumulateScroll, keyScroll} from './plot/aplotScroll';
import {bytesToBase64} from './plot/gif';
import {setLineGeometry} from './plot/linestyle';
import {setAnnotationGlyphs} from './plot/marks';
import {renderFrame} from './plot/kinescopeRender';
import {pickAnswer, type PickState} from './plot/pick';
import {chartOf} from './plot/registry';
import type {Ranges} from './plot/viewmath';
import {HOME, windowOf, type Viewport} from './store/plots';
import {sha256Hex, type FilesApi} from './protocol/files';
import type {Transport} from './protocol/transport';
import {kindOf, mayStart, type LayerWindow} from './protocol/kinds';
import {PROTOCOL, type AskEvent, type Command, type FilmEvent, type UserKeymap, type XppEvent} from './protocol/types';
import type {AplotHover} from './store/aplot';
import type {TextPosition, TextValues} from './store/marks';
import {activeView, autoWindow} from './store/diagram';
import {
  answerName, keepBothName, safeName, uploadPlan, type ReplaceChoice, type RunAnswer, type RunRecord, type Upload,
} from './store/files';
import {createStore, type Store} from './store/store';
import {initialState, LEAVE_ASK, noIdle, reduce, type Action, type AppState} from './store/state';
import {closeDesktopWindow} from './desktop';
import {stepTarget} from './store/ani';
import {snapshotWindow} from './store/kinescope';
import {planRequest} from './store/table';
import {continuePlan, steadyRun} from './store/steady';
import type {TextTab} from './store/text';
import {fieldKey, valueCommands, type ValueSet} from './store/values';
import {autoSettingsCommands, commandSettings, type AutoSettingsPatch} from './store/autoSettings';

/** the data browser's buttons (docs/protocol.md `browser` op; web/xpp-client.js's BROWSER_BUTTONS) */
export type BrowserOp = 'find' | 'replace' | 'unreplace' | 'table' | 'load' | 'write' | 'first' | 'last'
  | 'restore' | 'addcol' | 'delcol';

/** the AUTO window's buttons (toolbarCommand names the registry command of each); Close is session.closeAuto */
export type AutoOp = 'axes' | 'numerics' | 'run' | 'grab' | 'usr' | 'clear' | 'redraw';

/** a change of what a window displays, as the `display` command carries it: the zoom of each axis
    ([low, high] or null), the earlier runs' toggle (`runs`), AUTO's earlier branches' toggle (`show`) */
interface DisplayPatch {
  x?: [number, number] | null;
  y?: [number, number] | null;
  runs?: boolean;
  show?: boolean;
}

/** the display key of view k of the AUTO diagram: 0, -1, -2 ... (the plot windows are 1 up) */
/** the scope of the core's `set.restore_defaults` for a values section (docs/protocol.md) */
const RESTORE_SCOPE = {par: 'parameters', ic: 'initial'} as const;
/** the kind of file the core's `value.save_file` and `value.load_file` take for a values section */
const VALUE_FILE = {par: 'parameters', ic: 'initial_conditions'} as const;



/** a view's display change as auto.set_display: [] clears an axis' zoom, show_earlier is the earlier branches' toggle */
const autoDisplayCommand = (view: string, p: DisplayPatch): Command => ({
  cmd: 'auto.set_display', view,
  ...(p.x !== undefined ? {x_range: p.x ?? []} : {}),
  ...(p.y !== undefined ? {y_range: p.y ?? []} : {}),
  ...(p.show !== undefined ? {show_earlier: p.show} : {}),
});

const rangesOf = (v: Viewport): DisplayPatch => ({
  x: v.x ? [v.x.min, v.x.max] : null,
  y: v.y ? [v.y.min, v.y.max] : null,
});

/** one step of a planned dialogue: the answer to `ask`, or null when that ask is the user's */

/** the array plot window's buttons (toolbarCommand names the registry command of each) */
export type AplotOp = 'redraw' | 'edit' | 'fit' | 'range' | 'print' | 'gif';

/** the commands of the player's Play, Pause and Step buttons */
const PLAYER_CONTROLS = {start: 'recording.play', pause: 'recording.pause', step: 'recording.step'} as const;

export class Session {
  readonly store: Store<AppState, Action>;
  private otherProtocol = false;
  /** commands whose completion has not arrived yet */
  private idlesOwed = 0;
  /** a planned dialogue (T21: the axis dialog, AUTO settings save and load):
      the asks its commands open are answered by these steps in order, until
      the commands' idles */

  /** the point a grab took, until its command's idle (a periodic one's orbit is then imported) */
  private grabbed: number | null = null;
  /** pointer events of a drag made while the core was not asking (docs/protocol.md
      `drag`: it asks again after each one), and whether the drag has ended */
  private dragQueue: Record<string, unknown>[] = [];
  private dragEnded = false;
  /** a file ask for writing answered: the file to hand to the browser once the command is done */
  /* a file the core writes, offered at its command's idle: `ahead` counts the
     idles of the commands sent before it, which come first (W95's click right
     behind a redraw delivered it at the redraw's idle, before it was written) */
  private savedOutputs: {name: string; handle: SaveHandle | null; data: Promise<Blob | null>}[] = [];
  private savingReads = new Set<Promise<Blob | null>>();
  private nativeSave: string | null = null;
  private nativeRun: RunRecord | null = null;
  private retrySave: RunRecord | null = null;
  private retrySaveDir: string | null = null;
  private nativeSaveDir = '';
  private pendingSave: {name: string; handle: SaveHandle | null; ahead: number; suffix?: string} | null = null;
  /** the answer to the replace confirm, when it is open */
  private replaceChoice: ((c: ReplaceChoice) => void) | null = null;
  /** a command run again after "Add file…": the answers its prompts get, and
      the idles to wait for (the menu keys before it, then its own) */
  private replayAnswers: RunAnswer[] = [];
  /** rotate3d's throttle: the last time the 3d-params turn went out for a window,
      and its pending trailing send, by window */
  private rotate3dLast = new Map<string, number>();
  private rotate3dTimer = new Map<string, ReturnType<typeof setTimeout>>();
  /** the last angle a window was turned to while the core was busy, sent at its idle */
  private rotate3dHeld = new Map<string, {theta: number; phi: number}>();
  /** what the page displays is the core's (W65, docs/protocol.md "Display state"): the zoom
      shown, the earlier runs' toggle, AUTO's shown branches. The page keeps a copy it changes
      at once (a drag cannot wait for a round trip) and tells the core; the core's events
      set it back, except for a window whose change is held or on its way: `displayHeld` is the
      latest change of each (window 0: the AUTO diagram) not yet sent, `displayGuard` the
      windows whose zoom and toggles the core's events leave alone, until the idle of the
      command that carried the change (`displayIdles` counts down to it). */
  private displayHeld = new Map<string, DisplayPatch>();
  private displayGuard = new Set<string>();
  private displayIdles = 0;
  /** the tab last picked while the core was busy, shown and sent at its idle */
  private windowHeld: string | null = null;
  private replayIdles = 0;
  /** a command to send when the running one has ended (the AUTO view's close while busy, A10) */
  private afterIdle: Command[] | null = null;
  /** `redraw` was sent for the AUTO diagram's data, not yet answered */
  private diagramAsked = false;
  /** the array plot's scroll made while the core was busy (plot/aplotScroll.ts) */
  private pendingAplotDy = 0;
  /** the kinescope's play/autoplay clock (store/kinescope.ts: the core only
      says a play started; stepping the frames shown is this session's job) */
  private filmTimer: ReturnType<typeof setTimeout> | null = null;

  /** files: the model's folder over HTTP (none in unit tests) */
  constructor(private readonly transport: Transport, private readonly files: FilesApi | null = null) {
    this.store = createStore(reduce, initialState);
  }

  start(): void {
    /* a command the server refused or that never reached it is an error
       as the core's own are (Messages, the error dialog): never lost (W116) */
    this.transport.open(ev => this.receive(ev), open => this.store.dispatch({type: 'connection', open}),
      error => this.store.dispatch({type: 'event', ev: {ev: 'message', error}}));
  }

  private receive(ev: XppEvent): void {
    /* core and page ship together (the page is compiled into the program):
       a page of another build is a shown error, never read as this one */
    if (ev.ev === 'hello') this.otherProtocol = ev.protocol !== PROTOCOL;
    if (this.otherProtocol) {
      if (ev.ev === 'hello')
        this.store.dispatch({type: 'event', ev: {ev: 'message', error: `This page speaks protocol ${PROTOCOL} and the program ${ev.protocol}: `
          + 'open the page the program itself serves (reload it after an update).'}});
      return;
    }
    if (ev.ev === 'hello') {
      setLineGeometry(ev.line_styles); /* before any plot is built */
      setAnnotationGlyphs(ev.annotation_glyphs);
    }
    this.store.dispatch({type: 'event', ev: this.unguarded(ev)});
    if (ev.ev === 'hello') {
      this.displayHeld.clear();
      this.displayGuard.clear();
      this.displayIdles = 0;
      this.idlesOwed = 0;
      /* the plots as data (docs/protocol.md): every data event the server
         speaks (hello.features), asked for on every (re)connection, which
         also makes the server send the windows, their series, nullclines,
         direction fields and marks; values as base64 float32, which a long
         run needs */
      this.send({cmd: 'events.subscribe', events: ev.features, encoding: 'f32'});
    } else if (ev.ev === 'film') {
      this.onFilm(ev);
    } else if (ev.ev === 'ask') {
      if (this.store.getState().player.running >= 0 && ev.kind !== 'pixels') {
        /* a recording's step runs: the player answers its questions, the
           page only shows them (docs/protocol.md "Playing a recording") */
        this.checkDiagram(ev);
        return;
      }
      if (ev.kind === 'pixels') {
        this.answerPixels(ev);
      } else if (ev.kind === 'drag' && (this.dragEnded || this.dragQueue.length)) {
        /* the events made meanwhile first, then the end */
        if (this.dragQueue.length) this.answer(ev, this.dragQueue.shift()!);
        else this.cancel(ev);
      } else if (this.replayAnswers.length) this.continueReplay(ev);
      else if (ev.kind === 'file' && nativeFileDialog()) void this.nativeFile(ev);

    } else if (ev.ev === 'message' && ev.error?.includes('Permission denied:') && this.nativeRun) {
      this.retrySave = this.nativeRun;
      this.retrySaveDir = this.nativeSaveDir;
      this.nativeRun = null;
    } else if (ev.ev === 'saved') {
      const save = this.pendingSave;
      let handle: SaveHandle | null = null;
      if (save && save.ahead === 0 && (ev.file === save.name
          || (save.suffix && ev.file === save.name + save.suffix))) {
        handle = save.handle;
        save.name = ev.file; /* use the core's destination when it appended the dialog's suffix */
      }
      if (ev.saved && safeName(ev.file) && ev.file !== this.nativeSave && this.files) {
        /* Start reading at commit; the next answer waits for this snapshot. */
        const data = this.files.get(ev.file).catch(e => {
          this.failed(`${ev.file} could not be read: ${e instanceof Error ? e.message : String(e)}`);
          return null;
        });
        this.savingReads.add(data);
        void data.then(() => this.savingReads.delete(data));
        this.savedOutputs.push({name: ev.file, handle, data});
      }
      this.nativeSave = null;
    } else if ((ev.ev === 'idle' || ev.ev === 'done')) {
      if (this.idlesOwed > 0) this.idlesOwed--;
      this.afterAuto();
      this.dragQueue = [];
      this.dragEnded = false;
      if (this.replayIdles > 0 && --this.replayIdles === 0) this.replayAnswers = [];
      const save = this.pendingSave;
      if (save && save.ahead > 0) save.ahead--;
      else {
        this.pendingSave = null;
        this.nativeSave = null;
        for (const output of this.savedOutputs.splice(0)) void this.deliver(output.name, output.handle, output.data);
      }
      const next = this.afterIdle;
      this.afterIdle = null;
      if (next) for (const command of next) this.send(command);
      this.flushRotate3d();
      if (this.displayIdles > 0 && --this.displayIdles === 0) this.displayGuard = new Set(this.displayHeld.keys());
      this.flushDisplay();
      this.flushWindow();
      const retry = this.retrySave;
      this.retrySave = null;
      if (retry) {
        this.replayAnswers = retry.answers.slice();
        this.replayIdles = 1;
        this.send(retry.cmd);
        return;
      }
      this.nativeRun = null;

    }
    this.checkDiagram(ev);
  }

  /* the AUTO diagram is data the page must hold whole: a page that connected
     after AUTO opened has none, and an `add` it could not place leaves it out
     of step; `redraw` makes the core send all of it again (docs/protocol.md);
     at the idle, the page's own catch-up like the held changes below */
  private checkDiagram(ev: XppEvent): void {
    const {diagram: d, busy} = this.store.getState();
    if (!d.open || (d.views.every(v => v.axes) && !d.outOfStep)) {
      this.diagramAsked = false;
      return;
    }
    if (this.diagramAsked || busy || (ev.ev !== 'state' && ev.ev !== 'idle')) return;
    this.diagramAsked = true;
    this.send({cmd: 'window.redraw', window: autoWindow(this.store.getState())});
  }

  private continueReplay(ask: AskEvent): void {
    const next = this.replayAnswers[0];
    if (next.kind !== ask.kind) {
      this.replayAnswers = []; /* not the prompt it had: the user's to answer */
      return;
    }
    this.replayAnswers.shift();
    this.answer(ask, next.fields);
  }

  /** whether `cmd` may go out now, by its kind (protocol/kinds.ts, W95): a
      control action always, a view action also while a computation runs, a
      data or computation action only when none runs; while a question is
      open only its answer. The page's being busy with a command of its own
      (a held zoom sent at an idle) disables nothing: the core runs what
      comes after it in turn. */
  may(cmd: Command): boolean {
    const {hello, core, computing, ask, player} = this.store.getState();
    if ((cmd.cmd === 'value.undo' && core?.can_undo === false) || (cmd.cmd === 'value.redo' && core?.can_redo === false)) return false;
    const kind = kindOf(hello, cmd);
    /* a recording's step runs: only what steers the player or a view (W59b) */
    if (player.running >= 0) return kind === 'control' || kind === 'view';
    return mayStart(kind, computing, ask !== null);
  }

  /** Menu, palette, pins and keys all enter the same registry form. */
  registryAction(id: string, values: Record<string, unknown> = {}): void {
    const row = registryRows(this.store.getState().hello).find(row => row.id === id);
    if (!row) { this.failed(`Unknown command ${id}`); return; }
    if (id === 'tool.values') this.store.dispatch({type: 'valuesPanel', open: true});
    else if (id === 'tool.browser') this.openTable();
    else if (id === 'tool.command_palette') this.store.dispatch({type: 'drawer', open: true});
    else if (id === 'tool.keyboard_shortcuts') this.store.dispatch({type: 'keymapUi', action: {type: 'open'}});
    else if (id === 'session.get_source') {this.openText();}
    else if (id === 'tool.manual') this.store.dispatch({type: 'help', action: {type: 'open'}});
    else if (id === 'annotation.list') {
      this.store.dispatch({type: 'annotations', open: true});
      this.send({cmd:'window.select', window:this.store.getState().plots.active});
      this.send({cmd: id, window:this.store.getState().plots.active});
    } else if (row.arguments?.length) this.store.dispatch({type: 'commandForm', form: {id, values}});
    else this.send({cmd: id});
  }

  /** Run to steady state with the settings the toolbar holds (the user's edits over the defaults) */
  runSteady(): void {
    const {hello, numerics, steadyEdit} = this.store.getState();
    const run = steadyRun(hello, numerics, steadyEdit);
    if (!run || run.problem) {
      this.store.dispatch({type: 'bottom', text: run?.problem ?? 'Run to steady state: the numerics have not arrived'});
      return;
    }
    const {decimals, hold, maximum} = run.input;
    this.send({cmd: 'run.steady', decimals: Number(decimals), hold: Number(hold), maximum: Number(maximum)});
  }

  /** Replace the user's keymap (docs/protocol.md "Keymap"): the answer is a `keymap` event, which is how the
      page learns the effective keys, so nothing is changed here */
  setKeymap(map: UserKeymap): void {
    this.send({cmd: 'keymap.set', bindings: map.bindings, pinned: map.pinned, preset: map.preset});
  }

  /** Remove the user's keymap file: all keys, pins and the preset back to the defaults */
  resetKeymap(): void {
    this.send({cmd: 'keymap.reset'});
  }

  /** Continue (W213), the one way to continue: the toolbar's button, the command's key and a click on its
      menu item all send the toolbar's field as `continue`; an input that cannot run says why in the status bar */
  continueRun(): void {
    const {continueInput, numerics, core, hello} = this.store.getState();
    const plan = continuePlan(continueInput, numerics, core, hello);
    if (plan.command) this.send(plan.command);
    else this.store.dispatch({type: 'bottom', text: `Continue: ${core?.now ? plan.problem ?? 'no value' : 'run once before continuing'}`});
  }

  /** whether main-menu item `id` may go out now (may) */
  mayMain(id: string): boolean {
    return this.may({cmd: id});
  }

  mayKey(win: LayerWindow, id: string): boolean {
    return this.may({cmd: toolbarCommand(win, id)});
  }

  /** the one place commands go out. What may not start now (may) is
      discarded here, at the source: its control is disabled anyway, and the
      core would refuse it (docs/protocol.md "Commands during a command").
      False when it did not go. */
  send(cmd: Command): boolean {
    if (!this.may(cmd)) return false;
    if (cmd.cmd === 'auto.run') this.store.dispatch({type:'diagram',action:{type:'run',op:'start',at:Date.now()}});
    const patch = commandSettings(cmd);
    if (patch) this.store.dispatch({type: 'autoSettings', action: {type: 'sent', patch, ahead: this.idlesOwed}});
    if (cmd.cmd !== 'answer' && !noIdle(cmd)) this.idlesOwed++;
    this.store.dispatch({type: 'sent', cmd});
    this.transport.send(cmd);
    return true;
  }

  /** A key answering a core question, or Escape stopping a run. */
  typeKey(key: string): void {
    const {ask, busy, stopping} = this.store.getState();
    if (ask?.kind === 'choice') {
      const i = (ask.keys ?? '').toLowerCase().indexOf(key.toLowerCase());
      if (key.length === 1 && i >= 0) this.answer(ask, {key:ask.keys![i]});
    } else if (key === 'Escape' && busy && !stopping) this.abort();
  }

  answer(ask: AskEvent, fields: Record<string, unknown>): void {
    if (ask.id === LEAVE_ASK) {
      this.leaveAnswered(fields);
      return;
    }
    /* the Start menu of a Run answered: its clock starts now */
    const {run} = this.store.getState().diagram, {points} = activeView(this.store.getState().diagram);
    if (run?.active && ask.kind === 'choice' && points.x.length === run.first)
      this.store.dispatch({type: 'diagram', action: {type: 'run', op: 'clock', at: Date.now()}});
    const send = () => this.send({cmd: 'answer', id: ask.id, ...fields});
    if (this.savingReads.size) void Promise.all(this.savingReads).then(send);
    else send();
  }

  cancel(ask: AskEvent): void {
    if (this.store.getState().files.confirm?.ask === ask.id) this.resolveReplace('cancel');
    this.answer(ask, {ok: 0});
  }

  /** The desktop window's close box or File > Quit (desktop.ts __xppQuit,
      W110): File > Quit's question. While a command runs (and asks
      nothing), the page asks it itself, the run going on (LEAVE_ASK): the
      core could ask only once the run had stopped, and a misclick then
      Cancel would have lost it. Otherwise the core asks it, as F Q does
      (`quit` with `ask`; a question of the core's open is cancelled
      first). */
  quitAsked(): void {
    const {hello, ask, busy, computing} = this.store.getState();
    if (ask?.id === LEAVE_ASK) return;
    /* There is no loaded session to save during the startup model prompt. */
    if (!hello) {
      /* End the waiting core first: closing the OS picker first can send its
         cancellation answer ahead of Quit and turn an ordinary close into a load error. */
      this.send({cmd: 'session.quit', save: false});
      return;
    }
    if (hello && (computing || (busy && !ask))) this.store.dispatch({type: 'leave', open: true});
    else this.send({cmd: 'session.quit'});
  }

  /* the page's leave question answered: Save session stops the run, saves
     the session (and the recording in progress) and quits, all in the core
     (`quit` with `save`); Don't save quits at once (the plain quit: the
     window closes, docs/protocol.md "quit"); Cancel changes nothing */
  private leaveAnswered(fields: Record<string, unknown>): void {
    this.store.dispatch({type: 'leave', open: false});
    if (fields.key === 's') this.send({cmd: 'session.quit', save: true});
    else if (fields.key === 'd' && !closeDesktopWindow()) this.send({cmd: 'session.quit', save: false});
  }

  /* ---- plot modes: mouse, rubber and drag asks (plot/pick.ts, docs/ui-v2.md T4) ---- */

  /** the crosshair or a corner moved */
  movePick(pick: PickState): void {
    this.store.dispatch({type: 'pick', pick});
  }

  /** answers the plot mode's ask with its point or box, in data coordinates of `ranges` */
  confirmPick(pick: PickState, ranges: Ranges): void {
    const ask = this.store.getState().ask;
    if (ask && ask.id === pick.ask) this.answer(ask, pickAnswer(pick, ranges));
  }

  /** one pointer event of a drag (Window/Scroll), in data coordinates: the answer to
      the drag ask, or the next one when the core is still busy with the last */
  dragEvent(what: 'down' | 'move' | 'up', xd: number, yd: number): void {
    const ask = this.store.getState().ask, ev = {what, xd, yd};
    if (ask?.kind === 'drag' && !this.dragQueue.length) {
      this.answer(ask, ev);
      return;
    }
    const last = this.dragQueue[this.dragQueue.length - 1];
    if (what === 'move' && last?.what === 'move') this.dragQueue.pop(); /* only the latest position matters */
    this.dragQueue.push(ev);
  }

  /** Cancel, Done or Escape in a plot mode: the ask is answered as cancelled (a drag's
      next one after the events still queued, when the core is busy with the last) */
  cancelPick(): void {
    const {ask, pick} = this.store.getState();
    if (ask && (ask.kind === 'mouse' || ask.kind === 'rubber' || ask.kind === 'drag' || ask.kind === 'grab')
      && !this.dragQueue.length) {
      this.cancel(ask);
    } else if (pick) {
      this.dragEnded = true;
      this.store.dispatch({type: 'pick', pick: null});
      if (ask?.kind === 'drag') this.answer(ask, this.dragQueue.shift()!);
    }
  }

  /* ---- plot windows (docs/ui-v2.md T6): the core's Makewindow commands ---- */

  /** a window's tab picked: it is shown at once, and becomes the core's active window */
  selectWindow(win: string): void {
    const {plots, ask, busy} = this.store.getState();
    /* a prompt is the shown window's until answered */
    if (ask) return;
    /* a run draws into the core's active one (W68), and the command before
       (the last tab's own click, say) may not have ended yet: the tab
       picked last is held for the idle, never dropped */
    if (busy) {
      this.windowHeld = win;
      return;
    }
    this.windowHeld = null;
    if (plots.active === win) return;
    this.store.dispatch({type: 'selectWindow', win});
    this.send({cmd: 'window.select', window:win});
  }

  private flushWindow(): void {
    if (this.windowHeld === null || this.store.getState().busy) return;
    this.selectWindow(this.windowHeld);
  }

  renameWindow(window: string, name: string): void { this.send({cmd:'window.rename',window,name}); }

  /** Makewindow/Create: a copy of the active window, which becomes active */
  newWindow(): void {
    this.send({cmd:'window.create'});
  }

  /** Makewindow/Destroy: the active window (never window 1) */
  closeWindow(): void {
    this.send({cmd:'window.destroy',window:this.store.getState().plots.active});
  }

  /* ---- the display state (W65) ---- */

  /** the core's events without what a change of the user's on its way would undo */
  private unguarded(ev: XppEvent): XppEvent {
    if (!this.displayGuard.size) return ev;
    if (ev.ev === 'plots')
      return {...ev, windows: ev.windows.map(w => (this.displayGuard.has(w.win) ? {...w, zoom: undefined, runs: undefined} : w))};
    if (ev.ev === 'autoview') {
      const views = ev.views?.map(v => (this.displayGuard.has(v.name) ? {...v,zoom:undefined} : v));
      const guarded = [...this.displayGuard].some(k => this.store.getState().diagram.views.some(v=>v.name===k));
      return {...ev, views, show: guarded ? undefined : ev.show};
    }
    return ev;
  }

  private display(win: string, patch: DisplayPatch): void {
    this.displayGuard.add(win);
    this.displayHeld.set(win, {...this.displayHeld.get(win), ...patch});
    this.flushDisplay();
  }

  /** the oldest held change goes out when the core is not busy, else at its idle */
  private flushDisplay(): void {
    if (this.store.getState().busy) return;
    const first = this.displayHeld.entries().next();
    if (first.done) return;
    const [win, patch] = first.value;
    this.displayHeld.delete(win);
    const {x, y, runs} = patch;
    this.send(this.store.getState().diagram.views.some(v=>v.name===win) ? autoDisplayCommand(win, patch)
      : {cmd: 'window.set_display', window: win, ...(x === undefined ? {} : {x_range: x ?? []}), ...(y === undefined ? {} : {y_range: y ?? []}),
         ...(runs === undefined ? {} : {show_earlier_runs: runs})});
    this.displayGuard.add(win);
    this.displayIdles = this.idlesOwed;
  }

  /** window `win`'s zoom changed (a wheel, a drag, a typed range, Reset view) */
  /** the text with this id changed in its dialog: everything it takes, and where it is */
  changeText(id: number, v: TextValues, at: TextPosition): void {
    this.send({cmd: 'annotation.edit', annotation_id: id, text: v.text, text_size: v.size, style: v.style, color: v.color});
    this.send({cmd: 'annotation.move', annotation_id: id, new_x: at.x, new_y: at.y});
  }

  /** the annotation with this id deleted */
  deleteText(id: number): void {
    this.send({cmd: 'annotation.delete', annotation_id: id});
  }

  setViewport(win: string, viewport: Viewport): void {
    this.store.dispatch({type: 'viewport', viewport, win});
    this.display(win, rangesOf(viewport));
  }

  /** the zoom of view `view` of the AUTO diagram changed */
  setDiagramViewport(view: number, viewport: Viewport): void {
    this.store.dispatch({type: 'diagram', action: {type: 'viewport', view, viewport}});
    this.display(this.store.getState().diagram.views[view].name, rangesOf(viewport));
  }

  /** a view of the AUTO diagram clicked: the active one (W50), whose axes AUTO's Axes menu,
      zoom and a run go by */
  activateView(view: number): void {
    const d = this.store.getState().diagram;
    if (view === d.active || view >= d.views.length) return;
    this.store.dispatch({type: 'diagram', action: {type: 'activate', view}});
    this.send({cmd:'window.select',window:d.views[view].name});
  }

  /** a view of the AUTO diagram closed (the last one stays: the core refuses it) */
  closeView(view: number): void {
    this.send({cmd:'window.destroy',window:this.store.getState().diagram.views[view].name});
  }

  /** the legend's "previous runs" toggle of window `win` */
  setShowRuns(win: string, show: boolean): void {
    this.store.dispatch({type: 'showRuns', win, show});
    this.display(win, {runs: show});
  }

  /** AUTO's "Earlier branches" toggle */
  setShowEarlier(show: boolean): void {
    this.store.dispatch({type: 'diagram', action: {type: 'showEarlier', show}});
    this.display(activeView(this.store.getState().diagram).name, {show});
  }

  /* ---- Use this view (docs/ui-v2.md T9) ---- */

  /** "Use this view": the client's current zoom of `win` becomes the
      core's own axes, by the keys Window/Window (w w) and its four
      numbers, answered here. The core's `plots` and `state.view` that
      follow report the new axes; the plot's own viewport then goes back
      to "the core's axes" (store/plots.ts coreMoved, from the `state`
      reducer), with no visible jump since they are now the same range. */
  useThisView(win: string, ranges: Ranges): void {
    if (!this.mayMain('window.set_bounds')) return; /* W68 */
    const {x, y} = ranges;
    this.send({cmd:'window.set_bounds', window:win, x_min:x.min, x_max:x.max, y_min:y.min, y_max:y.max});
  }

  /** Axis labels open the existing core editor directly for their plot. */
  editPlotAxes(win: string, three: boolean): void {
    if (!this.mayMain('plot.set_2d')) return;
    this.selectWindow(win);
    this.registryAction(three ? 'plot.set_3d' : 'plot.set_2d', {window:win});
  }

  /** Window/Fit: the key sequence the classic page uses ('w' opens the
      Window submenu, 'f' is Fit) sets the active window's axes to the
      data's extent, and the plot's own pan/zoom clears at once: when the
      core's axes were already fitted its state does not move them, so
      the plot would otherwise stay where a scroll or zoom left it (T30) */
  fitView(): void {
    const p = this.store.getState().plots, w = windowOf(p, p.active);
    if (w && (w.viewport.x !== null || w.viewport.y !== null)) this.setViewport(w.win, HOME);
    this.send({cmd: 'window.fit', window: p.active});
  }

  /* ---- 3D plots (docs/ui-v2.md T14) ---- */

  /** window `win`'s 3D view turned to `theta`, `phi` (a drag or the arrow
      keys on the focused plot): the store updates at once, so the plot
      (projected in the client, plot/project3d.ts) redraws with no round
      trip. The core's own state (a PDF/SVG export, `state.view`,
      any other client) is kept in step by plot.set_3d_view, throttled to at most 10 a second while the turn continues; a trailing send 150 ms
      after the last change always lands, so the settled angle reaches the
      core even with no explicit end wired in (a key held down auto-
      repeats, with no keyup between steps). */
  rotate3d(win: string, theta: number, phi: number): void {
    this.store.dispatch({type: 'rotate3d', win, theta, phi});
    const timer = this.rotate3dTimer.get(win);
    if (timer !== undefined) clearTimeout(timer);
    const last = this.rotate3dLast.get(win) ?? -Infinity;
    const send = () => {
      this.rotate3dLast.set(win, performance.now());
      this.rotate3dTimer.delete(win);
      /* busy: held, the latest angle goes at the idle */
      if (this.store.getState().busy) this.rotate3dHeld.set(win, {theta, phi});
      else this.turn3d(win, theta, phi);
    };
    if (performance.now() - last >= 100) send();
    else this.rotate3dTimer.set(win, setTimeout(send, 150));
  }

  /* at an idle: the latest angle each window was turned to while the core was busy (rotate3d) */
  private flushRotate3d(): void {
    if (this.store.getState().busy) return;
    /* one plan at a time: the next window's angle goes at the next idle */
    const first = this.rotate3dHeld.entries().next();
    if (first.done) return;
    this.rotate3dHeld.delete(first.value[0]);
    this.turn3d(first.value[0], first.value[1].theta, first.value[1].phi);
  }

  /** window `win`'s 3D view set to `theta` and `phi` in the core (plot.set_3d_view) */
  private turn3d(win: string, theta: number, phi: number): void {
    this.send({cmd: 'plot.set_3d_view', window: win, theta, phi});
  }

  /** stops the running command; it still ends with its idle */
  abort(): void {
    this.store.dispatch({type: 'aborting'});
    this.transport.send({cmd: 'run.abort'});
  }

  /* ---- the AUTO view (docs/ui-v2.md T11a, docs/protocol.md `auto`) ---- */

  /** one of the AUTO window's buttons, sent as its key (`win` `auto`); its
      prompts come as ordinary asks. Clear also is the view's own (T21):
      the branches so far become the earlier ones, hidden until shown
      again; the core's own clear and redraw (keys `c`, `d`) follow */
  autoOp(op: AutoOp): void {
    this.registryAction(toolbarCommand('auto', op));
  }

  /** AUTO's Run (docs/protocol.md auto.run): the kind, and where it starts (a stored point's label, else the current state) */
  autoRun(args: {kind: string; label?: number; switch?: boolean; replace_diagram?: boolean}): void {
    const cmd: Command = {cmd: 'auto.run', ...args};
    if (!this.may(cmd)) return;
    this.send(cmd);
  }

  /* after every idle: the run's clock stops, a plan ends, a grabbed periodic orbit is imported */
  private afterAuto(): void {
    if (this.store.getState().diagram.run?.active)
      this.store.dispatch({type: 'diagram', action: {type: 'run', op: 'end', at: Date.now()}});
    const g = this.grabbed;
    this.grabbed = null;
    if (g !== null) this.importOrbit(g);
  }

  /* a grab took point `point`: when it is a periodic orbit AUTO stored (a
     labelled point), File/Import orbit loads it, so the main plot shows the
     limit cycle (docs/ui-v2.md T21); AUTO keeps no orbit for other points */
  private importOrbit(point: number): void {
    const {points, labels} = activeView(this.store.getState().diagram);
    const ty = points.ty[point];
    if ((ty !== 3 && ty !== 4) || points.f2[point]) return;
    if (!labels.some(l => l.point === point)) {
      this.store.dispatch({type: 'toast', kind: 'info', text: 'AUTO keeps the orbits of labelled points only: grab a '
        + 'labelled point of the periodic branch (Tab steps through them) to plot its limit cycle.'});
      return;
    }
    this.send({cmd: 'auto.load_orbit_into_browser'});
  }

  /** AUTO's settings edited in the page's forms (T22, store/autoSettings.ts):
      one `auto` `set` at once, busy or idle -- a setting (W106): during a
      run the core applies it when the run ends, to the next one */
  autoSettings(patch: AutoSettingsPatch): void {
    for (const {command} of autoSettingsCommands(patch)) this.send(command);
  }

  /** the AUTO view's close: done with it (A10). A running job is stopped
      first and the window closes at its idle, instead of the close waiting
      behind the run. */
  closeAuto(): void {
    const {busy, stopping, diagram} = this.store.getState();
    const commands = diagram.views.map(v => ({cmd: 'window.destroy', window: v.name}));
    if (!busy) {
      for (const command of commands) this.send(command);
      return;
    }
    this.afterIdle = commands;
    if (!stopping) this.abort();
  }

  /** the core's grab (docs/protocol.md `grab`): the cursor to point `point`
      of the diagram's data, and with `take` the point is taken at once */
  grabPoint(point: number, take = false): void {
    const ask = this.store.getState().ask;
    if (ask?.kind !== 'grab') return;
    if (take) this.grabbed = point;
    this.answer(ask, take ? {point, key: 'Return'} : {point});
  }

  /** the grab takes the point under its cursor (Enter) */
  grabTake(): void {
    const {ask, diagram} = this.store.getState();
    if (ask?.kind !== 'grab') return;
    this.grabbed = diagram.info?.point ?? null;
    this.answer(ask, {key: 'Return'});
  }

  /** a click on a two-parameter diagram: shown and kept as the point
      AUTO's File/sElect 2par pt uses (docs/protocol.md `auto` `point`) */
  autoPoint(x: number, y: number): void {
    this.store.dispatch({type: 'diagram', action: {type: 'stored', at: {x, y}}});
    this.send({cmd: 'auto.pick', x, y});
  }

  /** Hide AUTO hides every view of the diagram in the core (window.hide: they keep everything); Show brings them back */
  showAuto(shown: boolean): void {
    /* a grab or plot mode waiting on the diagram is cancelled with it: the ask would stay open behind
       the main window and swallow every key typed there (W100) */
    const {ask, pick, diagram} = this.store.getState();
    if (!shown && diagram.shown && ask && (diagram.grabbing || (pick && pick.win === autoWindow(this.store.getState())))) this.cancelPick();
    for (const view of diagram.views) this.send({cmd: shown ? 'window.show' : 'window.hide', window: view.name});
    if (shown && diagram.open) this.send({cmd:'window.select', window:autoWindow(this.store.getState())});
  }

  /* ---- values panel (docs/ui-v2.md T3, docs/protocol.md `set`/`slide`/`default`/`userbut`,
     GitHub #155): every edit (a field, a slider, Reset, a numerics field) is a setting (W106) and
     goes to the core at once as a `set`, busy or idle; during a computation the core applies it
     when that ends, never to the run in progress. The field shows it meanwhile (store/values.ts
     inflight): the value shown is the value. No undo (GitHub #110): Reset is the way back. ---- */

  /** edits, in one `set`: shown on their fields until its idle, which also
      ends the attribution of an error to them (A11) */
  private edit(...sets: ValueSet[]): void {
    for (const {command, sets: sent} of valueCommands(this.store.getState().hello, sets)) {
      const ahead = this.idlesOwed;
      if (!this.send(command)) continue;
      for (const set of sent) this.store.dispatch({type: 'values', action: {type: 'sent', set, ahead}});
    }
  }

  /** a parameter or initial condition box left with a new value */
  setValue(kind: 'par' | 'ic', name: string, text: string): void {
    this.edit({kind, name, text});
  }

  /** a boundary condition or delay box (by position: docs/protocol.md, BC names all read "0=") */
  setValueByIndex(kind: 'bc' | 'delay', index: number, text: string): void {
    this.edit({kind, index, text});
  }

  /** Slider definitions belong to the Session; the next state confirms the edit. */
  setSlider(id: number, def: {name: string; lo: string; hi: string; step: string}): void {
    this.send({cmd: 'value.configure_slider', slot: id - 1, name: def.name,
      low: Number(def.lo), high: Number(def.hi), step: Number(def.step)});
  }

  removeSlider(id: number): void {
    this.send({cmd: 'value.remove_slider', slot: id - 1});
  }

  /** a slider moved: sent like any other edit (W106) */
  slide(kind: 'par' | 'ic', name: string, value: number): void {
    this.edit({kind, name, text: String(value)});
  }

  /** the model file's value of a parameter or IC (null: not known) */
  defaultOf(kind: 'par' | 'ic', name: string): number | null {
    return this.store.getState().values.defaults?.[fieldKey(kind, name)] ?? null;
  }

  /** one field back to the model file's value: the core's `set.restore_defaults` with the field
      (GitHub #110: Undo value edit is the other way back) */
  resetValue(kind: 'par' | 'ic', name: string): void {
    this.send({cmd: 'set.restore_defaults', scope: RESTORE_SCOPE[kind], field: name});
  }

  /** Reset all: the core's `set.restore_defaults`, every parameter or IC of `kind` back to the model
      file's value (the core redoes the tables once) */
  defaultValues(kind: 'par' | 'ic'): void {
    this.store.dispatch({type: 'values', action: {type: 'defaulted', kind}});
    this.send({cmd: 'set.restore_defaults', scope: RESTORE_SCOPE[kind]});
  }

  /** a numerics field (the values panel's Numerics, W106): `key` as the
      `numerics` event names it, the method by its number */
  setNumeric(key: string, text: string): void {
    this.edit({kind: 'num', name: key, text});
  }

  /** Save of a section: the core writes its own .par/.ic (docs/protocol.md
      "values"; core/lunch-new.cpp io_parameter_file/io_ic_file), then, at
      the command's idle, the page offers it as a download -- the same
      `pendingSave`/`deliver` path `writeDataFile` uses (W66: the page
      itself builds no file). */
  saveValues(kind: 'par' | 'ic'): void {
    const name = this.store.getState().hello?.output_names[kind];
    if (!name) return;
    if (nativeFileDialog()) this.send({cmd: 'value.save_file', kind: VALUE_FILE[kind]});
    else {
      this.pendingSave = {name, handle: null, ahead: this.idlesOwed};
      this.send({cmd: 'value.save_file', kind: VALUE_FILE[kind], file: name});
    }
  }

  /** Load of a section (W66 review): the picked file goes into the
      model's folder through `upload`, as every upload does (the same name
      with other content asks first: Replace, Keep both, Cancel; W134), then
      the core reads it with `values` `read` (io_parameter_file/io_ic_file,
      READEM) -- at once, like File/Read set, not staged as a pending edit
      (the values panel has nothing left to parse: the file is XPP's own
      par/ic format, or nothing reads it). A bad file's `message` `error`
      (core/lunch-new.cpp err_msg, e.g. "Expected N initial conditions...")
      becomes a notification the same way any other command's does. */
  async loadValues(kind: 'par' | 'ic', file: File): Promise<void> {
    const uploads = await this.upload([{file, name: file.name}], null, true);
    if (uploads) this.send({cmd: 'value.load_file', kind: VALUE_FILE[kind], file: uploads[0].name});
  }

  /* ---- recording (W59a, docs/protocol.md "Recordings") ---- */

  /** starts recording the steps (File/recorD, the title bar's Record) */
  startRecording(): void {
    this.send({cmd: 'recording.start'});
  }

  /** Open model, from the start screen or anywhere: the core asks for the file (a `file` ask), exactly as File > Open model does
      (core/model_switch.h) */
  openModel(): void {
    this.chooseFile('session.open_model');
  }

  /** a button of its own for a command whose argument is a file: the core asks for the file through the
      picker, as the form's Choose file button does (CommandForm.tsx), without the form's extra step */
  private chooseFile(id: string, values: Record<string, unknown> = {}): void {
    const row = registryRows(this.store.getState().hello).find(row => row.id === id);
    this.send(row?.file_mode ? {cmd: id, ...values, file_picker: true} : {cmd: id, ...values});
  }

  /** Open model of a recent model (hello.start.recent), by its place in the list: the core knows the path */
  openRecent(index: number): void {
    this.send({cmd: 'session.open_model', recent: index});
  }

  /** Open model of a bundled example (hello.start.examples): the core copies it into the user's config
      folder and opens the copy; the page sends only the name */
  openExample(name: string): void {
    this.send({cmd: 'session.open_model', example: name});
  }

  /** the recording bar's Stop: the core asks the file's name (a `file` ask) and writes it */
  stopRecording(): void {
    this.send({cmd: 'recording.stop'});
  }

  /** the note shown above the next step (the recording bar's note box) */
  recordNote(text: string): void {
    this.send({cmd: 'recording.add_note', note: text});
  }

  /* ---- the player (W59b, docs/protocol.md "Playing a recording") ---- */

  /** File/plaY recording, the title bar's Play a recording: the core asks for the .recx */
  playOpen(): void {
    this.chooseFile('recording.open', {play: true});
  }

  /** Play, Pause, Step: at once, even while a step runs */
  play(op: 'start' | 'pause' | 'step'): void {
    this.send({cmd: PLAYER_CONTROLS[op]});
  }

  /** The speed the paces are divided by, within hello.player_speed. */
  playSpeed(speed: number): void {
    this.send({cmd: 'recording.set_speed', speed});
  }

  /** Restart (step 0, paused) and Play from here (the steps before it at once, then playing) */
  playFrom(step: number, play: boolean): void {
    this.send({cmd: 'recording.seek', step, play});
  }

  /** a step's note, written into the .recx (the fingerprint stays valid) */
  playNote(step: number, text: string): void {
    this.send({cmd: 'recording.set_step_note', step, note: text});
  }

  /** leaves the player; the model stays */
  playClose(): void {
    this.send({cmd: 'recording.close'});
  }

  /** an `@ button` of the ODE file */
  userButton(index: number): void {
    this.send({cmd: 'tool.run_user_button', index});
  }

  /* ---- data table (docs/ui-v2.md T10, docs/protocol.md `browser`) ---- */

  openTable(): void {
    this.store.dispatch({type: 'table', action: {type: 'open', open: true}});
  }

  /** stops the core sending further blocks (docs/protocol.md: `count` 0 stops the updates) */
  closeTable(): void {
    this.store.dispatch({type: 'table', action: {type: 'open', open: false}});
    this.send({cmd: 'browser.page', from_row: 0, count: 0});
  }

  /** the row the arrow keys, a click, Home/End move to */
  selectTableRow(row: number): void {
    this.store.dispatch({type: 'table', action: {type: 'select', row}});
  }

  /** the rows [visibleFrom, visibleFrom+visibleCount) the view can see: asks
      for a new block only when the cached one does not cover them, and
      never twice for the same block (store/table.ts planRequest). A
      `browser` request with `from` is a control line, answered at once
      even during a job or a prompt (docs/protocol.md), so this never waits
      on `state.busy`. */
  fetchTableRows(visibleFrom: number, visibleCount: number): void {
    const {table: {page, pendingKey}, hello, core} = this.store.getState();
    if (!hello) return;
    const req = planRequest(page, visibleFrom, visibleCount, hello.limits, Math.max(page?.rows ?? 0, core?.rows ?? 0));
    if (!req) return;
    const key = JSON.stringify(req);
    if (key === pendingKey) return;
    this.store.dispatch({type: 'table', action: {type: 'requested', req}});
    this.send({cmd: 'browser.page', from_row: req.from, count: req.count, first_column: req.col, columns: req.ncol});
  }

  /** one of the browser's buttons, on the selected row (docs/protocol.md
      `browser` op; Find, Replace, Table, Load and Write prompt through the
      ordinary `ask`, AskDialog already shows) */
  browserOp(op: BrowserOp): void {
    const row = this.store.getState().table.selected;
    const command = toolbarCommand('browser', op);
    if (['first', 'last'].includes(op)) this.send({cmd:command,row});
    else this.registryAction(command);
  }

  /** Get: the selected row becomes the initial conditions (the next `state` has them) */
  getRow(): void {
    this.send({cmd: 'ic.from_data_row', row: this.store.getState().table.selected});
  }

  /** Save data (docs/protocol.md "Saving data"): the core itself writes
      `what` (`table` or `plot`) as `format` into `name` in the model's
      folder (browse_data.cpp data_write, skipping the format/name asks
      since both are given), then, at the command's idle, the page offers
      it as a download -- the same `pendingSave`/`deliver` path a `file`
      ask's Write uses (W66: the page itself builds no file). */
  writeDataFile(what: 'table' | 'plot', format: string, name?: string): void {
    name ??= this.store.getState().hello?.output_names[what === 'plot' ? 'curves' : 'csv'];
    if (!name) return;
    const target = what === 'plot' ? 'plot_curves' : 'table';
    if (nativeFileDialog()) this.send({cmd: 'browser.export', target, format});
    else {
      this.pendingSave = {name, handle: null, ahead: this.idlesOwed};
      this.send({cmd: 'browser.export', target, format, file: name});
    }
  }

  /* ---- animation (docs/ui-v2.md T13, docs/protocol.md `ani` and "The animation as data") ---- */

  /** shows the panel, opening the core's animation window first (Viewaxes/Toon) when there is none */
  openAni(): void {
    if (this.store.getState().ani.exists) {
      this.send({cmd: 'window.show', window: this.store.getState().hello?.window_names.ani});
      return;
    }
    this.store.dispatch({type: 'ani', action: {type: 'open', open: true}});
    this.send({cmd: 'animation.open'});
  }

  /** hides the panel and the core's window (window.hide); a playing animation stops */
  closeAni(): void {
    this.aniPause();
    if (this.store.getState().ani.exists) this.send({cmd: 'window.hide', window: this.store.getState().hello?.window_names.ani});
    else this.store.dispatch({type: 'ani', action: {type: 'open', open: false}});
  }

  /** File: an .anix file, through the file ask (the browser's open dialog) */
  aniLoad(): void {
    this.chooseFile('animation.load');
  }

  /** Go: plays from the core's position to the last frame; never started by the page itself (A6) */
  aniPlay(): void {
    if (!this.store.getState().ani.playing) this.send({cmd: 'animation.play'});
  }

  /** Pause: reaches the running Go at once (a control line: no idle of its own) */
  aniPause(): void {
    if (this.store.getState().ani.playing) this.send({cmd: 'animation.pause'});
  }

  /** n frames from the frame shown (a playing animation pauses there first) */
  aniStep(n: number): void {
    const ani = this.store.getState().ani, target = stepTarget(ani, n);
    if (ani.playing) this.aniSeek(target);
    else this.send({cmd: 'animation.step', rows: target - ani.pos});
  }

  /** the frame of stored row `pos` */
  aniSeek(pos: number): void {
    this.aniPause();
    this.send({cmd: 'animation.seek', row: Math.max(0, Math.round(pos))});
  }

  /** the delay between two frames of Go, ms (reaches a running Go at once) */
  aniSpeed(ms: number): void {
    this.send({cmd: 'animation.set_speed', ms: Math.max(0, Math.round(ms))});
  }

  /** Grab: the frame's grab points wait for the pointer */
  aniGrab(): void {
    this.send({cmd:'animation.grab_point'});
  }

  /** a drag on the picture while grabbing, from the press to the release, both in the frame's unit coordinates
      (u, v: y up) of its dimension box `dim` (xlo, ylo, xhi, yhi): the core wants the animation's own x, y */
  aniGrabPoint(from: [number, number], to: [number, number], dim: [number, number, number, number]): void {
    const at = ([u, v]: [number, number]): [number, number] => [dim[0] + u * (dim[2] - dim[0]), dim[1] + v * (dim[3] - dim[1])];
    const [x, y] = at(from), [toX, toY] = at(to);
    this.send({cmd: 'animation.grab_point', x, y, to_x: toX, to_y: toY});
  }

  /* ---- kinescope (docs/ui-v2.md T15, docs/protocol.md `film` and `pixels`) ----
     The core's Kinescope menu (keys k then c/r/p/a/s/m) decides when a frame
     is captured or reset, and when a play or autoplay starts; store/state.ts's
     reducer keeps the frames, captured as data (store/kinescope.ts
     snapshotWindow) the moment a `capture` event arrives. What this session
     owns is the two things a reducer cannot: the playback clock (`onFilm`
     below, an ordinary timer, since the core sent nothing further once it
     said "play" - docs/protocol.md's do_movie_com case 2/3 ends the command
     at once) and answering a `pixels` ask by rendering the picture the ask
     is about (plot/kinescopeRender.ts), never by taking a screenshot. */

  /** a play or autoplay: shows the frames stored so far, `delay` ms apart,
      once (`play`) or `cycles` times (`autoplay`); Stop below ends it early */
  private onFilm(ev: FilmEvent): void {
    if (ev.op !== 'play' && ev.op !== 'autoplay') return; /* capture/reset: store/state.ts already updated the frames */
    this.stopFilmTimer();
    const total = Math.min(ev.count, this.store.getState().kinescope.frames.length);
    if (total <= 0) return;
    const cycles = ev.op === 'autoplay' ? Math.max(1, ev.cycles) : 1;
    const delay = Math.max(0, ev.delay);
    let shown = 0, cycle = 0;
    const step = () => {
      this.store.dispatch({type: 'kinescope', action: {type: 'show', index: shown}});
      shown++;
      if (shown >= total) {
        shown = 0;
        cycle++;
        if (cycle >= cycles) {
          this.filmTimer = null;
          this.store.dispatch({type: 'kinescope', action: {type: 'playing', playing: false}});
          return;
        }
      }
      this.filmTimer = setTimeout(step, delay);
    };
    step();
  }

  private stopFilmTimer(): void {
    if (this.filmTimer !== null) {
      clearTimeout(this.filmTimer);
      this.filmTimer = null;
    }
  }

  kinescopeCapture(): void {
    this.send({cmd: 'kinescope.capture'});
  }

  kinescopeReset(): void {
    this.send({cmd: 'kinescope.reset'});
  }

  kinescopePlay(): void {
    this.send({cmd: 'kinescope.play'});
  }

  /** Stop: a client-only action (the core's own command already ended) */
  kinescopeStop(): void {
    this.stopFilmTimer();
    if (this.store.getState().kinescope.playing) this.store.dispatch({type: 'kinescope', action: {type: 'playing', playing: false}});
  }

  /** a `pixels` ask (docs/protocol.md): a kinescope frame's picture when
      `film` is given, else window `win`'s (its live chart when it is on
      screen, else an offscreen render of the same data); cancelled when
      there is nothing to render, as before this task */
  private answerPixels(ask: AskEvent): void {
    const dark = document.documentElement.dataset.theme === 'dark';
    let pixels: {w: number; h: number; rgb: Uint8ClampedArray} | null = null;
    if (typeof ask.film === 'number') {
      const frame = this.store.getState().kinescope.frames[ask.film];
      if (frame) pixels = renderFrame(frame, dark);
    } else if (typeof ask.win === 'string') {
      pixels = chartOf(ask.win)?.pixels() ?? null;
      if (!pixels) {
        const w = windowOf(this.store.getState().plots, ask.win);
        if (w) pixels = renderFrame(snapshotWindow(w), dark);
      }
    }
    if (pixels) this.answer(ask, {w: pixels.w, h: pixels.h, rgb: bytesToBase64(pixels.rgb)});
    else this.cancel(ask);
  }

  /** Make Anigif (Kinescope's own menu item, k then m): the core writes
      the chosen GIF (json_windows.cpp j_movie_make_anigif), asking
      `pixels` with `film` for every captured frame (answerPixels above);
      the page only offers the result as a download, at the idle that
      follows (W66: the page built the GIF itself before this task). */
  downloadKinescopeGif(): void {
    if (!this.mayMain('kinescope.capture') || !this.store.getState().kinescope.frames.length) return;
    this.chooseFile('kinescope.export_gif');
  }

  /* ---- text views (docs/ui-v2.md T16, docs/protocol.md `equations`, `source`, ---- */
  /* `action`, `equilibrium`): equations, the source with its comment
     actions, and the last Sing pts equilibrium */

  /** opens the panel (R6: a side panel from 48rem, a sheet under that), on
      `tab` when given, and asks the core to (re)send that tab's data: the
      three are always fetched fresh, since the model or the ICs may have
      changed since they were last shown */
  openText(tab?: TextTab): void {
    const cur = this.store.getState().text;
    const next = tab ?? cur.tab;
    this.store.dispatch({type: 'text', action: {type: 'open', open: true}});
    if (next !== cur.tab) this.store.dispatch({type: 'text', action: {type: 'tab', tab: next}});
    this.refreshText(next);
  }

  closeText(): void {
    this.store.dispatch({type: 'text', action: {type: 'open', open: false}});
  }

  /** switches the panel's tab and asks the core for that tab's data */
  selectTextTab(tab: TextTab): void {
    if (tab === this.store.getState().text.tab) return;
    this.store.dispatch({type: 'text', action: {type: 'tab', tab}});
    this.refreshText(tab);
  }

  private refreshText(tab: TextTab): void {
    /* the model's source and its equations come together */
    if (tab === 'equations' || tab === 'source') this.send({cmd: 'session.get_source'});
    /* 'equilibrium' has no fetch of its own: it is the last Sing pts result
       the core sent (Sing pts/Go, session.findEquilibrium()) and stays
       shown until the next one */
  }

  /** run the action of comment `index` of the source's comments
      (docs/protocol.md `action`): a comment with a `{par=value,...}` block,
      shown as a button on its line (ui/TextViews.tsx) */
  runAction(index: number): void {
    this.send({cmd: 'tool.run_source_action', index});
  }

  /** Find the equilibrium from the current initial conditions. */
  findEquilibrium(): void {
    this.send({cmd: 'equilibrium.find', print_eigenvalues: false});
  }

  /** the equilibrium window's Import: the last equilibrium becomes the
      initial conditions (the next `state` has them) */
  importEquilibrium(): void {
    this.send({cmd: 'equilibrium.import_as_ic'});
  }

  /* ---- files (docs/ui-v2.md section 4, T5): the model's folder is the workspace ---- */

  private failed(text: string): void {
    this.store.dispatch({type: 'toast', kind: 'error', text});
  }

  /** files into the model's folder: every upload of the page comes here (a
      `file` ask's picks, Values > Load, a notification's Add file…; W134).
      Each name is checked first (the core's rule, safeName) and each size
      against the core's cap; then, against the folder's listing, a file
      already there with the same content is not copied again, and a name
      taken by other content is copied only after the confirm: Replace, Keep
      both (name-2.ext, when `keepBoth`) or Cancel, shown in the dialog of
      `ask`, or in one of its own when null. Whether a copy may land now is
      the core's (never during a computation, docs/protocol.md "Files"): a
      refusal fails it like any other. Resolves what each file became, in
      order, or null when cancelled at the confirm or failed, which a
      notification reports. */
  async uploadArgument(file: File): Promise<string | null> {
    const uploaded = await this.upload([{file, name: file.name}], null, true);
    return uploaded?.[0]?.name ?? null;
  }

  private async upload(files: {file: File; name: string}[], ask: number | null, keepBoth: boolean): Promise<Upload[] | null> {
    if (!this.files || !files.length) return null;
    try {
      const listing = await this.files.list();
      this.store.dispatch({type: 'files', action: {type: 'listing', files: listing}});
      const taken = new Set(listing.map(f => f.name)), uploads: Upload[] = [];
      for (const {file, name: wanted} of files) {
        if (!safeName(wanted)) {
          this.failed(`XPP cannot use a file named “${wanted}” in the model's folder.`
            + (wanted === file.name ? ' Rename it and pick it again.' : ''));
          return null;
        }
        const big = this.tooBig(file);
        if (big) {
          this.failed(big);
          return null;
        }
        const sha256 = await sha256Hex(file);
        const plan = uploadPlan(wanted, sha256, listing);
        let name = wanted;
        if (plan === 'confirm') {
          const other = keepBoth ? keepBothName(wanted, taken) : null;
          const choice = await this.confirmReplace(ask, wanted, other);
          if (choice === 'cancel') return null;
          if (choice === 'keep' && other) name = other;
        }
        if (plan !== 'same') await this.files.put(name, file);
        taken.add(name);
        uploads.push({picked: file.name, name, sha256, copied: plan !== 'same'});
      }
      this.store.dispatch({type: 'files', action: {type: 'uploaded', uploads}});
      return uploads;
    } catch (e) {
      this.failed(e instanceof Error ? e.message : String(e));
      return null;
    }
  }

  /** the files picked for a `file` ask for reading, uploaded (`upload`),
      then the ask answered with the name its pattern matches. Resolves
      false when nothing was answered: cancelled at the confirm, or a
      failure, which a notification reports. */
  async openFiles(ask: AskEvent, picked: File[]): Promise<boolean> {
    const uploads = await this.upload(picked.map(file => ({file, name: file.name})), ask.id, true);
    if (!uploads) return false;
    if (this.store.getState().ask?.id !== ask.id) return false; /* the prompt went meanwhile */
    const chosen = answerName(uploads.map(u => u.picked), ask.wild);
    this.answer(ask, {file: uploads.find(u => u.picked === chosen)!.name});
    return true;
  }

  /** the folder has `name` with other content: Replace, Keep both (when
      `keepBoth` names it) or Cancel (the dialog asks) */
  private confirmReplace(ask: number | null, name: string, keepBoth: string | null): Promise<ReplaceChoice> {
    this.replaceChoice?.('cancel');
    this.store.dispatch({type: 'files', action: {type: 'confirm', confirm: {ask, name, keepBoth}}});
    return new Promise(resolve => {
      this.replaceChoice = resolve;
    });
  }

  /** a `file` ask in the desktop window (W88): the operating system's own
      dialog, answered with the full path picked, no copy (the core reads
      and writes it where it is); Cancel there cancels the ask, as the
      page's dialog's Cancel does. A dialog that could not open says so and
      cancels. */
  async nativeFile(ask: AskEvent): Promise<void> {
    let path: unknown = null;
    try {
      const request = nativeFileRequest(ask);
      if (ask.mode === 'write' && this.retrySaveDir !== null) {
        request.dir = this.retrySaveDir;
        this.retrySaveDir = null;
      }
      path = await nativeFileDialog()!({...request, ask: ask.id});
    } catch (e) {
      this.failed(`The file dialog could not open: ${e instanceof Error ? e.message : String(e)}`);
    }
    if (this.store.getState().ask?.id !== ask.id) return; /* the prompt went meanwhile */
    if (typeof path === 'string' && path) {
      if (ask.mode === 'write') {
        this.nativeSave = path;
        const run = this.store.getState().files.run;
        this.nativeRun = run ? {...run, answers: run.answers.slice()} : null;
        this.nativeSaveDir = ask.save_dir ?? '';
      }
      this.answer(ask, ask.mode === 'write' ? {file: path, replace: 1} : {file: path});
    } else this.cancel(ask);
  }

  /** the user's answer to the replace confirm */
  resolveReplace(choice: ReplaceChoice): void {
    const done = this.replaceChoice;
    this.replaceChoice = null;
    this.store.dispatch({type: 'files', action: {type: 'confirm', confirm: null}});
    done?.(choice);
  }

  /** a `file` ask for writing answered with `name`: the core writes it into
      the model's folder, then, at the command's idle, the page copies it to
      `handle` (showSaveFilePicker's) or offers it as a download */
  saveFile(ask: AskEvent, name: string, handle: SaveHandle | null): void {
    /* the ask belongs to the command running now: its idle is the next one */
    const suffix = ask.wild?.match(/^\*(\.[^*? /\\]+)$/)?.[1];
    this.pendingSave = {name, handle, ahead: 0, suffix};
    this.answer(ask, {file: name});
  }

  private async deliver(name: string, handle: SaveHandle | null, snapshot: Promise<Blob | null>): Promise<void> {
    try {
      const data = await snapshot;
      if (!data) return; /* not written: the command stopped before it */
      if (handle) await writeTo(handle, data);
      else offerDownload(name, data);
      const how = handle ? 'picker' as const : 'download' as const;
      this.store.dispatch({type: 'files', action: {type: 'offered', offered: {name, size: data.size, sha256: await sha256Hex(data), how}}});
    } catch (e) {
      this.failed(`${name} is in the model's folder, but copying it failed: ${e instanceof Error ? e.message : String(e)}`);
    }
  }

  /** why `file` is too large for the model's folder (hello.limits.upload, the core's cap), or null */
  private tooBig(file: File): string | null {
    const hello = this.store.getState().hello;
    if (!hello) throw new Error("File upload before hello");
    return file.size > hello.limits.upload
      ? `${file.name}: ${hello.upload_error}`
      : null;
  }

  /** "Add file…" of a notification: `file` goes into the model's folder
      under the name the core could not open (`upload`, no Keep both: the
      core wants that name), and the command runs again */
  async addMissingFile(toastId: number, file: File): Promise<void> {
    const action = this.store.getState().toasts.find(t => t.id === toastId)?.action;
    if (!action || !(await this.upload([{file, name: action.name}], null, false))) return;
    this.store.dispatch({type: 'dismiss', id: toastId});
    const {run} = action, state = this.store.getState();
    if (!run || state.busy) {
      this.store.dispatch({type: 'toast', kind: 'info', text: `${action.name} is in the model's folder now. Run the command again.`});
      return;
    }
    this.replayAnswers = run.answers.slice();
    this.replayIdles = 1;
    this.send(run.cmd);
  }

  /* ---- array plot (docs/ui-v2.md T12, docs/protocol.md `aplot`) ---- */

  /** shows the panel; the core's array plot window (105) is created through
      its own menu (Window/zoom, Axes, Array: `v` then `a`, docs/ui-v2.md
      section 3), which also pops the Edit form the first time -- both
      already generic (AskDialog answers a `menu` ask, then a `form` one).
      Once the window exists this only asks for a fresh picture. */
  openAplot(): void {
    const {windowOpen} = this.store.getState().aplot;
    if (windowOpen) {
      this.send({cmd: 'window.show', window: this.store.getState().hello?.window_names.aplot});
      this.aplotOp('redraw');
      return;
    }
    this.store.dispatch({type: 'aplot', action: {type: 'panel', open: true}});
    this.send({cmd: 'array_plot.open'});
    this.aplotOp('edit'); /* the first time, the Edit form too */
  }

  /** hides the panel and the core's window (window.hide) */
  closeAplot(): void {
    if (this.store.getState().aplot.windowOpen) this.send({cmd: 'window.hide', window: this.store.getState().hello?.window_names.aplot});
    else this.store.dispatch({type: 'aplot', action: {type: 'panel', open: false}});
  }

  /** the classic array plot window's own buttons: Redraw, Edit (a form,
      AskDialog), Fit, Range, Print, GIF (a file ask, FileDialog), Close */
  aplotOp(op: AplotOp): void {
    this.registryAction(toolbarCommand('aplot', op));
  }

  /** the picture scrolled through time by dragging, wheeling or a keyboard
      step (plot/aplotScroll.ts): several gestures while the core is busy
      collapse into the one `scroll` it can act on next */
  aplotScroll(dy: number): void {
    const {send, pending} = accumulateScroll(this.pendingAplotDy, dy, this.store.getState().busy);
    this.pendingAplotDy = pending;
    const first = this.store.getState().aplot.event?.first_row;
    if (send && first !== undefined) this.send({cmd: 'array_plot.scroll', first_row: Math.max(0, first - send)});
  }

  /** a key the array plot's own hotkeys use (ui/AplotView.tsx); false when
      `key` is not one of them, so the caller can fall through to XPP's own */
  aplotKeyScroll(key: string): boolean {
    const dy = keyScroll(key, this.store.getState().aplot.event?.ny ?? 1);
    if (dy === null) return false;
    this.aplotScroll(dy);
    return true;
  }

  /** the session's colour scale, by name: the core's, so the pictures it writes use it too */
  setColormap(name: string): void {
    this.send({cmd: 'plot.set_colormap', colormap: name});
  }

  /** the cell under the pointer, or null off the grid */
  aplotHover(hover: AplotHover | null): void {
    this.store.dispatch({type: 'aplot', action: {type: 'hover', hover}});
  }
}
