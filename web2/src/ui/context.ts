/* How components reach the session and read the store. */
import {createContext} from 'preact';
import {useContext, useEffect, useReducer, useRef} from 'preact/hooks';
import type {Session} from '../session';
import type {AppState} from '../store/state';

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
  useEffect(() => session.store.subscribe(() => {
    if (!Object.is(selector.current(session.store.getState()), last.current)) force(0);
  }), [session]);
  return selected;
}
