/* Text views (docs/ui-v2.md T16): the model's equations, its source (with
   comment actions), and the last Sing pts equilibrium. All three come from
   the core as plain data (docs/protocol.md `equations`, `source`,
   `equilibrium`); this slice only keeps the latest of each plus the panel's
   own open/tab state (R6: a side panel from 48rem, a full-screen sheet
   under that, like store/table.ts's TableState). Pure: no DOM, no I/O. */
import type {EquationsEvent, EquilibriumEvent, SourceEvent} from '../protocol/types';

export type TextTab = 'equations' | 'source' | 'equilibrium';

/** one comment of the source, decoded from the event's [text, hasAction] tuple */
export interface SourceComment {
  text: string;
  hasAction: boolean;
}

/** a source line, with the comment it is (a `"..."` line), if any, and the
    `action` command index to run when its button is picked */
export interface SourceLine {
  text: string;
  comment?: {text: string; hasAction: boolean; index: number};
}

export interface EquilibriumInfo {
  type: string;
  cplus: number;
  cminus: number;
  rplus: number;
  rminus: number;
  im: number;
  values: [string, number][];
}

export interface TextState {
  /** the sheet/side panel is open (R6) */
  open: boolean;
  /** which of the three views is shown */
  tab: TextTab;
  equations: string[] | null;
  source: {lines: SourceLine[]; comments: SourceComment[]} | null;
  equilibrium: EquilibriumInfo | null;
}

export const initialText: TextState = {open: false, tab: 'equations', equations: null, source: null, equilibrium: null};

/** the event's raw [text, 0|1] tuples as typed comments */
export function parseComments(raw: [string, number][]): SourceComment[] {
  return raw.map(([text, flag]) => ({text, hasAction: !!flag}));
}

/** pairs the source's lines with their comments: every line starting with
    `"` is one more comment, in the order both arrays list them (core's
    save_eqn and comments[] are filled in the same top-to-bottom pass over
    the file, core/form_ode.cpp parse_a_string/add_comment) */
export function attachComments(lines: string[], comments: SourceComment[]): SourceLine[] {
  let next = 0;
  return lines.map(text => {
    if (text.startsWith('"') && next < comments.length) {
      const c = comments[next];
      return {text, comment: {text: c.text, hasAction: c.hasAction, index: next++}};
    }
    return {text};
  });
}

export type TextAction =
  | {type: 'open'; open: boolean}
  | {type: 'tab'; tab: TextTab}
  | {type: 'equations'; ev: EquationsEvent}
  | {type: 'source'; ev: SourceEvent}
  | {type: 'equilibrium'; ev: EquilibriumEvent};

export function reduceText(state: TextState, action: TextAction): TextState {
  switch (action.type) {
    case 'open':
      return action.open === state.open ? state : {...state, open: action.open};
    case 'tab':
      return action.tab === state.tab ? state : {...state, tab: action.tab};
    case 'equations':
      return {...state, equations: action.ev.lines};
    case 'source': {
      const comments = parseComments(action.ev.comments);
      return {...state, source: {lines: attachComments(action.ev.lines, comments), comments}};
    }
    case 'equilibrium': {
      const {type, cplus, cminus, rplus, rminus, im, values} = action.ev;
      return {...state, equilibrium: {type, cplus, cminus, rplus, rminus, im, values}};
    }
  }
}
