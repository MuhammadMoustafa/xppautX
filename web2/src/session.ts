/* The session: connects a transport to the store and is the one place that
   sends commands. Components call its methods, never the transport. */
import {offerDownload, writeTo, type SaveHandle} from './pickers';
import {accumulateScroll, keyScroll} from './plot/aplotScroll';
import type {AplotColorMap} from './plot/aplotColors';
import {download} from './plot/export';
import {bytesToBase64, encodeGif} from './plot/gif';
import {renderFrame} from './plot/kinescopeRender';
import {pickAnswer, type PickState} from './plot/pick';
import {chartOf} from './plot/registry';
import type {Ranges} from './plot/viewmath';
import {sha256Hex, type FilesApi} from './protocol/files';
import type {Transport} from './protocol/transport';
import type {AskEvent, BrowserEvent, Command, FilmEvent, XppEvent} from './protocol/types';
import type {AplotHover} from './store/aplot';
import {
  answerName, keepBothName, menuKeys, safeName, uploadPlan, type ReplaceChoice, type RunAnswer, type Upload,
} from './store/files';
import {createStore, type Store} from './store/store';
import {initialState, reduce, type Action, type AppState} from './store/state';
import {stepTarget} from './store/ani';
import {snapshotWindow, type KinescopeFrame} from './store/kinescope';
import {windowOf} from './store/plots';
import {MAX_COUNT, MAX_NCOL, planRequest, tableCsv} from './store/table';
import type {TextTab} from './store/text';
import type {ValueEdit} from './store/values';

/** the data browser's buttons (docs/protocol.md `browser` op; web/xpp-client.js's BROWSER_BUTTONS) */
export type BrowserOp = 'find' | 'get' | 'replace' | 'unreplace' | 'table' | 'load' | 'write' | 'first' | 'last'
  | 'restore' | 'addcol' | 'delcol';

/** the AUTO window's buttons (docs/protocol.md `auto` op); Close is session.closeAuto */
export type AutoOp = 'param' | 'axes' | 'numerics' | 'run' | 'grab' | 'usr' | 'clear' | 'redraw' | 'file';
/** the array plot window's buttons (docs/protocol.md `aplot` op; web/xpp-client.js's buildArrayPlot) */
export type AplotOp = 'redraw' | 'edit' | 'print' | 'fit' | 'range' | 'gif' | 'close';

export class Session {
  readonly store: Store<AppState, Action>;
  /** keys that answer the menus a key sequence opens ("i g": Initialconds, Go) */
  private pendingKeys: string[] = [];
  /** pointer events of a drag made while the core was not asking (docs/protocol.md
      `drag`: it asks again after each one), and whether the drag has ended */
  private dragQueue: Record<string, unknown>[] = [];
  private dragEnded = false;
  /** a file ask for writing answered: the file to hand to the browser once the command is done */
  private pendingSave: {name: string; handle: SaveHandle | null} | null = null;
  /** the answer to the replace confirm, when it is open */
  private replaceChoice: ((c: ReplaceChoice) => void) | null = null;
  /** a command run again after "Add file…": the answers its prompts get, and
      the idles to wait for (the menu keys before it, then its own) */
  private replayAnswers: RunAnswer[] = [];
  /** rotate3d's throttle: the last time a `view3d` went out for a window,
      and its pending trailing send, by window */
  private rotate3dLast = new Map<number, number>();
  private rotate3dTimer = new Map<number, ReturnType<typeof setTimeout>>();
  private replayIdles = 0;
  /** a command to send when the running one has ended (the AUTO view's close while busy, A10) */
  private afterIdle: Command | null = null;
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
    this.transport.open(ev => this.receive(ev), open => this.store.dispatch({type: 'connection', open}));
  }

  private receive(ev: XppEvent): void {
    this.store.dispatch({type: 'event', ev});
    if (ev.ev === 'hello') {
      /* the plots as data (docs/protocol.md): asked for on every (re)connection,
         which also makes the server send the windows, their series, nullclines,
         direction fields and marks; values
         as base64 float32, which a long run needs (a server that does not know
         enc sends JSON numbers, which the store reads as well) */
      const events = ['series', 'plots', 'nullclines', 'dfield', 'marks', 'ani', 'autoinfo'].filter(name => ev.features?.includes(name));
      if (events.length) this.send({cmd: 'data', events, enc: 'f32'});
    } else if (ev.ev === 'film') {
      this.onFilm(ev);
    } else if (ev.ev === 'ask') {
      if (ev.kind === 'pixels') {
        this.answerPixels(ev);
      } else if (ev.kind === 'alert') {
        /* an alert only informs: a notification that does not stop the run */
        this.store.dispatch({type: 'toast', kind: 'info', text: ev.message ?? ''});
        this.answer(ev, {});
      } else if (ev.kind === 'drag' && (this.dragEnded || this.dragQueue.length)) {
        /* the events made meanwhile first, then the end */
        if (this.dragQueue.length) this.answer(ev, this.dragQueue.shift()!);
        else this.cancel(ev);
      } else if (this.replayAnswers.length) this.continueReplay(ev);
      else this.continueKeys(ev);
    } else if (ev.ev === 'idle') {
      this.pendingKeys = [];
      this.dragQueue = [];
      this.dragEnded = false;
      if (this.replayIdles > 0 && --this.replayIdles === 0) this.replayAnswers = [];
      const save = this.pendingSave;
      this.pendingSave = null;
      if (save && !this.store.getState().files.runFailed) void this.deliver(save.name, save.handle);
      const next = this.afterIdle;
      this.afterIdle = null;
      if (next) this.send(next);
    }
    this.checkDiagram(ev);
  }

  /* the AUTO diagram is data the page must hold whole: a page that connected
     after AUTO opened has none, and an `add` it could not place leaves it out
     of step; `redraw` makes the core send all of it again (docs/protocol.md) */
  private checkDiagram(ev: XppEvent): void {
    const d = this.store.getState().diagram;
    if (!d.open || (d.axes && !d.outOfStep)) {
      this.diagramAsked = false;
      return;
    }
    if (this.diagramAsked || ev.ev !== 'state') return;
    this.diagramAsked = true;
    this.send({cmd: 'redraw'});
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

  private continueKeys(ask: AskEvent): void {
    if (!this.pendingKeys.length) return;
    if (ask.kind === 'menu' || ask.kind === 'choice') this.answer(ask, {key: this.pendingKeys.shift()});
    else this.pendingKeys = []; /* anything else is the user's to answer */
  }

  send(cmd: Command): void {
    this.store.dispatch({type: 'sent', cmd});
    this.transport.send(cmd);
  }

  /** an XPP hotkey, as typed in the X11 main window */
  key(key: string): void {
    this.send({cmd: 'key', key});
  }

  /** a key, then keys for the menus it opens: keys('i', 'g') integrates */
  keys(first: string, ...then: string[]): void {
    this.pendingKeys = then;
    this.key(first);
  }

  answer(ask: AskEvent, fields: Record<string, unknown>): void {
    this.send({cmd: 'answer', id: ask.id, ...fields});
  }

  cancel(ask: AskEvent): void {
    this.answer(ask, {ok: 0});
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
  selectWindow(win: number): void {
    const {plots, ask} = this.store.getState();
    if (plots.active === win || ask) return; /* a prompt is the shown window's until answered */
    this.store.dispatch({type: 'selectWindow', win});
    this.send({cmd: 'click', win});
  }

  /** Makewindow/Create: a copy of the active window, which becomes active */
  newWindow(): void {
    this.keys('m', 'c');
  }

  /** Makewindow/Destroy: the active window (never window 1) */
  closeWindow(): void {
    this.keys('m', 'd');
  }

  /* ---- Use this view (docs/ui-v2.md T9) ---- */

  /** "Use this view": the client's current zoom of `win` becomes the
      core's own axes (`{"cmd":"view",...}`, docs/protocol.md), exactly as
      Window/Window would. The core's `plots` and `state.view` that follow
      report the new axes; the plot's own viewport then goes back to "the
      core's axes" (store/plots.ts coreMoved, from the `state` reducer),
      with no visible jump since they are now the same range. */
  useThisView(win: number, ranges: Ranges): void {
    this.send({cmd: 'view', win, xlo: ranges.x.min, xhi: ranges.x.max, ylo: ranges.y.min, yhi: ranges.y.max});
  }

  /** Window/Fit: the key sequence the classic page uses ('w' opens the
      Window submenu, 'f' is Fit) sets the active window's axes to the
      data's extent. */
  fitView(): void {
    this.keys('w', 'f');
  }

  /* ---- 3D plots (docs/ui-v2.md T14) ---- */

  /** window `win`'s 3D view turned to `theta`, `phi` (a drag or the arrow
      keys on the focused plot): the store updates at once, so the plot
      (projected in the client, plot/project3d.ts) redraws with no round
      trip. The core's own state (a PostScript/SVG export, `state.view`,
      any other client) is kept in step with `{"cmd":"view3d",...}`,
      throttled to at most 10 a second while the turn continues, matching
      plot_data's own throttle (docs/protocol.md); a trailing send 150 ms
      after the last change always lands, so the settled angle reaches the
      core even with no explicit end wired in (a key held down auto-
      repeats, with no keyup between steps). */
  rotate3d(win: number, theta: number, phi: number): void {
    this.store.dispatch({type: 'rotate3d', win, theta, phi});
    const timer = this.rotate3dTimer.get(win);
    if (timer !== undefined) clearTimeout(timer);
    const last = this.rotate3dLast.get(win) ?? -Infinity;
    const send = () => {
      this.rotate3dLast.set(win, performance.now());
      this.rotate3dTimer.delete(win);
      this.send({cmd: 'view3d', win, theta, phi});
    };
    if (performance.now() - last >= 100) send();
    else this.rotate3dTimer.set(win, setTimeout(send, 150));
  }

  /** stops the running command; it still ends with its idle */
  abort(): void {
    this.store.dispatch({type: 'aborting'});
    this.transport.send({cmd: 'abort'});
  }

  /* ---- the AUTO view (docs/ui-v2.md T11a, docs/protocol.md `auto`) ---- */

  /** one of the AUTO window's buttons; its prompts come as ordinary asks */
  autoOp(op: AutoOp): void {
    this.send({cmd: 'auto', op});
  }

  /** the AUTO view's close: done with it (A10). A running job is stopped
      first and the window closes at its idle, instead of the close waiting
      behind the run. */
  closeAuto(): void {
    const {busy, stopping} = this.store.getState();
    if (!busy) {
      this.send({cmd: 'auto', op: 'close'});
      return;
    }
    this.afterIdle = {cmd: 'auto', op: 'close'};
    if (!stopping) this.abort();
  }

  /** the core's grab (docs/protocol.md `grab`): the cursor to point `point`
      of the diagram's data, and with `take` the point is taken at once */
  grabPoint(point: number, take = false): void {
    const ask = this.store.getState().ask;
    if (ask?.kind === 'grab') this.answer(ask, take ? {point, key: 'Return'} : {point});
  }

  /** the grab takes the point under its cursor (Enter) */
  grabTake(): void {
    const ask = this.store.getState().ask;
    if (ask?.kind === 'grab') this.answer(ask, {key: 'Return'});
  }

  /** a click on a two-parameter diagram: shown and kept as the point
      AUTO's File/sElect 2par pt uses (docs/protocol.md `auto` `point`) */
  autoPoint(x: number, y: number): void {
    this.store.dispatch({type: 'diagram', action: {type: 'stored', at: {x, y}}});
    this.send({cmd: 'auto', op: 'point', xd: x, yd: y});
  }

  /** Back hides the AUTO panel (the core's window stays open); Show brings it back */
  showAuto(shown: boolean): void {
    this.store.dispatch({type: 'diagram', action: {type: 'show', shown}});
  }

  /* ---- values panel (docs/ui-v2.md T3, docs/protocol.md `set`/`slide`/`default`/`userbut`) ---- */

  /** remember an edit for Undo (A12), without sending anything: the caller sends `set` or `slide` itself */
  recordEdit(edit: ValueEdit): void {
    this.store.dispatch({type: 'values', action: {type: 'edit', edit}});
  }

  /** a parameter or initial condition box left with a new value */
  setValue(kind: 'par' | 'ic', name: string, text: string, previous: string): void {
    this.recordEdit({kind, name, previous});
    this.send({cmd: 'set', kind, name, text});
  }

  /** a boundary condition or delay box (by position: docs/protocol.md, BC names all read "0=") */
  setValueByIndex(kind: 'bc' | 'delay', index: number, text: string, previous: string): void {
    this.recordEdit({kind, index, previous});
    this.send({cmd: 'set', kind, index, text});
  }

  /** a slider dragged: only the latest position while busy matters, like the classic panel */
  slide(name: string, value: number): void {
    this.send({cmd: 'slide', name, value, rerun: 1});
  }

  /** Ctrl+Z or the Undo button: sends `set` again with the previous text (A12) */
  undoValue(): void {
    const {history} = this.store.getState().values;
    const last = history[history.length - 1];
    if (!last) return;
    this.store.dispatch({type: 'values', action: {type: 'undo'}});
    if (last.index !== undefined) this.send({cmd: 'set', kind: last.kind, index: last.index, text: last.previous});
    else this.send({cmd: 'set', kind: last.kind, name: last.name, text: last.previous});
  }

  /** the Default button: values from the ODE file (not itself undoable: A12) */
  defaultValues(kind: 'par' | 'ic'): void {
    this.store.dispatch({type: 'values', action: {type: 'defaulted', kind}});
    this.send({cmd: 'default', kind});
  }

  /** an `@ button` of the ODE file */
  userButton(index: number): void {
    this.send({cmd: 'userbut', index});
  }

  /* ---- data table (docs/ui-v2.md T10, docs/protocol.md `browser`) ---- */

  openTable(): void {
    this.store.dispatch({type: 'table', action: {type: 'open', open: true}});
  }

  /** stops the core sending further blocks (docs/protocol.md: `count` 0 stops the updates) */
  closeTable(): void {
    this.store.dispatch({type: 'table', action: {type: 'open', open: false}});
    this.send({cmd: 'browser', from: 0, count: 0});
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
    const {page, pendingKey} = this.store.getState().table;
    const req = planRequest(page, visibleFrom, visibleCount);
    if (!req) return;
    const key = JSON.stringify(req);
    if (key === pendingKey) return;
    this.store.dispatch({type: 'table', action: {type: 'requested', req}});
    this.send({cmd: 'browser', ...req});
  }

  /** one of the browser's buttons, on the selected row (docs/protocol.md
      `browser` op; Find, Replace, Table, Load and Write prompt through the
      ordinary `ask`, AskDialog already shows) */
  browserOp(op: BrowserOp): void {
    this.send({cmd: 'browser', op, row: this.store.getState().table.selected});
  }

  /** Get: the selected row becomes the initial conditions (the next `state` has them) */
  getRow(): void {
    this.browserOp('get');
  }

  /** every stored row as CSV (A14), fetched from the core block by block
      and kept in the store (lastExport) for tests; the caller offers it as
      a download. The view asks for nothing meanwhile (table.exporting), and
      asks for its rows again once the export is done. */
  async exportTableCsv(): Promise<string> {
    const rows = this.store.getState().table.page?.rows ?? 0;
    this.store.dispatch({type: 'table', action: {type: 'exporting'}});
    const blocks: BrowserEvent[] = [];
    for (let from = 0; from < rows; from += MAX_COUNT)
      blocks.push(await this.browserBlock(from, MAX_COUNT));
    const csv = tableCsv(blocks);
    this.store.dispatch({type: 'table', action: {type: 'exported', csv}});
    return csv;
  }

  /** rows [from, from+count) of every column: the core answers a request
      with `from` at once and in order (a control line, docs/protocol.md) */
  private browserBlock(from: number, count: number): Promise<BrowserEvent> {
    return new Promise(resolve => {
      const before = this.store.getState().table.page;
      const stop = this.store.subscribe(() => {
        const page = this.store.getState().table.page;
        if (page && page !== before && page.from === from) {
          stop();
          resolve(page);
        }
      });
      this.send({cmd: 'browser', from, count, col: 1, ncol: MAX_NCOL});
    });
  }

  /* ---- animation (docs/ui-v2.md T13, docs/protocol.md `ani` and "The animation as data") ---- */

  /** shows the panel, opening the core's animation window first (Viewaxes/Toon) when there is none */
  openAni(): void {
    this.store.dispatch({type: 'ani', action: {type: 'open', open: true}});
    const {ani, busy} = this.store.getState();
    if (!ani.exists && !busy) this.keys('v', 't');
  }

  /** hides the panel; a playing animation stops */
  closeAni(): void {
    this.aniPause();
    this.store.dispatch({type: 'ani', action: {type: 'open', open: false}});
  }

  /** File: an .ani file, through the file ask (the browser's open dialog) */
  aniLoad(): void {
    this.send({cmd: 'ani', op: 'file'});
  }

  /** Go: plays from the core's position to the last frame; never started by the page itself (A6) */
  aniPlay(): void {
    if (!this.store.getState().ani.playing) this.send({cmd: 'ani', op: 'go'});
  }

  /** Pause: reaches the running Go at once (a control line: no idle of its own) */
  aniPause(): void {
    if (this.store.getState().ani.playing) this.send({cmd: 'ani', op: 'pause'});
  }

  /** n frames from the frame shown (a playing animation pauses there first) */
  aniStep(n: number): void {
    const ani = this.store.getState().ani, target = stepTarget(ani, n);
    if (ani.playing) this.aniSeek(target);
    else this.send({cmd: 'ani', op: 'step', n: target - ani.pos});
  }

  /** the frame of stored row `pos` */
  aniSeek(pos: number): void {
    this.aniPause();
    this.send({cmd: 'ani', op: 'seek', pos: Math.max(0, Math.round(pos))});
  }

  /** the delay between two frames of Go, ms (reaches a running Go at once) */
  aniSpeed(ms: number): void {
    this.send({cmd: 'ani', op: 'speed', ms: Math.max(0, Math.round(ms))});
  }

  /** Grab: the frame's grab points wait for the pointer */
  aniGrab(): void {
    this.send({cmd: 'ani', op: 'grab'});
  }

  /** the pointer over the picture while grabbing, in unit coordinates (u, v: y up) */
  aniPointer(what: 'down' | 'move' | 'up', u: number, v: number): void {
    this.send({cmd: 'ani', op: 'mouse', what, u, v});
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

  /** the core's Kinescope menu (keys k then the item's own mnemonic); a no-op
      while a command is already running, like the rest of the menu keys */
  private kinescopeMenu(item: string): void {
    if (!this.store.getState().busy) this.keys('k', item);
  }

  kinescopeCapture(): void {
    this.kinescopeMenu('c');
  }

  kinescopeReset(): void {
    this.kinescopeMenu('r');
  }

  kinescopePlay(): void {
    this.kinescopeMenu('p');
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
    } else if (typeof ask.win === 'number') {
      pixels = chartOf(ask.win)?.pixels() ?? null;
      if (!pixels) {
        const w = windowOf(this.store.getState().plots, ask.win);
        if (w) pixels = renderFrame(snapshotWindow(w), dark);
      }
    }
    if (pixels) this.answer(ask, {w: pixels.w, h: pixels.h, rgb: bytesToBase64(pixels.rgb)});
    else this.cancel(ask);
  }

  /** every captured frame's picture, rendered the same way a `pixels` ask
      is (plot/kinescopeRender.ts): null when there is nothing captured */
  kinescopeFramePixels(): {w: number; h: number; rgb: Uint8ClampedArray}[] | null {
    const dark = document.documentElement.dataset.theme === 'dark';
    const frames = this.store.getState().kinescope.frames;
    if (!frames.length) return null;
    const rendered = frames.map((f: KinescopeFrame) => renderFrame(f, dark));
    return rendered.every(p => p !== null) ? rendered as {w: number; h: number; rgb: Uint8ClampedArray}[] : null;
  }

  /** an animated GIF of the captured frames (plot/gif.ts), delay ms apart
      (the autoplay speed set last, or the kinescope slice's default) */
  kinescopeGifBytes(): Uint8Array | null {
    const pixels = this.kinescopeFramePixels();
    if (!pixels) return null;
    return encodeGif(pixels.map(p => ({w: p.w, h: p.h, rgb: p.rgb})), this.store.getState().kinescope.delay);
  }

  /** Export GIF: built in the client, downloaded through the browser (docs/ui-v2.md
      "GIF/PNG from the client") */
  downloadKinescopeGif(name = 'xpp-kinescope.gif'): void {
    const bytes = this.kinescopeGifBytes();
    if (!bytes) return;
    const url = URL.createObjectURL(new Blob([bytes as unknown as BlobPart], {type: 'image/gif'}));
    download(name, url);
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }

  /* ---- text views (docs/ui-v2.md T16, docs/protocol.md `equations`, `source`, ---- */
  /* `action`, `equilibrium`, `eqimport`): equations, the source with its comment
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
    if (tab === 'equations') {
      this.send({cmd: 'equations'});
    } else if (tab === 'source') {
      /* File/Prt src (docs/protocol.md: no direct command, only the menu
         key sequence): 'f' switches the core's own menu to File (a no-op
         once already there) unless the numerics menu is open, which reads
         a plain key as one of its own items and must be left with Escape
         first (core/commands.c commander(), help_menu); 'p' runs Prt src
         and the core returns to the main menu on its own afterward. */
      const menu = this.store.getState().core?.menu ?? 0;
      if (menu === 2) this.key('Escape');
      if (menu !== 1) this.key('f');
      this.key('p');
    }
    /* 'equilibrium' has no fetch of its own: it is the last Sing pts result
       the core sent (Sing pts/Go, session.findEquilibrium()) and stays
       shown until the next one */
  }

  /** run the action of comment `index` of the source's comments
      (docs/protocol.md `action`): a comment with a `{par=value,...}` block,
      shown as a button on its line (ui/TextViews.tsx) */
  runAction(index: number): void {
    this.send({cmd: 'action', index});
  }

  /** Sing pts/Go: find the equilibrium closest to the current initial
      conditions; its result arrives as the `equilibrium` event. The core
      asks "Print eigenvalues?" (a plain `ask` `choice`, its own `y`/`n`
      keys) before it sends that event; answered `n` here since the answer
      only controls whether they are also printed to the log at INFO: the
      event carries them either way (protocol/types.ts EquilibriumEvent). */
  findEquilibrium(): void {
    this.keys('s', 'g', 'n');
  }

  /** the equilibrium window's Import: the last equilibrium becomes the
      initial conditions (the next `state` has them) */
  importEquilibrium(): void {
    this.send({cmd: 'eqimport'});
  }

  /* ---- files (docs/ui-v2.md section 4, T5): the model's folder is the workspace ---- */

  private failed(text: string): void {
    this.store.dispatch({type: 'toast', kind: 'error', text});
  }

  /** the files picked for a `file` ask for reading, copied into the model's
      folder (one already there with the same content is not copied; one
      with other content only after the replace confirm), then the ask
      answered with the name its pattern matches. Resolves false when
      nothing was answered: cancelled at the confirm, or a failure, which a
      notification reports. */
  async openFiles(ask: AskEvent, picked: File[]): Promise<boolean> {
    if (!this.files || !picked.length) return false;
    try {
      const listing = await this.files.list();
      this.store.dispatch({type: 'files', action: {type: 'listing', files: listing}});
      const taken = new Set(listing.map(f => f.name)), uploads: Upload[] = [];
      for (const file of picked) {
        if (!safeName(file.name)) {
          this.failed(`XPP cannot use a file named “${file.name}” in the model's folder. Rename it and pick it again.`);
          return false;
        }
        if (file.size > FILE_CAP) {
          this.failed(`${file.name} is larger than 64 MB, the most the model's folder takes from the page.`);
          return false;
        }
        const sha256 = await sha256Hex(file);
        const plan = uploadPlan(file.name, sha256, listing);
        let name = file.name;
        if (plan === 'confirm') {
          const keepBoth = keepBothName(file.name, taken);
          const choice = await this.confirmReplace(ask, file.name, keepBoth);
          if (choice === 'cancel') return false;
          if (choice === 'keep') name = keepBoth;
        }
        if (plan !== 'same') await this.files.put(name, file);
        taken.add(name);
        uploads.push({picked: file.name, name, sha256, copied: plan !== 'same'});
      }
      this.store.dispatch({type: 'files', action: {type: 'uploaded', uploads}});
      if (this.store.getState().ask?.id !== ask.id) return false; /* the prompt went meanwhile */
      const chosen = answerName(uploads.map(u => u.picked), ask.wild);
      this.answer(ask, {file: uploads.find(u => u.picked === chosen)!.name});
      return true;
    } catch (e) {
      this.failed(e instanceof Error ? e.message : String(e));
      return false;
    }
  }

  /** the folder has `name` with other content: Replace, Keep both or Cancel (the dialog asks) */
  private confirmReplace(ask: AskEvent, name: string, keepBoth: string): Promise<ReplaceChoice> {
    this.replaceChoice?.('cancel');
    this.store.dispatch({type: 'files', action: {type: 'confirm', confirm: {ask: ask.id, name, keepBoth}}});
    return new Promise(resolve => {
      this.replaceChoice = resolve;
    });
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
    this.pendingSave = {name, handle};
    this.answer(ask, {file: name});
  }

  private async deliver(name: string, handle: SaveHandle | null): Promise<void> {
    if (!this.files) return;
    try {
      const data = await this.files.get(name);
      if (!data) return; /* not written: the command stopped before it */
      if (handle) await writeTo(handle, data);
      else offerDownload(name, data);
      const how = handle ? 'picker' as const : 'download' as const;
      this.store.dispatch({type: 'files', action: {type: 'offered', offered: {name, size: data.size, sha256: await sha256Hex(data), how}}});
    } catch (e) {
      this.failed(`${name} is in the model's folder, but copying it failed: ${e instanceof Error ? e.message : String(e)}`);
    }
  }

  /** "Add file…" of a notification: `file` goes into the model's folder
      under the name the core could not open, and the command runs again */
  async addMissingFile(toastId: number, file: File): Promise<void> {
    const action = this.store.getState().toasts.find(t => t.id === toastId)?.action;
    if (!this.files || !action) return;
    try {
      if (file.size > FILE_CAP) throw new Error(`${file.name} is larger than 64 MB, the most the model's folder takes from the page.`);
      await this.files.put(action.name, file);
    } catch (e) {
      this.failed(e instanceof Error ? e.message : String(e));
      return;
    }
    this.store.dispatch({type: 'dismiss', id: toastId});
    const {run} = action, state = this.store.getState();
    const keys = run ? menuKeys(state.core?.menu ?? 0, run.menu) : null;
    if (!run || !keys || state.busy) {
      this.store.dispatch({type: 'toast', kind: 'info', text: `${action.name} is in the model's folder now. Run the command again.`});
      return;
    }
    this.replayAnswers = run.answers.slice();
    this.replayIdles = keys.length + 1;
    for (const k of keys) this.key(k);
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
    this.store.dispatch({type: 'aplot', action: {type: 'panel', open: true}});
    if (windowOpen) this.aplotOp('redraw');
    else this.keys('v', 'a');
  }

  closeAplot(): void {
    this.store.dispatch({type: 'aplot', action: {type: 'panel', open: false}});
  }

  /** the classic array plot window's own buttons: Redraw, Edit (a form,
      AskDialog), Fit, Range, Print, GIF (a file ask, FileDialog), Close */
  aplotOp(op: AplotOp): void {
    this.send({cmd: 'aplot', op});
  }

  /** the picture scrolled through time by dragging, wheeling or a keyboard
      step (plot/aplotScroll.ts): several gestures while the core is busy
      collapse into the one `scroll` it can act on next */
  aplotScroll(dy: number): void {
    const {send, pending} = accumulateScroll(this.pendingAplotDy, dy, this.store.getState().busy);
    this.pendingAplotDy = pending;
    if (send) this.send({cmd: 'aplot', op: 'scroll', dy: send});
  }

  /** a key the array plot's own hotkeys use (ui/AplotView.tsx); false when
      `key` is not one of them, so the caller can fall through to XPP's own */
  aplotKeyScroll(key: string): boolean {
    const dy = keyScroll(key, this.store.getState().aplot.event?.ny ?? 1);
    if (dy === null) return false;
    this.aplotScroll(dy);
    return true;
  }

  setAplotColorMap(map: AplotColorMap): void {
    this.store.dispatch({type: 'aplot', action: {type: 'colorMap', map}});
  }

  /** the cell under the pointer, or null off the grid */
  aplotHover(hover: AplotHover | null): void {
    this.store.dispatch({type: 'aplot', action: {type: 'hover', hover}});
  }
}

/** the largest file the model's folder takes from the page (core/xpp_files.h XPP_FILES_CAP) */
const FILE_CAP = 64 * 1024 * 1024;
