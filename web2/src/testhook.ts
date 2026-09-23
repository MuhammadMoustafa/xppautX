/* What tests read and drive (tools/web2check.mjs): the store's state, the
   actions it took (newest last) and the plot's own state, never pixels.
   window.__xpp exists in every build; only `send` changes anything, and it
   is what the UI itself does. */
import {currentChart} from './plot/registry';
import type {Session} from './session';
import type {Action} from './store/state';

const KEEP = 200;

export function installTestHook(session: Session): void {
  const actions: string[] = [];
  const dispatch = session.store.dispatch;
  session.store.dispatch = (a: Action) => {
    actions.push(a.type === 'event' ? `event:${a.ev.ev}` : a.type === 'viewport' ? `viewport${a.push ? ':push' : ''}` : a.type);
    if (actions.length > KEEP) actions.shift();
    dispatch(a);
  };
  (window as unknown as {__xpp: unknown}).__xpp = {
    state: () => session.store.getState(),
    actions: () => actions.slice(),
    plot: () => currentChart()?.info() ?? null,
    send: (cmd: {cmd: string}) => session.send(cmd),
  };
}
