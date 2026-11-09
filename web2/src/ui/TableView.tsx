/* The data table (docs/ui-v2.md T10, docs/protocol.md "The data table"): the
   stored rows -- T and every variable and aux, full precision (A14) -- as
   a virtualized grid: only the rows the viewport can show are ever in the
   DOM, fetched from the core in pages as the user scrolls or moves the
   selection (session.fetchTableRows, store/table.ts planRequest). From
   48rem it is the right column's dock (W648): it takes the Values panel's
   place under the header, the plot staying whole beside it, and Back to
   Values returns it; under that it is a full-screen sheet with a x on top
   (css/panels.css). Opened by the title bar's Table button and closed by
   Back or Escape, focus moving in and back like the values panel
   (ui/ValuesPanel.tsx).

   The buttons are the core's own (docs/protocol.md data.* commands), acting
   on the selected row, grouped as Find, Rows, Columns (and More): where the
   panel is narrow (a container query, css/panels.css) Rows and Columns are
   two menus. Find and Replace prompt through the ordinary `ask`. Load data
   and one Export data menu, one entry per format of data.export's
   `format` choice (the last used first, marked), are in the header; Export
   sends data.export through the session's one export path. */
import {openPanel} from './hotkeys';
import {useEffect, useRef, useState} from 'preact/hooks';
import {useFocusBackOnClose} from './focusBack';
import {ShownName} from './CutText';
import {HELP} from '../help/links';
import type {DataOp} from '../session';
import {DropDownMenu} from './SetsRow';
import {choiceLabel} from '../store/fieldKinds';
import {readPref, writePref} from '../store/prefs';
import {rowAt} from '../store/table';
import {BUSY_TITLE, useMay, useMayKey, useSession, useStore} from './context';
import {CommandLabel, useCommandLabel} from './CommandButton';
import {commandRow, toolbarRow} from './navigation';
import {HelpButton} from './HelpButton';
import {FOCUSABLE} from './dialogFocus';


/** the toolbar's groups: the ops of each, in order; each op is the row navigation.ts toolbarCommand names (its label and description) */
const ROWS: DataOp[] = ['first', 'last', 'restore'];
const COLUMNS: DataOp[] = ['addcol', 'replace', 'delcol'];
const MORE_COLUMNS: DataOp[] = ['unreplace', 'table', 'transpose'];

/** the format last exported, remembered per viewer (it is listed first and marked) */
const FORMAT_KEY = 'xpp.table.exportFormat';

/** a stored value (docs/protocol.md "The data table") as the core sent it, the store's shortest text (W613): whole where its
    column has room, cut with a tip only where it has not (CutText measures the box) */
function Cell({v}: {v: number | null | undefined}) {
  return v === undefined ? null : <ShownName name={v === null ? 'NaN' : String(v)} />;
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
  const may = useMay();
  const mayKey = useMayKey();
  const open = useStore(s => s.table.open);
  const hello = useStore(s => s.hello);
  const page = useStore(s => s.table.page);
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

  /* focus in on open, back to the toggle on close; Escape closes it (hotkeys.ts's one owner, openPanel) */
  useFocusBackOnClose(open, panel, '.table-toggle');
  useEffect(() => {
    if (!open) return;
    panel.current?.querySelector<HTMLElement>(FOCUSABLE)?.focus();
    return openPanel(close);
  }, [open]);

  /* the visible window: only it (plus a small buffer) is ever fetched or rendered */
  useEffect(() => {
    const el = scroller.current;
    if (!open || !el) return;
    /* an edge with more columns past it fades (css/panels.css .table-grid-wrap), so a cut column is not read as a value */
    const edges = () => {
      el.dataset.fadeLeft = String(el.scrollLeft > 1);
      el.dataset.fadeRight = String(el.scrollLeft + el.clientWidth < el.scrollWidth - 1);
    };
    const onScroll = () => { setScrollTop(el.scrollTop); edges(); };
    const ro = new ResizeObserver(() => { setViewHeight(el.clientHeight); edges(); });
    ro.observe(el);
    el.addEventListener('scroll', onScroll);
    setViewHeight(el.clientHeight);
    setScrollTop(el.scrollTop);
    edges();
    return () => {
      ro.disconnect();
      el.removeEventListener('scroll', onScroll);
    };
  }, [open, cols.length]);

  const visibleCount = Math.max(1, Math.ceil((viewHeight || 1) / rowHeight) + 1);
  const first = Math.max(0, Math.floor(scrollTop / rowHeight));

  useEffect(() => {
    if (!open || !rows) return;
    session.fetchTableRows(first, visibleCount);
  }, [open, rows, first, visibleCount, page]);

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

  const button = (op: DataOp, inMenu: boolean) => {
    const row = toolbarRow(hello, 'data', op);
    const off = !mayKey('data', op);
    const click = {key: op, 'data-button': `data:${op}`, title: off ? BUSY_TITLE : row?.description, disabled: off, onClick: () => session.dataOp(op)};
    return inMenu ? <li key={op}><button role="menuitem" {...click}>{row?.label}</button></li> : <button {...click}>{row?.label}</button>;
  };
  const group = (name: string, ops: DataOp[], more: DataOp[] = []) => (
    <>
      <div class="table-group table-wide" role="group" aria-label={name}>
        <span class="table-group-label">{name}</span>
        {ops.map(op => button(op, false))}
        {more.length > 0 && <DropDownMenu normal id={`table-more-${name}`} label="More">{more.map(op => button(op, true))}</DropDownMenu>}
      </div>
      <div class="table-group table-narrow">
        <DropDownMenu normal id={`table-${name}`} label={name}>{[...ops, ...more].map(op => button(op, true))}</DropDownMenu>
      </div>
    </>
  );

  /* Export data: one entry per format of data.export's `format` choice, the last used first and marked */
  const formats = commandRow(hello, 'data.export')?.arguments?.find(a => a.name === 'format')?.choices ?? [];
  const [lastFormat, setLastFormat] = useState(() => readPref(FORMAT_KEY, v => (typeof v === 'string' ? v : null), ''));
  const ordered = [...formats].sort((a, b) => Number(b === lastFormat) - Number(a === lastFormat));
  const exportAs = (format: string) => {
    setLastFormat(format);
    writePref(FORMAT_KEY, format);
    session.writeDataFile('table', format);
  };
  const exportOff = !page?.data.length || !may({cmd: 'data.export'});
  const selectedT = rowAt(page, selected)?.[0];
  const sliced = page && (page.start > 0 || page.end < rows);

  return (
    <section id="table-panel" ref={panel} class={'table-panel panel-sheet' + (open ? ' open' : '')} aria-label={useCommandLabel('data.open')}>
      <div class="panel-header">
        <button class="table-back" onClick={close}>Back to Values</button>
        <button class="table-close" onClick={close} aria-label="Close" title="Close">×</button>
        <div class="table-title">
          <h2><CommandLabel command="data.open" /></h2>
          <HelpButton target={HELP.dataTab} label="the Data tab" />
        </div>
        <div class="table-actions">
        {button('load', false)}
        <DropDownMenu normal id="table-export" label={commandRow(hello, 'data.export')?.label ?? ''}>
          {ordered.map(format => (
            <li key={format}>
              <button role="menuitem" data-export-format={format} disabled={exportOff}
                title={exportOff ? BUSY_TITLE : `Export every stored row as ${choiceLabel(format)}${format === lastFormat ? ' (used last)' : ''}`}
                onClick={() => exportAs(format)}>{format === lastFormat ? '✓ ' : ''}{choiceLabel(format)}</button>
            </li>
          ))}
        </DropDownMenu>
        </div>
      </div>
      <div class="table-tools">
        {button('find', false)}
        {group('Rows', ROWS)}
        {group('Columns', COLUMNS, MORE_COLUMNS)}
      </div>
      <p class="table-info" role="status">
        {!rows ? 'No data yet: integrate first.' : <>
          {`${rows} rows`}{sliced && ` · rows ${page.start}..${Math.max(page.start, page.end - 1)} kept`}
          {` · row ${selected}`}{selectedT !== undefined && selectedT !== null && ` (t = ${selectedT})`}{' selected'}
          {' '}<button class="small" data-button="initial_conditions.set_from_data_row" disabled={!may({cmd: 'initial_conditions.set_from_data_row', row: 0})}
            title={commandRow(hello, 'initial_conditions.set_from_data_row')?.description} onClick={() => session.getRow()}>Use row {selected} as initial conditions</button>
        </>}
      </p>
      <div class="table-grid-wrap" ref={scroller} role="grid" aria-label="Stored data" style={{'--table-cols': cols.length}} aria-rowcount={rows + 1}
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
              {cols.map((_, i) => <span class="table-cell" role="gridcell" key={i}><Cell v={values?.[i]} /></span>)}
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
