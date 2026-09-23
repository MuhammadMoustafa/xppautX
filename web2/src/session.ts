/* The session: connects a transport to the store and is the one place that
   sends commands. Components call its methods, never the transport. */
import type {Transport} from './protocol/transport';
import type {AskEvent, Command, XppEvent} from './protocol/types';
import {createStore, type Store} from './store/store';
import {initialState, reduce, type Action, type AppState} from './store/state';

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
         which also makes the server send the current plot */
      if (ev.features?.includes('series')) this.send({cmd: 'data', events: ['series']});
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
}
