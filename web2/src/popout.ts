/* A view in a window of its own (W645, docs/ui-v2.md): a graph, an autoX view or a dock tab (Values, Table, Model,
   Help) popped out of the page it is a tab of. The pop-out is the same page at the same address (its own token, no new
   one) with the view named in the fragment, `#popout=<kind>:<name>`, which never reaches the server: it is one more
   client of the same session, so what it shows is live and what it sends runs as from the main window. In a browser
   it is another tab; in the desktop app a native window of the same process (`__xppPopOut`, core/xpp_window.cpp,
   bound in the main window only). The pages tell each other when a view goes out or comes back over a
   BroadcastChannel: same origin, no more than the pages already share. Pop in closes the pop-out (the window's own
   close box and the browser tab's × come to the same: `closed`), and closing the main window closes the pop-outs. */
import type {Store} from './store/store';
import {DOCK_TABS} from './store/dock';
import type {Action, AppState} from './store/state';

export type ViewKind = 'graph' | 'autox' | 'dock';

/** a view's identity: what the pop-out shows and what a tab is marked by */
export interface ViewKey {
  kind: ViewKind;
  name: string;
}

const KINDS: readonly ViewKind[] = ['graph', 'autox', 'dock'];
const KEY_SEPARATOR = ':';
const FRAGMENT = '#popout=';
const CHANNEL = 'xppautx-popout';
/** the window name a browser pop-out takes, so a second Pop out of one view reaches the window it has (a same-address
    navigation is a fragment one: nothing reloads) */
const TARGET_PREFIX = 'xppautx-popout-';

type Message = {type: 'closed' | 'popin'; key: string} | {type: 'main-closed'};

/** what the view is called: a graph's or autoX view's name, a dock tab's label */
export const viewLabel = (v: ViewKey): string => v.kind === 'dock' ? DOCK_TABS.find(t => t.id === v.name)?.label ?? v.name : v.name;

export const viewKeyText = (v: ViewKey): string => `${v.kind}${KEY_SEPARATOR}${v.name}`;

/** the view a page address names: null for the main window (no fragment of ours), an error text for a fragment that
    names no view */
export function popOutRequest(hash: string): ViewKey | string | null {
  if (!hash.startsWith(FRAGMENT)) return null;
  let text: string;
  try {
    text = decodeURIComponent(hash.slice(FRAGMENT.length));
  } catch {
    return 'The address names a view that is not written as a name.';
  }
  const at = text.indexOf(KEY_SEPARATOR);
  const kind = at > 0 ? KINDS.find(k => k === text.slice(0, at)) : undefined;
  const name = text.slice(at + 1);
  const known = kind === 'dock' ? DOCK_TABS.some(t => t.id === name) : !!name;
  return kind && known ? {kind, name} : `The address names no view: "${text}".`;
}

/** what the desktop window binds in the main window only (core/xpp_window.cpp): a window made for a key, or the
    existing one brought to the front */
type NativePopOut = (key: string, title: string) => Promise<unknown>;
const nativePopOut = (): NativePopOut | undefined => {
  const f = (globalThis as {__xppPopOut?: unknown}).__xppPopOut;
  return typeof f === 'function' ? (f as NativePopOut) : undefined;
};
/** what a desktop pop-out has: closing this window alone */
const nativeClosePopOut = (): (() => Promise<unknown>) | undefined => {
  const f = (globalThis as {__xppClosePopOut?: unknown}).__xppClosePopOut;
  return typeof f === 'function' ? (f as () => Promise<unknown>) : undefined;
};

export class PopOuts {
  private channel: BroadcastChannel | null = null;

  constructor(private readonly store: Store<AppState, Action>) {}

  isOut(view: ViewKey): boolean {
    return this.store.getState().popouts.includes(viewKeyText(view));
  }

  private post(m: Message): void {
    this.channel ??= new BroadcastChannel(CHANNEL);
    this.channel.postMessage(m);
  }

  private listen(on: (m: Message) => void): void {
    this.channel ??= new BroadcastChannel(CHANNEL);
    this.channel.onmessage = e => on(e.data as Message);
  }

  private mark(key: string, out: boolean): void {
    this.store.dispatch({type: 'popout', key, out});
  }

  /** The main window: hears of a view that came back, and tells the pop-outs when it goes. */
  startMain(): void {
    this.listen(m => {
      if (m.type === 'closed') this.mark(m.key, false);
    });
    window.addEventListener('pagehide', () => this.post({type: 'main-closed'}));
  }

  /** Pop out `view`, or bring its window to the front when it is out already (choosing its tab). */
  async show(view: ViewKey): Promise<void> {
    const key = viewKeyText(view);
    const native = nativePopOut();
    try {
      if (native) {
        await native(encodeURIComponent(key), `${viewLabel(view)} \u2014 xppautX`);
      } else {
        const url = `${location.origin}${location.pathname}${location.search}${FRAGMENT}${encodeURIComponent(key)}`;
        const win = window.open(url, TARGET_PREFIX + encodeURIComponent(key));
        if (!win) throw new Error('the browser blocked the new window: allow pop-ups for this page');
        win.focus();
      }
    } catch (e) {
      this.store.dispatch({type: 'toast', kind: 'error', text: `"${viewLabel(view)}" did not open in a window of its own: ${e instanceof Error ? e.message : String(e)}`});
      return;
    }
    this.mark(key, true);
  }

  /** Pop in from the main window: the pop-out closes itself (and tells this page, `closed`). */
  popIn(view: ViewKey): void {
    this.post({type: 'popin', key: viewKeyText(view)});
  }

  /** The pop-out showing `view`: closes when told to (Pop in from the main window), and when the main window closes. */
  startPopOut(view: ViewKey): void {
    const key = viewKeyText(view);
    this.listen(m => {
      if (m.type === 'main-closed' || (m.type === 'popin' && m.key === key)) this.close(key);
    });
    window.addEventListener('pagehide', () => this.post({type: 'closed', key}));
    (window as unknown as {__xppPopIn: () => void}).__xppPopIn = () => this.close(key);
  }

  /** Pop in, from the pop-out: the main window's tab returns, and this window goes (the app's native window, or
      the tab a script opened, which a browser lets it close). */
  close(key: string): void {
    this.post({type: 'closed', key});
    const native = nativeClosePopOut();
    if (native) void native().catch(() => undefined);
    else window.close();
  }
}
