/* The session: connects a transport to the store and is the one place that
   sends commands. Components call its methods, never the transport. */
import type {Transport} from './protocol/transport';
import type {AskEvent, Command, XppEvent} from './protocol/types';
import {createStore, type Store} from './store/store';
import {initialState, reduce, type Action, type AppState} from './store/state';
import {planRequest, tableCsv} from './store/table';
import type {ValueEdit} from './store/values';

/** the data browser's buttons (docs/protocol.md `browser` op; web/xpp-client.js's BROWSER_BUTTONS) */
export type BrowserOp = 'find' | 'get' | 'replace' | 'unreplace' | 'table' | 'load' | 'write' | 'first' | 'last'
  | 'restore' | 'addcol' | 'delcol';

export class Session {
  readonly store: Store<AppState, Action>;
  /** keys that answer the menus a key sequence opens ("i g": Initialconds, Go) */
  private pendingKeys: string[] = [];

  constructor(private readonly transport: Transport) {
    this.store = createStore(reduce, initialState);
  }

  start(): void {
    this.transport.open(ev => this.receive(ev), open => this.store.dispatch({type: 'connection', open}));
  }

  private receive(ev: XppEvent): void {
    this.store.dispatch({type: 'event', ev});
    if (ev.ev === 'hello') {
      /* the plot as data (docs/protocol.md): asked for on every (re)connection,
         which also makes the server send the current plot; values as base64
         float32, which a long run needs (a server that does not know enc
         sends JSON numbers, which the store reads as well) */
      if (ev.features?.includes('series')) this.send({cmd: 'data', events: ['series'], enc: 'f32'});
    } else if (ev.ev === 'ask') {
      if (ev.kind === 'pixels') {
        this.cancel(ev); /* frame and GIF writers want the client's picture; this UI has none yet */
      } else if (ev.kind === 'alert') {
        /* an alert only informs: a notification that does not stop the run */
        this.store.dispatch({type: 'toast', kind: 'info', text: ev.message ?? ''});
        this.answer(ev, {});
      } else this.continueKeys(ev);
    } else if (ev.ev === 'idle') {
      this.pendingKeys = [];
    }
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

  /** stops the running command; it still ends with its idle */
  abort(): void {
    this.store.dispatch({type: 'aborting'});
    this.transport.send({cmd: 'abort'});
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

  /** the cached page as CSV (docs/ui-v2.md T10: "CSV export done in the
      client from fetched data"); returns the text so the caller can offer
      it as a download (plot/export.ts's download()) and tests can read it
      through __xpp.state().table.lastExport without one */
  exportTableCsv(): string {
    const csv = tableCsv(this.store.getState().table.page);
    this.store.dispatch({type: 'table', action: {type: 'exported', csv}});
    return csv;
  }
}
