/* How components reach the session and read the store. */
import {createContext} from 'preact';
import {useContext, useEffect, useReducer, useRef} from 'preact/hooks';
import type {LayerWindow} from '../protocol/kinds';
import type {Command} from '../protocol/types';
import type {Session} from '../session';
import type {AppState} from '../store/state';

/** the title of a control whose kind waits while the core computes (W95: data and computation) */
export const BUSY_TITLE = 'Not while a computation runs: available when it ends';

export const SessionContext = createContext<Session | null>(null);

export function useSession(): Session {
  const s = useContext(SessionContext);
  if (!s) throw new Error('no session');
  return s;
}

/** a slice of the state; the component renders again only when the slice changes */
export function useStore<T>(select: (s: AppState) => T): T {
  const session = useSession();
  const [, force] = useReducer((n: number) => n + 1, 0);
  const selected = select(session.store.getState());
  const last = useRef(selected), selector = useRef(select);
  last.current = selected;
  selector.current = select;
  useEffect(() => {
    const check = () => {
      if (!Object.is(selector.current(session.store.getState()), last.current)) force(0);
    };
    const stop = session.store.subscribe(check);
    /* an update between this render and the subscription (a command the
       mount itself sent, answered at once) would otherwise go unseen */
    check();
    return stop;
  }, [session]);
  return selected;
}

/** whether a command may go out now (Session.may: by its kind, protocol/kinds.ts, W95),
    for a control's `disabled`; the component renders again when that can change */
export function useMay(): (cmd: Command) => boolean {
  const session = useSession();
  useStore(s => s.computing);
  useStore(s => s.ask);
  useStore(s => s.hello);
  useStore(s => s.core?.menu);
  useStore(s => s.player.running);
  return cmd => session.may(cmd);
}

/** the same for item `id` of the main menu (Session.mayMain: initialconds, window, ...) */
export function useMayMain(): (id: string) => boolean {
  const session = useSession();
  useMay();
  return id => session.mayMain(id);
}

/** the same for item `id` of window `win`'s key layer (Session.mayKey) */
export function useMayKey(): (win: LayerWindow, id: string) => boolean {
  const session = useSession();
  useMay();
  return (win, id) => session.mayKey(win, id);
}
