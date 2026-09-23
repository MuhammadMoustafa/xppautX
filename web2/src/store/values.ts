/* Parameters, initial conditions, boundary conditions and delays
   (docs/protocol.md `set`, `slide`, `default`; docs/ui-v2.md T3): the
   values themselves stay in AppState.core (the `state` event), sent by the
   core. This slice is only what the page adds: which field an edit is
   pending on (so a `message` `error` can be shown on that field, not as a
   modal, A11/A14), and the undo history (A12: session.ts's undoValue()
   sends `set` again with the previous text). Pure: no DOM, no I/O. */

export type ValueKind = 'par' | 'ic' | 'bc' | 'delay';

/** one committed edit, kept for Undo; `name` for par/ic, `index` for bc/delay (docs/protocol.md `set`) */
export interface ValueEdit {
  kind: ValueKind;
  name?: string;
  index?: number;
  /** the field's text before this edit, to send back on Undo */
  previous: string;
}

export interface ValuesState {
  /** field id -> the error the core sent for it, until the next edit on it or a clean idle */
  errors: Record<string, string>;
  /** the field id the next `message` `error` is attributed to (the edit most recently sent, until its `idle`) */
  pending: string | null;
  /** committed edits, oldest first */
  history: ValueEdit[];
}

export const initialValues: ValuesState = {errors: {}, pending: null, history: []};

const HISTORY_KEEP = 50;

/** a field's identity as a store key: names fold case, as the core matches them (docs/protocol.md `set`) */
export function fieldKey(kind: ValueKind, nameOrIndex: string | number): string {
  return `${kind}:${typeof nameOrIndex === 'number' ? nameOrIndex : nameOrIndex.toLowerCase()}`;
}

function omit(o: Record<string, string>, key: string): Record<string, string> {
  if (!(key in o)) return o;
  const rest = {...o};
  delete rest[key];
  return rest;
}

export type ValuesAction =
  | {type: 'edit'; edit: ValueEdit}
  | {type: 'undo'}
  | {type: 'error'; text: string}
  | {type: 'settled'}
  /** the Default button: values from the ODE file (docs/protocol.md `default`); not itself undoable (A12) */
  | {type: 'defaulted'; kind: ValueKind};

export function reduceValues(state: ValuesState, action: ValuesAction): ValuesState {
  switch (action.type) {
    case 'edit': {
      const field = fieldKey(action.edit.kind, action.edit.index ?? action.edit.name!);
      const history = [...state.history, action.edit].slice(-HISTORY_KEEP);
      return {errors: omit(state.errors, field), pending: field, history};
    }
    case 'undo': {
      if (!state.history.length) return state;
      const last = state.history[state.history.length - 1];
      const field = fieldKey(last.kind, last.index ?? last.name!);
      return {errors: omit(state.errors, field), pending: field, history: state.history.slice(0, -1)};
    }
    case 'error':
      return state.pending ? {...state, errors: {...state.errors, [state.pending]: action.text}} : state;
    case 'settled':
      return state.pending ? {...state, pending: null} : state;
    case 'defaulted': {
      const prefix = `${action.kind}:`;
      const keys = Object.keys(state.errors).filter(k => k.startsWith(prefix));
      if (!keys.length) return state;
      const errors = {...state.errors};
      for (const k of keys) delete errors[k];
      return {...state, errors};
    }
  }
}

/** display precision (A14): six significant digits */
export function sixSig(n: number): string {
  return Number.isFinite(n) ? String(Number(n.toPrecision(6))) : String(n);
}
