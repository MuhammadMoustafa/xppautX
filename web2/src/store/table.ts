/* The data table (docs/ui-v2.md T10, docs/protocol.md `browser`): the core
   keeps one browser window (rows and, since we always ask for every
   column, every variable and aux too) and answers a
   {"cmd":"browser","from":,"count":,"col":1,"ncol":500} request with that
   block at once, even during a prompt or another command (a control line,
   never busy: state.ts's `sent` case knows not to toggle busy for it). This
   slice keeps the last block the core sent (`page`, docs/protocol.md's
   `browser` event) and the client's own selected row, which the view's
   buttons act on and which follows the core's own `row0` when a button
   (Find, Get, First, Last, ...) moves it there. No growing buffer: a
   scroll off the cached page asks again, like the classic browser
   (web/xpp-client.js). Pure: no DOM, no I/O, no timers. */
import type {BrowserEvent} from '../protocol/types';

export interface BrowserRequest {
  from: number;
  count: number;
  col: number;
  ncol: number;
}

export interface TableState {
  /** the sheet/floating panel is open (R6): a section under the plot from
      48rem, a full-screen sheet under that */
  open: boolean;
  /** the highlighted row: what the arrow keys move, and what the buttons
      (Get, Find's row, ...) act on; follows the core's row0 when it moves it */
  selected: number;
  /** the last block the core sent, or null before the first one */
  page: BrowserEvent | null;
  /** the request in flight (or last sent), so the same block is not asked twice */
  pendingKey: string | null;
  /** the text of the last Export CSV (A14: tests read it through __xpp, no download needed) */
  lastExport: string | null;
}

export const initialTable: TableState = {open: false, selected: 0, page: null, pendingKey: null, lastExport: null};

export type TableAction =
  | {type: 'open'; open: boolean}
  | {type: 'select'; row: number}
  | {type: 'event'; ev: BrowserEvent}
  | {type: 'requested'; req: BrowserRequest | null}
  | {type: 'exported'; csv: string};

export function reduceTable(state: TableState, action: TableAction): TableState {
  switch (action.type) {
    case 'open':
      return action.open === state.open ? state : {...state, open: action.open};
    case 'select': {
      const rows = state.page?.rows ?? 0;
      const row = Math.max(0, Math.min(rows > 0 ? rows - 1 : 0, action.row));
      return row === state.selected ? state : {...state, selected: row};
    }
    case 'event': {
      const ev = action.ev;
      /* the core moved the selection itself (Find, or after Get/First/Last
         and the like): the view follows it, as the classic browser does */
      const moved = !state.page || state.page.row0 !== ev.row0;
      return {...state, page: ev, pendingKey: null, selected: moved ? ev.row0 : state.selected};
    }
    case 'requested':
      return {...state, pendingKey: action.req ? JSON.stringify(action.req) : null};
    case 'exported':
      return {...state, lastExport: action.csv};
  }
}

/* ---- paging: what to ask for so the visible rows are in `page` ---- */

const ROW_BUFFER = 3; /* ask for 3x the visible window, like the classic browser: a small scroll needs nothing */
const MAX_COUNT = 2000; /* the core's own cap (core/ui_json.c browser_rows) */
const MAX_NCOL = 500; /* the core's own cap; comfortably more than a model has, so every column is always asked for */

/** the request to send so rows [visibleFrom, visibleFrom+visibleCount) are
    cached, or null when `page` already covers them. Always asks for every
    column (col 1, ncol 500): the core caps ncol at 500 regardless, and a
    table that shows "every variable and aux" (T10) never needs to page
    columns for an ordinary model. */
export function planRequest(page: BrowserEvent | null, visibleFrom: number, visibleCount: number): BrowserRequest | null {
  const from = Math.max(0, visibleFrom);
  const to = from + Math.max(0, visibleCount);
  const have = !!page && page.col === 1 && page.from <= from && page.from + page.data.length >= to;
  if (have) return null;
  return {
    from: Math.max(0, from - Math.max(1, visibleCount)),
    count: Math.min(MAX_COUNT, Math.max(1, visibleCount) * ROW_BUFFER),
    col: 1,
    ncol: MAX_NCOL,
  };
}

/** row `row`'s values as the core sent them (T then every column, `null`
    for NaN), or null when it is not in the cached page */
export function rowAt(page: BrowserEvent | null, row: number): (number | null)[] | null {
  if (!page || row < page.from || row >= page.from + page.data.length) return null;
  return page.data[row - page.from];
}

/** the cached page as CSV, full precision (A14): exactly the digits the
    core sent (docs/protocol.md `browser`: T at 8 significant digits, the
    other columns at 7 -- a client cannot show more precision than it was
    sent). Only the rows fetched so far, as docs/ui-v2.md T10 asks
    ("CSV export done in the client from fetched data"): Export CSV first,
    or scroll further and export again, for more of the run. */
export function tableCsv(page: BrowserEvent | null): string {
  if (!page || !page.data.length) return page ? page.cols.join(',') + '\n' : '';
  const header = page.cols.join(',');
  const lines = page.data.map(row => row.map(v => (v === null ? 'NaN' : String(v))).join(','));
  return [header, ...lines].join('\n') + '\n';
}
