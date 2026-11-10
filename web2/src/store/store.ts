/* A minimal store: one immutable state, changed only by a pure reducer,
   observed by subscribers. Framework-free, so the reducer is testable in
   Node and the UI binds to it through one hook (ui/useStore.ts). */

export type Reducer<S, A> = (state: S, action: A) => S;
export type Listener = () => void;

export interface Store<S, A> {
  getState(): S;
  dispatch(action: A): void;
  /** runs `actions`, one dispatch each, and tells the subscribers once, at the end */
  batch(actions: () => void): void;
  subscribe(listener: Listener): () => void;
}

export function createStore<S, A>(reducer: Reducer<S, A>, initial: S): Store<S, A> {
  let state = initial;
  const listeners = new Set<Listener>();
  let batching = 0, changed = false;
  const notify = () => {
    changed = false;
    for (const l of [...listeners]) l();
  };
  return {
    getState: () => state,
    dispatch(action) {
      const next = reducer(state, action);
      if (next === state) return;
      state = next;
      if (batching) changed = true;
      else notify();
    },
    batch(actions) {
      batching++;
      try {
        actions();
      } finally {
        if (--batching === 0 && changed) notify();
      }
    },
    subscribe(listener) {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
  };
}
