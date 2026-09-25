/* The data table (docs/ui-v2.md T10, docs/protocol.md `browser`): the
   stored rows -- T and every variable and aux, full precision (A14) -- as
   a virtualized grid: only the rows the viewport can show are ever in the
   DOM, fetched from the core in pages as the user scrolls or moves the
   selection (session.fetchTableRows, store/table.ts planRequest). It is
   always a floating overlay (R6: a panel anchored to the side from 48rem,
   a full-screen sheet under that), opened by the title bar's Table button
   and closed by Back or Escape, focus moving in and back like the values
   panel (ui/ValuesPanel.tsx).

   The buttons are the core's own (docs/protocol.md `browser` op), acting
   on the selected row: Find and Replace prompt through the ordinary `ask`
   (AskDialog already renders a form for them), Load and Write through the
   file ask (a Cancel-only placeholder until T5). Export CSV is the client's
   own: the rows fetched so far (docs/ui-v2.md T10, "CSV export done in the
   client from fetched data"). */
import {useEffect, useRef, useState} from 'preact/hooks';
import {useFocusBackOnClose} from './focusBack';
import {HELP} from '../help/links';
import {download} from '../plot/export';
import type {BrowserOp} from '../session';
import {rowAt} from '../store/table';
import {useSession, useStore} from './context';
import {HelpButton} from './HelpButton';
import {FOCUSABLE} from './dialogFocus';


/** label, op, hint (web/xpp-client.js's BROWSER_BUTTONS, the same protocol) */
const BUTTONS: [string, BrowserOp, string][] = [
  ['Find', 'find', 'Find the row where a column is closest to a value'],
  ['Get', 'get', 'Make the selected row the initial conditions'],
  ['Replace', 'replace', 'Replace a column by a formula'],
  ['Unrepl', 'unreplace', 'Undo the last Replace'],
  ['Table', 'table', 'Write a column as a function table file'],
  ['Load', 'load', 'Load data from a file'],
  ['Write', 'write', 'Write the rows from First to Last to a file'],
  ['First', 'first', 'Start the range at the selected row'],
  ['Last', 'last', 'End the range at the selected row'],
  ['Restore', 'restore', 'Redraw the plot from the rows First to Last'],
  ['Add col', 'addcol', 'Add a column computed from a formula'],
  ['Del col', 'delcol', 'Delete a column'],
];

/** a stored value as the core sent it (docs/protocol.md `browser`), full
    precision (A14): whatever digits it was sent with, no rounding here */
function fmt(v: number | null | undefined): string {
  return v === null ? 'NaN' : v === undefined ? '' : String(v);
}

/** the row height the layout actually uses (var(--target): 2rem with a
    mouse, 2.75rem on touch, A8), measured from the rendered header row so
    the scroll math matches the CSS exactly */
function useRowHeight(ref: {current: HTMLElement | null}): number {
  const [h, setH] = useState(32);
  useEffect(() => {
    const measure = () => {
      const r = ref.current?.getBoundingClientRect().height;
      if (r) setH(r);
    };
    measure();
    const mq = matchMedia('(pointer: coarse)');
    const onChange = () => requestAnimationFrame(measure);
    mq.addEventListener('change', onChange);
    window.addEventListener('resize', onChange);
    return () => {
      mq.removeEventListener('change', onChange);
      window.removeEventListener('resize', onChange);
    };
  }, []);
  return h;
}

export function TableView() {
  const session = useSession();
  const open = useStore(s => s.table.open);
  const page = useStore(s => s.table.page);
  const exporting = useStore(s => s.table.exporting);
  const selected = useStore(s => s.table.selected);
  const coreRows = useStore(s => s.core?.rows ?? 0);
  const rows = Math.max(page?.rows ?? 0, coreRows);
  const cols = page?.cols ?? ['T'];

  const panel = useRef<HTMLElement>(null);
  const scroller = useRef<HTMLDivElement>(null);
  const headRow = useRef<HTMLDivElement>(null);
  const rowHeight = useRowHeight(headRow);
  const [scrollTop, setScrollTop] = useState(0);
  const [viewHeight, setViewHeight] = useState(0);

  const close = () => session.closeTable();

  /* focus in on open, back to the toggle on close; Escape closes it,
     wherever the focus is inside (narrow only: CSS keeps it open elsewhere) */
  useFocusBackOnClose(open, panel, '.table-toggle');
  useEffect(() => {
    if (!open) return;
    panel.current?.querySelector<HTMLElement>(FOCUSABLE)?.focus();
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== 'Escape' || session.store.getState().ask) return;
      e.preventDefault();
      e.stopPropagation();
      close();
    };
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, [open]);

  /* the visible window: only it (plus a small buffer) is ever fetched or rendered */
  useEffect(() => {
    const el = scroller.current;
    if (!open || !el) return;
    const onScroll = () => setScrollTop(el.scrollTop);
    const ro = new ResizeObserver(() => setViewHeight(el.clientHeight));
    ro.observe(el);
    el.addEventListener('scroll', onScroll);
    setViewHeight(el.clientHeight);
    setScrollTop(el.scrollTop);
    return () => {
      ro.disconnect();
      el.removeEventListener('scroll', onScroll);
    };
  }, [open]);

  const visibleCount = Math.max(1, Math.ceil((viewHeight || 1) / rowHeight) + 1);
  const first = Math.max(0, Math.floor(scrollTop / rowHeight));

  useEffect(() => {
    if (!open || !rows || exporting) return;
    session.fetchTableRows(first, visibleCount);
  }, [open, rows, first, visibleCount, page, exporting]);

  const scrollToRow = (row: number) => {
    const el = scroller.current;
    if (!el) return;
    const top = row * rowHeight, bottom = top + rowHeight, visible = viewHeight || el.clientHeight;
    if (top < el.scrollTop) el.scrollTop = top;
    else if (bottom > el.scrollTop + visible) el.scrollTop = bottom - visible;
  };

  const move = (row: number) => {
    const clamped = Math.max(0, Math.min(rows > 0 ? rows - 1 : 0, row));
    session.selectTableRow(clamped);
    scrollToRow(clamped);
  };

  /* arrows move the selection, PageUp/PageDown by a screenful, Home/End to
     the ends, Enter is Get (A3): stopped here so the plot's own hotkeys
     (ui/hotkeys.ts) do not also see them */
  const onKeyDown = (e: KeyboardEvent) => {
    const page1 = Math.max(1, visibleCount - 1);
    const moves: Record<string, number> = {ArrowUp: -1, ArrowDown: 1, PageUp: -page1, PageDown: page1};
    if (e.key in moves) {
      e.preventDefault();
      e.stopPropagation();
      move(selected + moves[e.key]);
    } else if (e.key === 'Home') {
      e.preventDefault();
      e.stopPropagation();
      move(0);
    } else if (e.key === 'End') {
      e.preventDefault();
      e.stopPropagation();
      move(rows - 1);
    } else if (e.key === 'Enter') {
      e.preventDefault();
      e.stopPropagation();
      session.getRow();
    }
  };

  const last = Math.min(rows, first + visibleCount);
  const items: {row: number; values: (number | null)[] | null}[] = [];
  for (let i = first; i < last; i++) items.push({row: i, values: rowAt(page, i)});

  const exportCsv = async () => {
    const csv = await session.exportTableCsv();
    const url = URL.createObjectURL(new Blob([csv], {type: 'text/csv'}));
    download('data.csv', url);
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  };

  return (
    <section id="table-panel" ref={panel} class={'table-panel' + (open ? ' open' : '')} aria-label="Data table">
      <div class="table-header">
        <button class="table-back" onClick={close}>Back</button>
        <h2>Data</h2>
        <HelpButton target={HELP.dataTab} label="the Data tab" />
        <button class="small" onClick={exportCsv} disabled={!page?.data.length || exporting}
          title="Save every stored row as a CSV file">
          {exporting ? 'Exporting…' : 'Export CSV'}
        </button>
      </div>
      <div class="table-tools">
        {BUTTONS.map(([label, op, hint]) => (
          <button key={op} title={hint} onClick={() => session.browserOp(op)}>{label}</button>
        ))}
      </div>
      <p class="table-info" role="status">
        {!rows ? 'No data yet: integrate first.'
          : `${rows} rows. Selected row ${selected}.`
            + (page ? ` First..Last: ${page.start}..${Math.max(page.start, page.end - 1)}.` : '')}
      </p>
      <div class="table-grid-wrap" ref={scroller} role="grid" aria-label="Stored data" aria-rowcount={rows + 1}
        aria-colcount={cols.length} aria-activedescendant={rows ? `table-row-${selected}` : undefined}
        tabIndex={0} onKeyDown={onKeyDown}>
        <div class="table-row table-row-head" role="row" aria-rowindex={1} ref={headRow}>
          {cols.map((c, i) => <span class="table-cell" role="columnheader" key={i} title={c}>{c}</span>)}
        </div>
        <div class="table-body" style={{height: `${rows * rowHeight}px`}}>
          {items.map(({row, values}) => (
            <div key={row} id={`table-row-${row}`} class={'table-row' + (row === selected ? ' selected' : '')}
              role="row" aria-rowindex={row + 2} aria-selected={row === selected}
              style={{top: `${row * rowHeight}px`}} onClick={() => move(row)}>
              {cols.map((_, i) => <span class="table-cell" role="gridcell" key={i}>{fmt(values?.[i])}</span>)}
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
