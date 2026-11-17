import {splitDrawn} from './store/splitView';
import {panelSpec, toolbarCommand} from './ui/navigation';
import {registryRows} from './ui/navigation';
/* The session: connects a transport to the store and is the one place that
   sends commands. Components call its methods, never the transport. */
import {nativeFileDialog, nativeFileRequest, offerDownload, pickStem, releaseFolder} from './pickers';
import {EXPORT_TITLE, defaultFileName, exportDialogTitle, exportRows, folderOf, joinFolder, restem, typedStem, type ExportRow} from './exportSeveral';
import {doneMessage, endLine, startLine, stepCommand, stoppedMessage, type BatchStep} from './batch';
import {pinSteps, stepRun} from './pins';
import type {BatchAction, BatchOutcome} from './store/batch';
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
import {kindOf, mayStart, type CommandRow, type LayerWindow} from './protocol/kinds';
import type {SettingsValues} from './store/settings';
import {PROTOCOL, type AskEvent, type Command, type FilmEvent, type PageSettings, type SettingsInfo, type UserKeymap, type XppEvent} from './protocol/types';
import type {AplotHover} from './store/aplot';
import {activeView, autoxWindow} from './store/diagram';
import {
  answerName, baseName, keepBothName, safeName, uploadPlan, type ReplaceChoice, type RunAnswer, type RunRecord, type Upload,
} from './store/files';
import {createStore, type Store} from './store/store';
import {initialState, LEAVE_ASK, noDone, type LeaveThen, reduce, type Action, type AppState, type ValuesFileOffer, type ValuesPart} from './store/state';
import {checkUpdates, newerThanRunning, RELEASE_PREFIX, type UpdateResult} from './help/updates';
import {closeDesktopWindow} from './desktop';
import {stepTarget} from './store/ani';
import {snapshotWindow} from './store/kinescope';
import {planRequest} from './store/table';
import {continuePlan, steadyRun} from './store/steady';
import type {DockTab} from './store/dock';
import type {TextTab} from './store/text';
import {PopOuts, popOutRequest, type ViewKey} from './popout';
import {fieldKey, valueCommands, type ValueSet} from './store/values';
import {autoxSettingsCommands, commandSettings, type AutoxSettingsPatch} from './store/autoxOptions';
/** a command's done as the transport delivers it (File > Export > Several awaits its own by id) */
type DoneEvent = Extract<XppEvent, {ev: 'done'}>;

/** the data table's buttons (docs/protocol.md data.* commands; web/xpp-client.js's BROWSER_BUTTONS) */
export type DataOp = 'find' | 'replace' | 'unreplace' | 'table' | 'load' | 'first' | 'last'
  | 'restore' | 'addcol' | 'delcol' | 'transpose';

/** the autoX window's buttons (toolbarCommand names the registry command of each); Close is session.closeAutox */
export type AutoxOp = 'axes' | 'numerics' | 'run' | 'grab' | 'usr' | 'clear' | 'redraw';

/** a change of what a window displays, as the `display` command carries it: the zoom of each axis
    ([low, high] or null), the earlier runs' toggle (`runs`), autoX's earlier branches' toggle (`show`) */
interface DisplayPatch {
  x?: [number, number] | null;
  y?: [number, number] | null;
  runs?: boolean;
  show?: boolean;
}

/** the display key of view k of the autoX diagram: 0, -1, -2 ... (the plot windows are 1 up) */
/** the scope of the core's `set.restore_defaults` for a values section (docs/protocol.md) */
const RESTORE_SCOPE = {par: 'parameters', ic: 'initial'} as const;
/** the kind of file the core's `value.import_file` takes for a values section */
const VALUE_FILE = {par: 'parameters', ic: 'initial_conditions'} as const;



/** a view's display change as autox.set_display: [] clears an axis' zoom, show_earlier is the earlier branches' toggle */
const autoxDisplayCommand = (view: string, p: DisplayPatch): Command => ({
  cmd: 'autox.set_display', view,
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
export type AplotOp = 'redraw' | 'edit' | 'fit' | 'range';

/** how long the diagram's live points wait for more to arrive before one draw shows them all: five draws a second keep a live diagram moving */
const DIAGRAM_HOLD_MS = 200;

/** the commands of the player's Play, Pause and Step buttons */
const PLAYER_CONTROLS = {start: 'recording.play', pause: 'recording.pause', step: 'recording.step'} as const;

/** the table, as a pop-out names it */
const TABLE_VIEW: ViewKey = {kind: 'dock', name: 'table'};

/** an update's restart or install given its save: the leave question's answer, which may go while a computation runs (the core
    stops it, then runs the command, W689) */
const answeredSwitch = (cmd: Command): boolean => (cmd.cmd === 'update.restart' || cmd.cmd === 'update.install') && cmd.save !== undefined;

/* the page's settings keys (settings.json's page group) and the wait after a dock drag before its width is saved: a pause of the
   user's hand, not a check's wait */
const PAGE_KEYS = ['theme', 'dock_hidden', 'dock_width', 'folds'] as const;
const PAGE_SETTLE_MS = 300;

/** one step of a batch in the runner: the step, the file name to ask about, whether the browser downloads its file (the core asks where, the
    page answers with the name), and (once asked) whether its file exists and whether the answer skips it */
interface BatchPlan {
  step: BatchStep;
  name: string;
  download?: boolean;
  exists?: boolean;
  skip?: boolean;
}

/** how a batch ends: whether its window reports (else one message), whether the first failure ends it, and what runs after it */
interface BatchOptions {
  report: boolean;
  stopOnFailure: boolean;
  after?: () => void;
}

export class Session {
  readonly store: Store<AppState, Action>;
  /** what the leave question's answer goes with (leaveQuestion) */
  private leaveThen: LeaveThen = 'session.quit';
  /** the views in windows of their own (popout.ts) */
  readonly popouts: PopOuts;
  private otherProtocol = false;
  /** commands whose completion has not arrived yet */
  private donesOwed = 0;
  /** W622: the page's opening checked for an update (once a page, not at each model's hello; a pop-out never: its main window does) */
  private updateChecked = false;
  /** a planned dialogue (T21: the axis dialog, autoX options save and load):
      the asks its commands open are answered by these steps in order, until
      the commands' dones */

  /** the point a grab took, until its command's done (a periodic one's orbit is then imported) */
  private grabbed: number | null = null;
  /** pointer events of a drag made while the core was not asking (docs/protocol.md
      `drag`: it asks again after each one), and whether the drag has ended */
  private dragQueue: Record<string, unknown>[] = [];
  private dragEnded = false;
  /** a file ask for writing answered: the file to hand to the browser once the command is done */
  /* a file the core writes, offered at its command's done: `ahead` counts the
     dones of the commands sent before it, which come first (W95's click right
     behind a redraw delivered it at the redraw's done, before it was written) */
  private savedOutputs: {name: string; data: Promise<Blob | null>; announce?: boolean}[] = [];
  private savingReads = new Set<Promise<Blob | null>>();
  private nativeSave: string | null = null;
  private nativeRun: RunRecord | null = null;
  private retrySave: RunRecord | null = null;
  private retrySaveDir: string | null = null;
  private nativeSaveDir = '';
  private pendingSave: {name: string; ahead: number; scratch?: boolean} | null = null;
  /** the file names of a batch step's download (a browser's Export several), in the order the asks come (browserSave) */
  private downloadNames: string[] = [];
  /** the command sent last (an answer is none): the one whose file question is asked, for its default name (askedName) */
  private lastCommand: Command | null = null;
  /** File > Export > Several: whether the window granted a folder (a browser has none: its files are downloads) */
  private exportDesktop = false;
  /** the batch waiting for the replace answer (batchStart), and whether Stop was pressed during the one that runs */
  private batchPending: {title: string; plan: BatchPlan[]; options: BatchOptions} | null = null;
  private batchStopped = false;
  private exportWaits = new Map<string, (ev: DoneEvent | null) => void>();
  private callSerial = 0;
  /** the answer to the replace confirm, when it is open */
  private replaceChoice: ((c: ReplaceChoice) => void) | null = null;
  /** a command run again after "Add file…": the answers its prompts get, and
      the dones to wait for (the menu keys before it, then its own) */
  private replayAnswers: RunAnswer[] = [];
  /** rotate3d's throttle: the last time the 3d-params turn went out for a window,
      and its pending trailing send, by window */
  private rotate3dLast = new Map<string, number>();
  private rotate3dTimer = new Map<string, ReturnType<typeof setTimeout>>();
  /** the last angle a window was turned to while the core was busy, sent at its done */
  private rotate3dHeld = new Map<string, {theta: number; phi: number}>();
  /** what the page displays is the core's (W65, docs/protocol.md "Display state"): the zoom
      shown, the earlier runs' toggle, autoX's shown branches. The page keeps a copy it changes
      at once (a drag cannot wait for a round trip) and tells the core; the core's events
      set it back, except for a window whose change is held or on its way: `displayHeld` is the
      latest change of each (window 0: the autoX diagram) not yet sent, `displayGuard` the
      windows whose zoom and toggles the core's events leave alone, until the done of the
      command that carried the change (`displayDones` counts down to it). */
  private displayHeld = new Map<string, DisplayPatch>();
  private displayGuard = new Set<string>();
  private displayDones = 0;
  /** the tab last picked while the core was busy, shown and sent at its done */
  private windowHeld: string | null = null;
  private replayDones = 0;
  /** a command to send when the running one has ended (the autoX view's close while busy, A10) */
  private afterDone: Command[] | null = null;
  /** `redraw` was sent for the autoX diagram's data, not yet answered */
  private diagramAsked = false;
  /** the array plot's scroll made while the core was busy (plot/aplotScroll.ts) */
  private pendingAplotDy = 0;
  /** the kinescope's play/autoplay clock (store/kinescope.ts: the core only
      says a play started; stepping the frames shown is this session's job) */
  private filmTimer: ReturnType<typeof setTimeout> | null = null;

  /** the autoX diagram's live points and the jobs' progress wait here and go in as one batch, DIAGRAM_HOLD_MS after the first
      (W676): a run sends them at 140 000 points a second, drawing a diagram of 500 000 points takes a second, and the browser
      draws a frame between any two events, so a draw per event left the page minutes behind the core, Stop's join queued
      behind it. Any other event goes in after them, so the order stays the core's. */
  private diagramHeld: XppEvent[] = [];
  private diagramTimer: ReturnType<typeof setTimeout> | null = null;

  /** files: the model's folder over HTTP (none in unit tests) */
  constructor(private readonly transport: Transport, private readonly files: FilesApi | null = null) {
    this.store = createStore(reduce, initialState);
    this.popouts = new PopOuts(this.store);
  }

  start(): void {
    /* a command the server refused or that never reached it is an error
       as the core's own are (Messages, the error dialog): never lost (W116) */
    this.transport.open(ev => this.receive(ev), open => this.store.dispatch({type: 'connection', open}),
      error => this.store.dispatch({type: 'event', ev: {ev: 'session.message', error}}));
  }

  private holdDiagram(ev: XppEvent): void {
    this.diagramHeld.push(ev);
    this.diagramTimer ??= setTimeout(() => this.flushDiagram(), DIAGRAM_HOLD_MS);
  }

  private flushDiagram(): void {
    if (this.diagramTimer !== null) clearTimeout(this.diagramTimer);
    this.diagramTimer = null;
    const held = this.diagramHeld.splice(0);
    if (held.length) this.store.batch(() => held.forEach(ev => this.store.dispatch({type: 'event', ev: this.unguarded(ev)})));
  }

  private receive(ev: XppEvent): void {
    /* core and page ship together (the page is compiled into the program):
       a page of another build is a shown error, never read as this one */
    if (ev.ev === 'hello') this.otherProtocol = ev.protocol !== PROTOCOL;
    if (this.otherProtocol) {
      if (ev.ev === 'hello')
        this.store.dispatch({type: 'event', ev: {ev: 'session.message', error: `This page speaks protocol ${PROTOCOL} and the program ${ev.protocol}: `
          + 'open the page the program itself serves (reload it after an update).'}});
      return;
    }
    if (ev.ev === 'hello') {
      setLineGeometry(ev.line_styles); /* before any plot is built */
      setAnnotationGlyphs(ev.annotation_glyphs);
    }
    if (ev.ev === 'autox.jobs' || (ev.ev === 'autox.diagram' && ev.op === 'add')) {
      this.holdDiagram(ev);
      return;
    }
    this.flushDiagram(); /* what came before goes in first: the order is the core's */
    this.store.dispatch({type: 'event', ev: this.unguarded(ev)});
    if (ev.ev === 'hello') this.adoptPage(ev.settings);
    if (ev.ev === 'hello') {
      this.displayHeld.clear();
      this.displayGuard.clear();
      this.displayDones = 0;
      this.donesOwed = 0;
      /* the plots as data (docs/protocol.md): every data event the server
         speaks (hello.features), asked for on every (re)connection, which
         also makes the server send the windows, their series, nullclines,
         direction fields and marks; values as base64 float32, which a long
         run needs */
      this.send({cmd: 'events.subscribe', events: ev.features, encoding: 'f32'});
      if (!this.updateChecked && popOutRequest(globalThis.location?.hash ?? '') === null) {
        this.updateChecked = true;
        /* W622: the opening's check for a newer release, when the core says one is due (setting on, none today) */
        this.send({cmd: 'update.due'});
        /* and an installer's file an earlier download kept (Later) is offered again until it is installed */
        this.send({cmd: 'update.pending'});
      }
    } else if (ev.ev === 'kinescope.film') {
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
      else if (ev.kind === 'file' && ev.mode === 'write') this.browserSave(ev);

    } else if (ev.ev === 'session.message' && ev.error?.includes('Permission denied:') && this.nativeRun) {
      this.retrySave = this.nativeRun;
      this.retrySaveDir = this.nativeSaveDir;
      this.nativeRun = null;
    } else if (ev.ev === 'session.saved' && !this.pendingSave?.scratch) {
      if (ev.saved && safeName(ev.file) && ev.file !== this.nativeSave && this.files) {
        /* Start reading at commit; the next answer waits for this snapshot. */
        const data = this.files.get(ev.file).catch(e => {
          this.failed(`${ev.file} could not be read: ${e instanceof Error ? e.message : String(e)}`);
          return null;
        });
        this.savingReads.add(data);
        void data.then(() => this.savingReads.delete(data));
        this.savedOutputs.push({name: ev.file, data});
      } else if (ev.saved && ev.file && ev.file !== this.nativeSave) {
        /* written where the page cannot copy it from (a Play's model folder, a full path): the message names where it is (W786) */
        this.store.dispatch({type: 'toast', kind: 'info', text: `Saved ${ev.file}`});
      }
      this.nativeSave = null;
    } else if (ev.ev === 'done') {
      const waiting = ev.id === undefined ? undefined : this.exportWaits.get(ev.id);
      if (waiting) { this.exportWaits.delete(ev.id!); waiting(ev); }
      /* `set.list_file_contents` (a section's Load from file, or File > Sets > Load from file): its done lists the parts to offer */
      const parts = ev.status === 'ok' ? ev.result?.parts : null;
      if (Array.isArray(parts))
        this.store.dispatch({type: 'valuesFile', offer: {file: String(ev.result?.file), scope: String(ev.result?.scope), format: String(ev.result?.format), parts: parts as ValuesPart[]}});
      /* `update.due` answered: when it is due the check runs; a newer release is a quiet notice, a failure only a log line */
      if (ev.status === 'ok' && ev.result?.due === true) void this.automaticUpdateCheck();
      this.updateAnswered(ev);
      /* `options.get` answered: the group's values for every editor of its rows; a group's options applied: its values again, as the core holds them */
      if (ev.status === 'ok' && typeof ev.result?.group === 'string') {
        if (ev.result.values) this.store.dispatch({type: 'optionValues', group: ev.result.group, values: {values: ev.result.values as Record<string, string>,
          unavailable: ev.result.unavailable as Record<string, Record<string, string>>}});
        else if (ev.result.applied) this.fetchOptions(ev.result.group);
      }
      if (this.donesOwed > 0) this.donesOwed--;
      this.afterAutox();
      this.dragQueue = [];
      this.dragEnded = false;
      if (this.replayDones > 0 && --this.replayDones === 0) this.replayAnswers = [];
      const save = this.pendingSave;
      /* a browser save (W786): the core wrote the file the command answers with (the one file, or the zip of numbered ones) into a scratch
         folder; the page fetches it under its name, once */
      if (save?.scratch && save.ahead === 0 && ev.status === 'ok' && typeof ev.result?.file === 'string') this.keepDownload(ev.result.file);
      if (save && save.ahead > 0) save.ahead--;
      else {
        this.pendingSave = null;
        this.nativeSave = null;
        for (const output of this.savedOutputs.splice(0)) void this.deliver(output.name, output.data, output.announce);
      }
      const next = this.afterDone;
      this.afterDone = null;
      if (next) for (const command of next) this.send(command);
      this.flushRotate3d();
      if (this.displayDones > 0 && --this.displayDones === 0) this.displayGuard = new Set(this.displayHeld.keys());
      this.flushDisplay();
      this.flushWindow();
      const retry = this.retrySave;
      this.retrySave = null;
      if (retry) {
        this.replayAnswers = retry.answers.slice();
        this.replayDones = 1;
        this.send(retry.cmd);
        return;
      }
      this.nativeRun = null;

    }
    this.checkDiagram(ev);
  }

  /* the autoX diagram is data the page must hold whole: a page that connected
     after autoX opened has none, and an `add` it could not place leaves it out
     of step; `redraw` makes the core send all of it again (docs/protocol.md);
     at the done, the page's own catch-up like the held changes below */
  private checkDiagram(ev: XppEvent): void {
    const {diagram: d, busy} = this.store.getState();
    if (!d.open || (d.views.every(v => v.axes) && !d.outOfStep)) {
      this.diagramAsked = false;
      return;
    }
    if (this.diagramAsked || busy || (ev.ev !== 'session.state' && ev.ev !== 'done')) return;
    this.diagramAsked = true;
    this.send({cmd: 'window.redraw', window: autoxWindow(this.store.getState())});
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
      (a held zoom sent at a done) disables nothing: the core runs what
      comes after it in turn. */
  may(cmd: Command): boolean {
    const {hello, core, computing, ask, player} = this.store.getState();
    if ((cmd.cmd === 'value.undo' && core?.can_undo === false) || (cmd.cmd === 'value.redo' && core?.can_redo === false)) return false;
    if (answeredSwitch(cmd)) return true;
    const kind = kindOf(hello, cmd);
    /* a recording's step runs: only what steers the player or a view (W59b) */
    if (player.running >= 0) return kind === 'control' || kind === 'view';
    return mayStart(kind, computing, ask !== null);
  }

  /** Menu, palette, pins and keys all enter the same registry form. */
  registryAction(id: string, values: Record<string, unknown> = {}, fixed: string[] = []): void {
    const row = registryRows(this.store.getState().hello).find(row => row.id === id);
    if (!row) { this.failed(`Unknown command ${id}`); return; }
    if (id === 'values.open') this.selectDockTab('values');
    else if (id === 'data.open') this.openTable();
    else if (id === 'command_palette.open') this.store.dispatch({type: 'drawer', open: true});
    else if (id === 'session.get_source') {this.openText();}
    else if (id === 'manual.open') this.store.dispatch({type: 'help', action: {type: 'open'}});
    else if (id === 'about.open') this.store.dispatch({type: 'help', action: {type: 'about'}});
    else if (id === 'export.several') this.store.dispatch({type: 'exportSeveral', action: {type: 'open', open: true}});
    else if (id === 'array_plot.open') this.openAplot();
    else if (id === 'animation.open') this.openAni();
    else if (id === 'annotation.list') {
      this.store.dispatch({type: 'annotations', open: true});
      this.send({cmd:'window.select', window:this.store.getState().plots.active});
      this.send({cmd: id, window:this.store.getState().plots.active});
    } else if (id === 'settings.open') this.openSettings('general');
    else if (id === 'keymap.edit') this.openSettings('keymap'); else if (row.panel && !row.arguments?.some(a => a.option) && !row.panel.endsWith('+')) this.openOptions(panelSpec(row.panel)!.group);
    else if (row.arguments?.length) this.store.dispatch({type: 'commandForm', form: {id, values, fixed}});
    else this.send({cmd: id});
  }

  /** an item of a menu_choice row (File > Export > Model as > Python): the command with that one value and no form; what it still
      needs (the file, a target) the core asks, as for the command sent by a script */
  registryChoice(id: string, args: Record<string, string | boolean>): void {
    const cmd: Command = {cmd: id, ...args};
    if (this.may(cmd)) this.send(cmd);
  }

  /** what a row names, run as a click on it runs it: an item (a command with fixed arguments) as its submenu sends it, a command by its form */
  registryRow(row: CommandRow): void {
    if (row.choice) this.registryChoice(row.id, row.choice.arguments);
    else this.registryAction(row.id);
  }

  /** the opening's check (W622): never a dialog; an answer that fails is told to the core's log at INFO and shown nowhere */
  private async automaticUpdateCheck(): Promise<void> {
    try { this.offerUpdate(await this.checkForUpdate()); }
    catch (e) {
      this.send({cmd: 'update.report_failure', reason: e instanceof Error ? e.message : String(e)});
    }
  }

  /** asks GitHub's release list for a newer release than this program, taking the file that fits how it was installed */
  checkForUpdate(signal?: AbortSignal): Promise<UpdateResult> {
    const {hello} = this.store.getState();
    return checkUpdates(hello?.about ?? [], hello?.update.asset_suffix ?? '', signal);
  }

  /** a newer release was found: it becomes the offer, unless one already is further along (downloading, or ready) */
  offerUpdate(found: UpdateResult): void {
    const now = this.store.getState().update;
    if (!found.version || !found.url || (now && now.stage !== 'available')) return;
    this.store.dispatch({type: 'update', offer: {version: found.version, url: found.url, asset: found.asset ?? null, stage: 'available'}});
  }

  /** Update: the offered file is downloaded and prepared in the core (`update.download`, docs/protocol.md); its done moves the offer on */
  downloadUpdate(): void {
    const offer = this.store.getState().update;
    if (!offer?.asset || offer.stage !== 'available') return;
    this.store.dispatch({type: 'updateStage', stage: 'downloading'});
    this.send({cmd: 'update.download', ...offer.asset, version: offer.version});
  }

  /** Restart now, or Install and restart: the core asks the leave question Quit asks, then starts the new program or the installer and quits.
      While a computation runs the page asks that question itself (leaveQuestion), and the answer goes with the command's save (W689) */
  switchUpdate(): void {
    const offer = this.store.getState().update;
    const command = offer?.stage === 'restart' ? 'update.restart' : offer?.stage === 'install' ? 'update.install' : null;
    if (!command) return;
    if (this.runningNow()) this.leaveQuestion(command);
    else this.send({cmd: command});
  }

  /** the answers of `update.download` and `update.pending` (a stop or an error puts the offer back; the error itself is shown once, by the page's error dialog) */
  private updateAnswered(ev: Extract<XppEvent, {ev: 'done'}>): void {
    const offer = this.store.getState().update;
    if (offer?.stage === 'downloading') {
      if (ev.status === 'ok' && (ev.result?.ready === 'restart' || ev.result?.ready === 'install'))
        this.store.dispatch({type: 'updateStage', stage: ev.result.ready});
      else this.store.dispatch({type: 'updateStage', stage: 'available'});
    } else if (ev.status === 'ok' && ev.result?.pending === true && typeof ev.result.version === 'string') {
      const version = ev.result.version;
      if (newerThanRunning(this.store.getState().hello?.about ?? [], version))
        this.store.dispatch({type: 'update', offer: {version, url: `${RELEASE_PREFIX}tag/v${version}`, asset: null, stage: 'install'}});
      else this.send({cmd: 'update.discard'});
    }
  }

  /** File > Settings was chosen and its values are on the way */

  /** the options panel of an option group (ui/OptionsPanel.tsx): open at once, its values asked for (`options.get`) */
  openOptions(group: string): void {
    /* the values shown are the core's current ones: those of an earlier look are dropped, the fields wait for the answer */
    this.store.dispatch({type: 'optionValues', group, values: null});
    this.store.dispatch({type: 'optionsPanel', panel: {group}});
    this.fetchOptions(group);
  }

  /** a group's values from the core (`options.get`); the answer is stored for every editor of the group's rows */
  fetchOptions(group: string): void {
    this.send({cmd: 'options.get', group});
  }

  /** Run to steady state with the options the toolbar holds (the user's edits over the defaults) */
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

  /** the page's own settings as settings.json holds them (hello's `settings`, docs/protocol.md, settings and state): what the page shows
      (page), and what the core has (sent), so only a value that changed is sent. Before hello there is no file's value to keep. */
  private page: PageSettings | null = null;
  private pageSent: PageSettings | null = null;
  private pageTimer: ReturnType<typeof setTimeout> | null = null;

  private adoptPage(info: SettingsInfo): void {
    this.showPage(info.page);
    this.store.dispatch({type: 'folds', folds: info.page.folds});
  }

  /** the page's theme and panels as the core says they are (hello's, or the Settings window's effective values after a change) */
  private showPage(page: PageSettings): void {
    this.page = {...page};
    this.pageSent = {...page};
    this.store.dispatch({type: 'theme', theme: page.theme});
    this.store.dispatch({type: 'panels', panels: {dockHidden: page.dock_hidden, dockWidth: page.dock_width}});
  }

  /** The Settings window (W792, store/settings.ts): opens on `group`, with both files' values asked for. A change is `settings.configure` (or
      `settings.reset`, the ↺) for the file shown, and the values are asked for again, so what the window shows is the core's. */
  openSettings(group: string): void {
    this.store.dispatch({type: 'keymapUi', action: {type: 'open'}});
    this.store.dispatch({type: 'settingsUi', action: {type: 'open', group}});
    this.fetchSettings();
  }

  closeSettings(): void {
    this.store.dispatch({type: 'keymapUi', action: {type: 'close'}});
    this.store.dispatch({type: 'settingsUi', action: {type: 'close'}});
  }

  private fetchSettings(): void {
    for (const scope of ['user', 'folder'] as const) {
      void this.callDone({cmd: 'settings.get', scope, id: `settings-${this.callSerial++}`}).then(ev => {
        if (ev?.status !== 'ok' || !ev.result) return;
        this.store.dispatch({type: 'settingsUi', action: {type: 'values', values: ev.result as unknown as SettingsValues}});
        const r = ev.result;
        if (scope === 'folder' && this.page)
          this.showPage({...this.page, theme: r.theme as PageSettings['theme'], dock_hidden: r.dock_hidden as boolean, dock_width: r.dock_width as number | null});
      });
    }
  }

  /** the file the window shows, in the system's own program for it (the core makes the path: the page names only the scope) */
  openSettingsFile(): void {
    void this.callDone({cmd: 'settings.open_file', scope: this.store.getState().settings.scope, id: `settings-${this.callSerial++}`}).then(ev => {
      if (ev?.status === 'error') this.store.dispatch({type: 'settingsUi', action: {type: 'error', text: ev.error ?? 'the file could not be opened'}});
    });
  }

  /** a value of the file the window shows (`json` is its JSON text), or, with null, the key taken out of that file */
  async changeSetting(group: string, key: string, json: string | null): Promise<void> {
    const scope = this.store.getState().settings.scope;
    const id = `settings-${this.callSerial++}`;
    const ev = await this.callDone(json === null ? {cmd: 'settings.reset', group, key, scope, id} : {cmd: 'settings.configure', group, values: {[key]: json}, scope, id});
    this.store.dispatch({type: 'settingsUi', action: {type: 'error', text: ev?.status === 'error' ? ev.error ?? 'the change was refused' : ''}});
    this.fetchSettings();
  }

  /** Saves the changed page settings (one settings.configure for the changed keys; the core checks them all and writes once). A dock
      width is saved once the drag has settled (PAGE_SETTLE_MS), the others at once. The store is the caller's to update. */
  setPage(change: Partial<PageSettings>): void {
    if (!this.page) return;
    this.page = {...this.page, ...change};
    if (this.pageTimer !== null) clearTimeout(this.pageTimer);
    this.pageTimer = null;
    if ('dock_width' in change) this.pageTimer = setTimeout(() => this.sendPage(), PAGE_SETTLE_MS);
    else this.sendPage();
  }

  private sendPage(): void {
    this.pageTimer = null;
    if (!this.page || !this.pageSent) return;
    const page = this.page, sent = this.pageSent;
    const values: Record<string, string> = {};
    for (const key of PAGE_KEYS) {
      if (JSON.stringify(page[key]) === JSON.stringify(sent[key])) continue;
      values[key] = JSON.stringify(page[key]);
    }
    if (Object.keys(values).length) this.send({cmd: 'settings.configure', group: 'page', values});
    this.pageSent = {...page};
  }

  /** Replace the user's keymap (docs/protocol.md "Keymap"): the answer is a `keymap` event, which is how the
      page learns the effective keys, so nothing is changed here */
  setKeymap(map: UserKeymap): void {
    this.send({cmd: 'keymap.set', bindings: map.bindings, pinned: map.pinned.map(p => p.name), steps: Object.fromEntries(map.pinned.map(p => [p.name, p.steps]))});
  }

  /** Remove the user's keymap file: all keys and pins back to the defaults */
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
    if (cmd.cmd === 'autox.run') this.store.dispatch({type:'diagram',action:{type:'run',op:'start',at:Date.now()}});
    const patch = commandSettings(cmd);
    if (patch) this.store.dispatch({type: 'autoxSettings', action: {type: 'sent', patch, ahead: this.donesOwed}});
    if (cmd.cmd !== 'answer' && !noDone(cmd)) this.donesOwed++;
    if (cmd.cmd !== 'answer') this.lastCommand = cmd;
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

  /** The desktop window's close box or the macOS app menu's Quit
      (desktop.ts __xppQuit, W110): Quit's question. While a command runs (and asks
      nothing), the page asks it itself, the run going on (LEAVE_ASK): the
      core could ask only once the run had stopped, and a misclick then
      Cancel would have lost it. Otherwise the core asks it, as F Q does
      (`quit` with `ask`; a question of the core's open is cancelled
      first). */
  quitAsked(): void {
    const {hello, ask} = this.store.getState();
    if (ask?.id === LEAVE_ASK) return;
    /* There is no loaded session to save during the startup model prompt. */
    if (!hello) {
      /* End the waiting core first: closing the OS picker first can send its
         cancellation answer ahead of Quit and turn an ordinary close into a load error. */
      this.send({cmd: 'session.quit', save: false});
      return;
    }
    if (this.runningNow()) this.leaveQuestion('session.quit');
    else this.send({cmd: 'session.quit'});
  }

  /** a computation runs (or a run's question is open): the page asks the leave question itself, the run going on */
  private runningNow(): boolean {
    const {hello, ask, busy, computing} = this.store.getState();
    return !!hello && (computing || (busy && !ask));
  }

  /** how many commands sent have no done yet (a test waits for 0: `busy` goes false at the first done, with others still on the way) */
  owed(): number {
    return this.donesOwed;
  }

  /** the page's leave question, opened while a run goes on: `then` is the command its answer goes with (session.quit, or
      update.restart / update.install, W689) */
  private leaveQuestion(then: LeaveThen): void {
    this.leaveThen = then;
    this.store.dispatch({type: 'leave', open: true, then});
  }

  /* the page's leave question answered: Save session stops the run, saves
     the session (and the recording in progress) and quits, all in the core
     (`quit` with `save`); Don't save quits at once (the plain quit: the
     window closes, docs/protocol.md "quit"); Cancel changes nothing. For an
     update, the same answers go with its save: the core does the same, then
     starts the new program or the installer (W689) */
  private leaveAnswered(fields: Record<string, unknown>): void {
    const then = this.leaveThen;
    this.leaveThen = 'session.quit';
    this.store.dispatch({type: 'leave', open: false});
    if (then !== 'session.quit') {
      if (fields.key === 's' || fields.key === 'd') this.send({cmd: then, save: fields.key === 's'});
      return;
    }
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

  /** a graph's tab picked: it is shown at once, and becomes the core's active window; the autoX view shown gives way (its views are
      hidden in the core, as Hide autoX did, so a saved session keeps which view was shown, and a grab waiting on it is cancelled) */
  selectWindow(win: string): void {
    const state = this.store.getState();
    const {plots, ask, busy, diagram} = state;
    /* in a split view on screen an autoX view stays where it is in its group (W720), so the graph's pick does not hide it */
    const drawn = splitDrawn(state);
    const fromAutox = diagram.shown && !drawn;
    if (fromAutox) this.showAutox(false);
    /* a prompt is the shown window's until answered */
    if (ask) return;
    /* a run draws into the core's active one (W68), and the command before
       (the last tab's own click, say) may not have ended yet: the tab
       picked last is held for the done, never dropped */
    if (busy) {
      this.windowHeld = win;
      return;
    }
    this.windowHeld = null;
    if (plots.active === win && !fromAutox && (!drawn || state.split.selected === win)) return;
    this.store.dispatch({type: 'selectWindow', win});
    this.send({cmd: 'window.select', window:win});
  }

  private flushWindow(): void {
    if (this.windowHeld === null || this.store.getState().busy) return;
    this.selectWindow(this.windowHeld);
  }

  renameWindow(window: string, name: string): void { this.send({cmd:'window.rename',window,name}); }

  /** a view's tab moved to `position` among the tabs of its kind, from 0 (a dragged tab, W719); one place earlier or later is the
      commands `window.move_earlier and window.move_later, which keys and the tab menu send */
  moveWindow(window: string, position: number): void { this.send({cmd:'window.move',window,position}); }
  /** one place earlier (-1) or later (1) in the tab's own strip: the tab is selected, then the core's move the keys send (W720) */
  moveWindowBeside(window: string, step: -1 | 1): void { this.send({cmd: 'window.select', window}); this.send({cmd: step < 0 ? 'window.move_earlier' : 'window.move_later'}); }

  /** the split view (W720): `window` to the second group, the split going `direction`; back to the first group; the first group's
      share of the area, sent when the splitter is released */
  splitWindow(window: string, direction: 'right' | 'down'): void { this.send({cmd:'window.split',window,direction}); }
  joinWindow(window: string): void { this.send({cmd:'window.join',window}); }
  setSplitShare(share: number): void { this.send({cmd:'window.set_split_share',share}); }

  /** Makewindow/Create: a copy of the active window, which becomes active */
  newWindow(): void {
    this.send({cmd:'window.new'});
  }

  /** Makewindow/Destroy: the window `win` (default the active one; never window 1) */
  closeWindow(win: string = this.store.getState().plots.active): void {
    this.send({cmd:'window.close',window:win});
  }

  /* ---- the display state (W65) ---- */

  /** the core's events without what a change of the user's on its way would undo */
  private unguarded(ev: XppEvent): XppEvent {
    if (!this.displayGuard.size) return ev;
    if (ev.ev === 'plot.shown')
      return {...ev, windows: ev.windows.map(w => (this.displayGuard.has(w.win) ? {...w, zoom: undefined, runs: undefined} : w))};
    if (ev.ev === 'autox.view') {
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

  /** the oldest held change goes out when the core is not busy, else at its done */
  private flushDisplay(): void {
    if (this.store.getState().busy) return;
    const first = this.displayHeld.entries().next();
    if (first.done) return;
    const [win, patch] = first.value;
    this.displayHeld.delete(win);
    const {x, y, runs} = patch;
    this.send(this.store.getState().diagram.views.some(v=>v.name===win) ? autoxDisplayCommand(win, patch)
      : {cmd: 'window.set_display', window: win, ...(x === undefined ? {} : {x_range: x ?? []}), ...(y === undefined ? {} : {y_range: y ?? []}),
         ...(runs === undefined ? {} : {show_earlier_runs: runs})});
    this.displayGuard.add(win);
    this.displayDones = this.donesOwed;
  }

  /** window `win`'s zoom changed (a wheel, a drag, a typed range, Reset zoom) */
  setViewport(win: string, viewport: Viewport): void {
    this.store.dispatch({type: 'viewport', viewport, win});
    this.display(win, rangesOf(viewport));
  }

  /** the zoom of view `view` of the autoX diagram changed */
  setDiagramViewport(view: number, viewport: Viewport): void {
    this.store.dispatch({type: 'diagram', action: {type: 'viewport', view, viewport}});
    this.display(this.store.getState().diagram.views[view].name, rangesOf(viewport));
  }

  /** a view of the autoX diagram clicked: the active one (W50), whose axes autoX's Axes menu,
      zoom and a run go by */
  activateView(view: number): void {
    const state = this.store.getState(), d = state.diagram;
    if (view >= d.views.length) return;
    /* a graph may be the selected one beside it in a split view: then it is selected too */
    if (view === d.active && !(splitDrawn(state) && state.split.selected !== d.views[view].name)) return;
    this.store.dispatch({type: 'diagram', action: {type: 'activate', view}});
    this.send({cmd:'window.select',window:d.views[view].name});
  }

  /** a view of the autoX diagram closed (the last one stays: the core refuses it) */
  closeView(view: number): void {
    this.send({cmd:'window.close',window:this.store.getState().diagram.views[view].name});
  }

  /** the legend's "previous runs" toggle of window `win` */
  setShowRuns(win: string, show: boolean): void {
    this.store.dispatch({type: 'showRuns', win, show});
    this.display(win, {runs: show});
  }

  /** autoX's "Earlier branches" toggle */
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
      /* busy: held, the latest angle goes at the done */
      if (this.store.getState().busy) this.rotate3dHeld.set(win, {theta, phi});
      else this.turn3d(win, theta, phi);
    };
    if (performance.now() - last >= 100) send();
    else this.rotate3dTimer.set(win, setTimeout(send, 150));
  }

  /* at a done: the latest angle each window was turned to while the core was busy (rotate3d) */
  private flushRotate3d(): void {
    if (this.store.getState().busy) return;
    /* one plan at a time: the next window's angle goes at the next done */
    const first = this.rotate3dHeld.entries().next();
    if (first.done) return;
    this.rotate3dHeld.delete(first.value[0]);
    this.turn3d(first.value[0], first.value[1].theta, first.value[1].phi);
  }

  /** window `win`'s 3D view set to `theta` and `phi` in the core (plot.set_3d_view) */
  private turn3d(win: string, theta: number, phi: number): void {
    this.send({cmd: 'plot.set_3d_view', window: win, theta, phi});
  }

  /** stops the running command; it still ends with its done */
  abort(): void {
    this.batchStopped = true;
    this.store.dispatch({type: 'aborting'});
    this.transport.send({cmd: 'run.stop'});
  }

  /* ---- the autoX view (docs/ui-v2.md T11a, docs/protocol.md `auto`) ---- */

  /** one of the autoX window's buttons, sent as its key (`win` `auto`); its
      prompts come as ordinary asks. Clear also is the view's own (T21):
      the branches so far become the earlier ones, hidden until shown
      again; the core's own clear and redraw (keys `c`, `d`) follow */
  autoxOp(op: AutoxOp): void {
    this.registryAction(toolbarCommand('autox', op));
  }

  /** autoX's Run (docs/protocol.md autox.run): the kind, and where it starts (a stored point's label, else the current state) */
  autoxRun(args: {kind: string; label?: number; switch?: boolean; replace_diagram?: boolean}): void {
    const cmd: Command = {cmd: 'autox.run', ...args};
    if (!this.may(cmd)) return;
    this.send(cmd);
  }

  /* after every done: the run's clock stops, a plan ends, a grabbed periodic orbit is imported */
  private afterAutox(): void {
    if (this.store.getState().diagram.run?.active)
      this.store.dispatch({type: 'diagram', action: {type: 'run', op: 'end', at: Date.now()}});
    const g = this.grabbed;
    this.grabbed = null;
    if (g !== null) this.importOrbit(g);
  }

  /* a grab took point `point`: when it is a periodic orbit autoX stored (a
     labelled point), File/Import orbit loads it, so the main plot shows the
     limit cycle (docs/ui-v2.md T21); autoX keeps no orbit for other points */
  private importOrbit(point: number): void {
    const {points, labels} = activeView(this.store.getState().diagram);
    const ty = points.ty[point];
    if ((ty !== 3 && ty !== 4) || points.f2[point]) return;
    if (!labels.some(l => l.point === point)) {
      this.store.dispatch({type: 'toast', kind: 'info', text: 'autoX keeps the orbits of labelled points only: grab a '
        + 'labelled point of the periodic branch (Tab steps through them) to plot its limit cycle.'});
      return;
    }
    this.send({cmd: 'autox.show_orbit'});
  }

  /** autoX's options edited in the page's forms (T22, store/autoxOptions.ts):
      one `autox` `set` at once, busy or idle -- an option (W106): during a
      run the core applies it when the run ends, to the next one */
  autoxSettings(patch: AutoxSettingsPatch): void {
    for (const {command} of autoxSettingsCommands(patch)) this.send(command);
  }

  /** an autoX tab's ×: done with that view (A10); the last one is autoX itself, so the window closes. A running job is
      stopped first and the view closes at its done, instead of the close waiting behind the run. */
  closeAutox(view: number | null = null): void {
    const {busy, stopping, diagram} = this.store.getState();
    const closing = view === null || diagram.views.length === 1 ? diagram.views : [diagram.views[view]];
    const commands = closing.map(v => ({cmd: 'window.close', window: v.name}));
    if (!busy) {
      for (const command of commands) this.send(command);
      return;
    }
    this.afterDone = commands;
    if (!stopping) this.abort();
  }

  /** an autoX tab picked: that view of the diagram is the active one, and shown */
  selectAutoxView(view: number): void {
    const {diagram} = this.store.getState();
    if (view >= diagram.views.length) return;
    if (!diagram.shown) this.showAutox(true);
    this.activateView(view);
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
      autoX's File/sElect 2par pt uses (docs/protocol.md `autox` `point`) */
  autoxPoint(x: number, y: number): void {
    this.store.dispatch({type: 'diagram', action: {type: 'stored', at: {x, y}}});
    this.send({cmd: 'autox.pick', x, y});
  }

  /** Hide autoX hides every view of the diagram in the core (window.hide: they keep everything); Show brings them back */
  showAutox(shown: boolean): void {
    /* a grab or plot mode waiting on the diagram is cancelled with it: the ask would stay open behind
       the main window and swallow every key typed there (W100) */
    const {ask, pick, diagram} = this.store.getState();
    if (!shown && diagram.shown && ask && (diagram.grabbing || (pick && pick.win === autoxWindow(this.store.getState())))) this.cancelPick();
    for (const view of diagram.views) this.send({cmd: shown ? 'window.show' : 'window.hide', window: view.name});
    if (shown && diagram.open) this.send({cmd:'window.select', window:autoxWindow(this.store.getState())});
  }

  /* ---- values panel (docs/ui-v2.md T3, docs/protocol.md `set`/`slide`/`default`/`userbut`,
     GitHub #155): every edit (a field, a slider, Reset, a numerics field) is an option (W106) and
     goes to the core at once as a `set`, busy or idle; during a computation the core applies it
     when that ends, never to the run in progress. The field shows it meanwhile (store/values.ts
     inflight): the value shown is the value. No undo (GitHub #110): Reset is the way back. ---- */

  /** edits, in one `set`: shown on their fields until its done, which also
      ends the attribution of an error to them (A11) */
  private edit(...sets: ValueSet[]): void {
    for (const {command, sets: sent} of valueCommands(this.store.getState().hello, sets)) {
      const ahead = this.donesOwed;
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
    this.send({cmd: 'value.delete_slider', slot: id - 1});
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

  /** a section's Restore defaults (the sets row, W323): the core's `set.restore_defaults` for the section's scope, every
      value of it back to the model file's (the core redoes the tables once); a graph's section names its window */
  restoreDefaults(scope: string, window?: string): void {
    const kind = (Object.keys(RESTORE_SCOPE) as (keyof typeof RESTORE_SCOPE)[]).find(k => RESTORE_SCOPE[k] === scope);
    if (kind) this.store.dispatch({type: 'values', action: {type: 'defaulted', kind}});
    this.send({cmd: 'set.restore_defaults', scope, ...(window ? {window} : {})});
  }

  /** a numerics field (the values panel's Numerics, W106): `key` as the
      `numerics` event names it, the method by its number */
  setNumeric(key: string, text: string): void {
    this.edit({kind: 'num', name: key, text});
  }

  /** Load values from a file by scope (W321, docs/protocol.md `set.list_file_contents`): the picked file goes into the model's
      folder as every upload does, the core reads it whole and lists the parts that hold `scope`'s kind, and the page
      offers them (ui/ValuesFileDialog.tsx); the part chosen is loaded by loadValuesPart. */
  async offerValuesFile(scope: string, file: File): Promise<void> {
    const uploads = await this.upload([{file, name: file.name}], null, true);
    if (!uploads) return;
    this.send({cmd: 'set.list_file_contents', file: uploads[0].name, scope});
  }

  /** the offered part chosen: `set.load_file` applies it limited to the scope, and the offer closes */
  loadValuesPart(offer: ValuesFileOffer, part: ValuesPart): void {
    this.store.dispatch({type: 'valuesFile', offer: null});
    this.send({cmd: 'set.load_file', file: offer.file, scope: offer.scope, from: part.from, ...(part.from === 'set' ? {name: part.name} : {})});
  }

  /** Load of a section's XPPAUT .par/.ic (W66 review; an import, nothing of ours writes these files, W321): the
      picked file goes into the model's folder through `upload`, as every
      upload does (the same name with other content asks first: Replace,
      Keep both, Cancel; W134), then the core reads it with `value.import_file`
      -- at once, not staged as a pending edit (the values panel has nothing
      left to parse). A bad file's `message` `error` (core/lunch-new.cpp,
      e.g. "Expected N initial conditions...") becomes a notification the
      same way any other command's does. */
  async loadValues(kind: 'par' | 'ic', file: File): Promise<void> {
    const uploads = await this.upload([{file, name: file.name}], null, true);
    if (uploads) this.send({cmd: 'value.import_file', kind: VALUE_FILE[kind], file: uploads[0].name});
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

  /* ---- data table (docs/ui-v2.md T10, docs/protocol.md "The data table") ---- */

  openTable(): void {
    /* the core keeps one table block, which the pop-out has: this page opening it too would take it over */
    if (this.popouts.isOut(TABLE_VIEW)) {
      void this.popouts.show(TABLE_VIEW);
      return;
    }
    this.store.dispatch({type: 'table', action: {type: 'open', open: true}});
  }

  /** closes the table; ui/TableView.tsx then stops the core sending blocks, however the table came to close (docs/protocol.md: `count` 0 stops the updates) */
  closeTable(): void {
    this.store.dispatch({type: 'table', action: {type: 'open', open: false}});
  }

  /** the right dock's tab `tab` shown, the dock too (Values closes the panel that was open) */
  selectDockTab(tab: DockTab): void {
    /* a tab in a window of its own brings that window forward; Values stays the dock's fallback, so it is also shown */
    const view: ViewKey = {kind: 'dock', name: tab};
    if (this.popouts.isOut(view)) {
      void this.popouts.show(view);
      if (tab !== 'values') return;
    }
    const {table, text, help} = this.store.getState();
    if (tab === 'table') this.openTable();
    else if (tab === 'text') this.openText();
    else if (tab === 'help') this.store.dispatch({type: 'help', action: {type: 'open'}});
    else {
      if (table.open) this.closeTable();
      if (text.open) this.closeText();
      if (help.open) this.store.dispatch({type: 'help', action: {type: 'close'}});
      this.store.dispatch({type: 'panels', panels: {dockHidden: false}});
    }
    this.store.dispatch({type: 'dockPanel', open: true});
  }

  /** the dock tab `tab` into a window of its own; the dock shows Values again, so the panel (the table's block, which
      the core keeps one of, the manual's place) is the pop-out's alone */
  popOutDockTab(tab: DockTab): void {
    if (tab !== 'values') this.selectDockTab('values');
    void this.popouts.show({kind: 'dock', name: tab});
  }

  /** the dock's ×: the panel that was open closed, and the dock hidden (the column, kept per viewer) or, `asSheet` (a phone), its sheet shut;
      beside the autoX view only the panel closes (the dock is there for it alone) */
  hideDock(asSheet = false): void {
    const {table, text, help, diagram} = this.store.getState();
    if (table.open) this.closeTable();
    if (text.open) this.closeText();
    if (help.open) this.store.dispatch({type: 'help', action: {type: 'close'}});
    if (!asSheet && !diagram.shown) this.store.dispatch({type: 'panels', panels: {dockHidden: true}});
    this.store.dispatch({type: 'dockPanel', open: false});
  }

  /** the row the arrow keys, a click, Home/End move to */
  selectTableRow(row: number): void {
    this.store.dispatch({type: 'table', action: {type: 'select', row}});
  }

  /** the rows [visibleFrom, visibleFrom+visibleCount) the view can see: asks
      for a new block only when the cached one does not cover them, and
      never twice for the same block (store/table.ts planRequest). A
      `data.page` request is a control line, answered at once
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
    this.send({cmd: 'data.page', from_row: req.from, count: req.count, first_column: req.col, columns: req.ncol});
  }

  /** one of the browser's buttons, on the selected row (docs/protocol.md
      data.* commands; Find, Replace, Table, Load and Write prompt through the
      ordinary `ask`, AskDialog already shows) */
  dataOp(op: DataOp): void {
    const row = this.store.getState().table.selected;
    const command = toolbarCommand('data', op);
    if (['first', 'last'].includes(op)) this.send({cmd:command,row});
    else this.registryAction(command);
  }

  /** Get: the selected row becomes the initial conditions (the next `state` has them) */
  getRow(): void {
    this.send({cmd: 'initial_conditions.set_from_data_row', row: this.store.getState().table.selected});
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

  /** Pause: reaches the running Go at once (a control line: no done of its own) */
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
    this.send({cmd: 'source.run_action', index});
  }

  /** Find the equilibrium from the current initial conditions. */
  findEquilibrium(): void {
    this.send({cmd: 'equilibrium.find'});
  }

  /** the equilibrium window's Import: the last equilibrium becomes the
      initial conditions (the next `state` has them) */
  importEquilibrium(): void {
    this.send({cmd: 'initial_conditions.set_from_equilibrium'});
  }

  /* ---- files (docs/ui-v2.md section 4, T5): the model's folder is the workspace ---- */

  /** the window's exports now: the registry's rows, the ones the session cannot run greyed (File > Export > Several, W774) */
  private exportRowsNow(): ExportRow[] {
    const {hello, core} = this.store.getState();
    return exportRows(registryRows(hello), hello?.menu_separator ?? '', hello?.output_names ?? {}, core?.unavailable ?? []);
  }

  /** a format ticked or unticked in the window */
  exportSeveralTick(key: string, on: boolean): void {
    this.store.dispatch({type: 'exportSeveral', action: {type: 'tick', key, on}});
  }

  /** the window's Export button: its ticked files go to the batch runner, each one ordinary export command. On the desktop one save dialog,
      titled with the count, names every file: the name written is the stem in place of the model's, its folder where they go (the window
      grants it to those files); Cancel there goes back to the ticks. A browser downloads each file under the model's base */
  async exportSeveralStart(): Promise<void> {
    const st = this.store.getState().exportSeveral;
    const items = this.exportRowsNow().filter(row => !row.reason).flatMap(row => row.items).filter(item => st.ticks.includes(item.key));
    if (!items.length) return;
    const hello = this.store.getState().hello;
    const asking = pickStem(exportDialogTitle(items.length), folderOf(hello?.file ?? ''), hello?.output_stem ?? '');
    let folder = '';
    let names = items.map(item => item.file);
    if (asking) {
      let picked: string | null;
      try {
        picked = await asking;
      } catch (e) {
        this.failed(`The file dialog could not open: ${e instanceof Error ? e.message : String(e)}`);
        return;
      }
      if (picked === null) return; /* Cancel: back to the ticks, nothing written */
      this.exportDesktop = true; /* the window granted the folder: released with the batch (exportSeveralClose, after) */
      const typed = typedStem(picked);
      const stemmed = 'stem' in typed ? items.map(item => restem(item.file, hello?.output_stem ?? '', typed.stem)) : [];
      if ('error' in typed || stemmed.includes(null)) {
        this.failed(`Export several: ${'error' in typed ? typed.error : 'the name gives a file name the core does not accept'}`);
        this.releaseExportFolder();
        return;
      }
      folder = folderOf(picked);
      names = stemmed as string[];
    }
    const download = !asking;
    const plan = items.map((item, i) => ({
      step: {label: names[i], command: {cmd: item.cmd, ...item.args, ...(download && {file_picker: true})}, file: download ? null : joinFolder(folder, names[i])},
      name: names[i], download}));
    const exists = async (): Promise<boolean[]> => {
      if (download) return plan.map(() => false); /* a download never collides with what is there */
      const found: boolean[] = [];
      for (const {step} of plan) {
        const ev = await this.callDone({cmd: 'session.file_exists', file: step.file!, id: `exists-${this.callSerial++}`});
        if (ev?.status !== 'ok') throw new Error(ev?.error ?? 'the check stopped');
        found.push(ev.result?.exists === true);
      }
      return found;
    };
    await this.batchStart(EXPORT_TITLE, plan, exists, {report: true, stopOnFailure: false, after: () => this.releaseExportFolder()});
  }

  /** the folder the save dialog granted to the window's files is released (the batch ended, the window closed or the name was refused) */
  private releaseExportFolder(): void {
    if (!this.exportDesktop) return;
    void releaseFolder();
    this.exportDesktop = false;
  }

  /** the replace question's answer (Export several's and a pin's): Replace writes every file (the answer passed to each command), Skip
      those writes the others, Cancel ends the batch with nothing sent */
  batchAnswer(answer: 'replace' | 'skip' | 'cancel'): void {
    const pending = this.batchPending;
    this.batchPending = null;
    if (!pending) return;
    if (answer === 'cancel') {
      this.store.dispatch({type: 'batch', action: {type: 'clear'}});
      return;
    }
    void this.batchRun(pending.title, pending.plan.map(p => ({...p, skip: answer === 'skip' && p.exists === true})), pending.options, answer === 'replace' ? 1 : undefined);
  }

  /** runs pin `name` (the toolbar's button, Settings' Run): its steps are ordinary commands, each with the file it writes in the model's
      folder, through the batch runner that Export several uses; no step asks, and one replace question covers the files that exist */
  async runPin(name: string): Promise<void> {
    const {keymap, hello} = this.store.getState();
    const pin = keymap.info?.pinned.find(p => p.name === name);
    if (!pin || this.store.getState().batch.phase !== 'idle') return;
    const items = this.exportRowsNow().flatMap(row => row.items);
    const {steps, problem} = pinSteps(pin, step => stepRun(keymap.info, hello, step), items);
    if (problem) {
      this.failed(`Pin "${name}" did not start: ${problem}`);
      return;
    }
    const plan = steps.map(step => ({step, name: step.file ?? ''}));
    const exists = async (): Promise<boolean[]> => {
      const listing = await this.files?.list().catch(() => []) ?? [];
      return plan.map(({name: file}) => !!file && listing.some(f => f.name === file));
    };
    await this.batchStart(`Pin "${name}"`, plan, exists, {report: false, stopOnFailure: true});
  }

  /** the model's folder as it is now, for what the page says about files (a step's check says its file exists) */
  async refreshListing(): Promise<void> {
    const listing = await this.files?.list().catch(() => null);
    if (listing) this.store.dispatch({type: 'files', action: {type: 'listing', files: listing}});
  }

  /** asks the core to read a step as a command value (keymap.check_step, the one reader): its run, or the error to show under the step */
  async checkStep(text: string): Promise<{run: Record<string, string | boolean>} | {error: string}> {
    const ev = await this.callDone({cmd: 'keymap.check_step', step: text, id: `step-${this.callSerial++}`});
    if (ev?.status === 'ok') return {run: ev.result?.run as Record<string, string | boolean>};
    return {error: ev?.error ?? 'the check was not sent'};
  }

  /** a batch: the files that exist are asked about once (phase exists), else every step runs at once */
  private async batchStart(title: string, plan: BatchPlan[], exists: () => Promise<boolean[]>, options: BatchOptions): Promise<void> {
    let found: boolean[];
    try {
      found = await exists();
    } catch (e) {
      this.failed(`${title}: ${e instanceof Error ? e.message : String(e)}`);
      options.after?.();
      return;
    }
    const withExists = plan.map((p, i) => ({...p, exists: found[i]}));
    const names = withExists.filter(p => p.exists).map(p => p.name);
    if (names.length) {
      this.batchPending = {title, plan: withExists, options};
      this.store.dispatch({type: 'batch', action: {type: 'ask', title, steps: plan.map(p => p.step.label), exists: names}});
    } else await this.batchRun(title, withExists, options);
  }

  /** the batch runner (Export several's and the pins'): one ordinary command per step, each awaited to its done, so the outcomes come in
      order. The status bar shows the step running and Messages a line as each starts and ends; Stop or a failing step (stopOnFailure) ends
      a batch with one message naming the step, those done and those not run. Steps that ran stay done: nothing is undone */
  private async batchRun(title: string, plan: BatchPlan[], options: BatchOptions, replace?: 1): Promise<void> {
    const dispatch = (action: BatchAction) => this.store.dispatch({type: 'batch', action});
    const all = plan.map(p => p.step.label);
    const outcomes: BatchOutcome[] = [];
    this.batchStopped = false;
    dispatch({type: 'run', title, steps: all});
    for (const [i, {step, skip, download}] of plan.entries()) {
      if (this.batchStopped) break;
      dispatch({type: 'step', index: i});
      let outcome: BatchOutcome;
      if (skip) outcome = {step: step.label, status: 'skipped', text: 'it exists, and was kept'};
      else {
        this.store.dispatch({type: 'note', text: startLine(title, i, plan.length, step.label)});
        if (download) this.downloadNames = [step.label];
        const ev = await this.callDone(stepCommand(step.command, step.file, `batch-${this.callSerial++}`, replace));
        this.downloadNames = [];
        /* a file question that ended with no file (the core could not make the scratch folder: its error is in Messages) is `cancelled`, an ok done */
        outcome = ev?.status === 'ok' && ev.result?.cancelled === true ? {step: step.label, status: 'failed', text: 'no file was written'}
          : ev?.status === 'ok' ? {step: step.label, status: download ? 'downloaded' : step.file ? 'written' : 'ran', text: step.file ?? ''}
          : {step: step.label, status: 'failed', text: ev?.error ?? (ev ? 'Stop was pressed' : 'the command was not sent')};
        this.store.dispatch({type: 'note', text: endLine(title, i, plan.length, outcome)});
      }
      outcomes.push(outcome);
      dispatch({type: 'outcome', outcome});
      if (outcome.status === 'failed' && options.stopOnFailure) break;
    }
    options.after?.();
    void this.refreshListing();
    const message = options.stopOnFailure ? stoppedMessage(title, all, outcomes) : null;
    dispatch({type: 'end', report: options.report});
    if (message) this.store.dispatch({type: 'toast', kind: this.batchStopped ? 'info' : 'error', text: message}); /* Stop is the user's own: a notice, not an error dialog */
    else if (!options.report) {
      this.store.dispatch({type: 'note', text: doneMessage(title, plan.length)});
      this.store.dispatch({type: 'bottom', text: doneMessage(title, plan.length)});
    }
    this.batchStopped = false;
  }

  /** sends a command and resolves with its done (matched by its id), or null when it was not sent */
  private callDone(cmd: Command): Promise<DoneEvent | null> {
    return new Promise(resolve => {
      const id = String(cmd.id);
      this.exportWaits.set(id, resolve);
      if (!this.send(cmd)) {
        this.exportWaits.delete(id);
        resolve(null);
      }
    });
  }

  /** the window closes: a desktop's folder grant ends with it */
  exportSeveralClose(): void {
    this.releaseExportFolder();
    this.batchPending = null;
    this.store.dispatch({type: 'batch', action: {type: 'clear'}});
    this.store.dispatch({type: 'exportSeveral', action: {type: 'open', open: false}});
  }

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
          this.failed(`xppautX cannot use a file named “${wanted}” in the model's folder.`
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
      const request = nativeFileRequest({...ask, file: this.askedName(ask)});
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

  /** a `file` ask for writing, answered at once (the maintainer, 2026-11-17: in a browser every export is a download): the core writes
      `name` into its scratch folder, and the page fetches it under that name (keepDownload) and downloads it */
  private saveFile(ask: AskEvent, name: string): void {
    this.pendingSave = {name, ahead: 0, scratch: true};
    this.answer(ask, {file: name, scratch: true});
  }

  /** the browser's save ask: the file's name is the one a command of Export several was given (downloadNames), else the ask's default */
  private browserSave(ask: AskEvent): void {
    this.saveFile(ask, baseName(this.downloadNames.shift() ?? this.askedName(ask)));
  }

  /** the name a file question starts with: the core's, else the command's default (hello's output_names: the one table of default names) */
  private askedName(ask: AskEvent): string {
    const {hello} = this.store.getState();
    return ask.file || (this.lastCommand && hello ? defaultFileName(registryRows(hello), hello.output_names ?? {}, this.lastCommand) : '');
  }

  /** a browser save's file the core wrote into its scratch folder: the page fetches it once, under its name, and delivers it */
  private keepDownload(file: string): void {
    const name = baseName(file);
    const data = (this.files ? this.files.download(name) : Promise.resolve(null)).catch(e => {
      this.failed(`${name} could not be downloaded: ${e instanceof Error ? e.message : String(e)}`);
      return null;
    });
    this.savingReads.add(data);
    void data.then(() => this.savingReads.delete(data));
    this.savedOutputs.push({name, data, announce: this.store.getState().batch.phase !== 'running'}); /* a batch's report lists its files */
  }

  private async deliver(name: string, snapshot: Promise<Blob | null>, announce = false): Promise<void> {
    try {
      const data = await snapshot;
      if (!data) return; /* not written: the command stopped before it */
      offerDownload(name, data);
      if (announce) this.store.dispatch({type: 'toast', kind: 'info', text: `${name} downloaded`});
      this.store.dispatch({type: 'files', action: {type: 'offered', offered: {name, size: data.size, sha256: await sha256Hex(data), how: 'download'}}});
    } catch (e) {
      this.failed(`${name} could not be delivered: ${e instanceof Error ? e.message : String(e)}`);
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
    this.replayDones = 1;
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
      AskDialog), Fit, Range; its Export menu (array_plot.export) is ChoiceItems */
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

  /** the session's color scale, by name: the core's, so the pictures it writes use it too */
  setColormap(name: string): void {
    this.send({cmd: 'plot.set_colormap', colormap: name});
  }

  /** the cell under the pointer, or null off the grid */
  aplotHover(hover: AplotHover | null): void {
    this.store.dispatch({type: 'aplot', action: {type: 'hover', hover}});
  }
}
