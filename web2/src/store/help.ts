/* The Help view's own state (docs/roadmap.md W12b): open/closed, the
   chapter and anchor shown, and the search box's query. Pure: no DOM, no
   I/O -- the manual's data (virtual:manual) lives in the component, not
   here, the same way store/table.ts holds paging state but not the rows'
   own schema. */
import type {HelpTarget} from '../help/links';

export interface HelpState {
  open: boolean;
  chapter: string;
  /** the heading to scroll to when `chapter` next renders; cleared once used */
  anchor: string | null;
  query: string;
}

export const HELP_HOME = '01-introduction';

export const initialHelp: HelpState = {open: false, chapter: HELP_HOME, anchor: null, query: ''};

export type HelpAction =
  | {type: 'open'; target?: HelpTarget}
  | {type: 'close'}
  | {type: 'go'; target: HelpTarget}
  | {type: 'query'; query: string};

export function reduceHelp(state: HelpState, action: HelpAction): HelpState {
  switch (action.type) {
    case 'open':
      /* a target opens Help there (its anchor, or the chapter's top if it
         names none -- an empty string, from a search hit above a chapter's
         first heading, means the same as no anchor); no target (F1, the
         title bar's Help button) reopens wherever it was left */
      return {
        ...state, open: true,
        chapter: action.target?.chapter ?? state.chapter,
        anchor: action.target ? (action.target.anchor || null) : state.anchor,
      };
    case 'close':
      return state.open ? {...state, open: false} : state;
    case 'go':
      return {...state, chapter: action.target.chapter, anchor: action.target.anchor || null};
    case 'query':
      return action.query === state.query ? state : {...state, query: action.query};
  }
}
