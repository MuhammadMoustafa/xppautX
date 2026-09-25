/* The array plot (docs/ui-v2.md T12, docs/protocol.md `aplot`): XPP's grid
   of cells over a range of stored columns and rows, each cell coloured by
   its value between the core's zmin and zmax. The core also sends `cells`
   (its own colour indices, FIRSTCOLOR..+ncolors, what its GIF writer paints); this
   view draws from `values` instead, so it can offer its own colour map
   (session.setAplotColorMap) without asking the core again -- store/aplot.ts
   is the truth for both, `plot/aplotColors.ts` the pure colour maths. A
   floating panel like the data table (R6): a section under the plot from
   48rem, a full-screen sheet under that, opened from the title bar's Array
   button and closed by Back or Escape, focus moving in and back like
   ui/TableView.tsx. The core's own buttons (docs/protocol.md `aplot` op)
   work unchanged: Edit pops a form (AskDialog already renders it), GIF a
   save dialog (FileDialog). Time scrolls (SCOPE) by dragging or wheeling
   the grid, or PageUp/PageDown/arrows from the keyboard -- all through the
   same `scroll` op the classic page's mouse drag uses
   (plot/aplotScroll.ts). Hover (mouse or a finger held) names the cell
   under the pointer: its variable (from the title, store/aplot.ts
   columnName), its row's approximate time and its value. */
import {useEffect, useRef} from 'preact/hooks';
import {useFocusBackOnClose} from './focusBack';
import {cellColor, legendStops} from '../plot/aplotColors';
import {dragScroll, wheelScroll} from '../plot/aplotScroll';
import type {AplotColorMap} from '../store/aplot';
import {columnName, timeAt, valueAt} from '../store/aplot';
import type {AplotOp} from '../session';
import {useSession, useStore} from './context';

const FOCUSABLE = 'button:not([disabled]), select, [tabindex]:not([tabindex="-1"])';

const BUTTONS: [string, AplotOp, string][] = [
  ['Redraw', 'redraw', 'Draw again from the data'],
  ['Edit', 'edit', 'Columns, rows, skips and the z range'],
  ['Fit', 'fit', 'Fit the z range to the data'],
  ['Range', 'range', 'Save a GIF for each run of Integrate/Range'],
  ['Print', 'print', 'Write a PostScript file'],
  ['GIF', 'gif', 'Save the picture as a GIF'],
];

/** A14: six significant digits in a readout */
function fmt6(v: number): string {
  if (!Number.isFinite(v)) return 'NaN';
  const s = v.toPrecision(6);
  return s.includes('e') ? s : s.replace(/(\.\d*?)0+$/, '$1').replace(/\.$/, '');
}

export function AplotView() {
  const session = useSession();
  const open = useStore(s => s.aplot.open);
  const windowOpen = useStore(s => s.aplot.windowOpen);
  const aplot = useStore(s => s.aplot);
  const ev = aplot.event;
  const colorMap = aplot.colorMap;
  const hover = aplot.hover;

  const panel = useRef<HTMLElement>(null);
  const canvas = useRef<HTMLCanvasElement>(null);
  const wrap = useRef<HTMLDivElement>(null);
  const dragFrom = useRef<number | null>(null);

  const close = () => session.closeAplot();

  /* focus in on open, back to the toggle on close; Escape closes it */
  useFocusBackOnClose(open, panel, '.aplot-toggle');
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

  /* the canvas tracks its box's actual pixel size (R1/R5: no fixed px layout) */
  useEffect(() => {
    const box = wrap.current, c = canvas.current;
    if (!box || !c) return;
    const ro = new ResizeObserver(() => {
      const w = Math.max(1, Math.floor(box.clientWidth)), h = Math.max(1, Math.floor(box.clientHeight));
      if (c.width !== w || c.height !== h) {
        c.width = w;
        c.height = h;
      }
    });
    ro.observe(box);
    return () => ro.disconnect();
  }, []);

  const nx = ev?.nx ?? 0, ny = ev?.ny ?? 0;

  /* paints the grid: one fillRect per cell, like the classic page's
     paintArrayPlot (web/xpp-client.js) -- small grids (typically well under
     a million cells), so no offscreen canvas or imagedata path is needed */
  useEffect(() => {
    const c = canvas.current;
    const ctx = c?.getContext('2d');
    if (!c || !ctx) return;
    ctx.clearRect(0, 0, c.width, c.height);
    if (!nx || !ny || !ev) return;
    const dx = c.width / nx, dy = c.height / ny;
    for (let row = 0; row < ny; row++) {
      for (let col = 0; col < nx; col++) {
        ctx.fillStyle = cellColor(colorMap, valueAt(aplot, row, col), ev.zmin, ev.zmax);
        ctx.fillRect(Math.floor(col * dx), Math.floor(row * dy), Math.ceil(dx) + 1, Math.ceil(dy) + 1);
      }
    }
  }, [aplot, colorMap, nx, ny]);

  const cellAt = (clientX: number, clientY: number): {row: number; col: number} | null => {
    const c = canvas.current;
    if (!c || !nx || !ny) return null;
    const r = c.getBoundingClientRect();
    if (r.width <= 0 || r.height <= 0) return null;
    const col = Math.floor(((clientX - r.left) / r.width) * nx);
    const row = Math.floor(((clientY - r.top) / r.height) * ny);
    if (col < 0 || col >= nx || row < 0 || row >= ny) return null;
    return {row, col};
  };

  const showHover = (clientX: number, clientY: number) => {
    const cell = cellAt(clientX, clientY);
    session.aplotHover(cell && ev ? {...cell, t: timeAt(ev, cell.row), value: valueAt(aplot, cell.row, cell.col)} : null);
  };

  const onWheel = (e: WheelEvent) => {
    e.preventDefault();
    session.aplotScroll(wheelScroll(e.deltaY));
  };

  const onPointerDown = (e: PointerEvent) => {
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
    dragFrom.current = e.clientY;
  };
  const onPointerMove = (e: PointerEvent) => {
    if (dragFrom.current !== null && e.buttons) {
      session.aplotScroll(dragScroll(dragFrom.current, e.clientY));
      dragFrom.current = e.clientY;
    }
    showHover(e.clientX, e.clientY);
  };
  const endDrag = () => { dragFrom.current = null; };
  const onPointerLeave = () => {
    endDrag();
    session.aplotHover(null);
  };

  const onKeyDown = (e: KeyboardEvent) => {
    if (!session.aplotKeyScroll(e.key)) return;
    e.preventDefault();
    e.stopPropagation();
  };

  const stops = legendStops(colorMap);
  const label = ev && hover ? columnName(ev, hover.col) : '';

  return (
    <section id="aplot-panel" ref={panel} class={'aplot-panel' + (open ? ' open' : '')} aria-label="Array plot">
      <div class="aplot-header">
        <button class="aplot-back" onClick={close}>Back</button>
        <h2>Array plot</h2>
        <label class="aplot-map">
          Colour map
          <select value={colorMap}
            onChange={e => session.setAplotColorMap((e.target as HTMLSelectElement).value as AplotColorMap)}>
            <option value="viridis">Viridis</option>
            <option value="xpp">XPP</option>
          </select>
        </label>
      </div>
      <div class="aplot-tools">
        {BUTTONS.map(([label2, op, hint]) => (
          <button key={op} title={hint} onClick={() => session.aplotOp(op)}>{label2}</button>
        ))}
      </div>
      <p class="aplot-info" role="status">
        {!windowOpen ? 'No array plot yet: use the Window/zoom menu (Axes, Array) to define one.'
          : !nx || !ny || !ev ? 'Nothing to show: use Edit, or integrate first.'
            : `${fmt6(ev.tlo)} < t < ${fmt6(ev.thi)}. ${nx} columns, ${ny} rows.`}
      </p>
      <div class="aplot-body">
        <div class="aplot-scale" aria-hidden="true">
          <span>{ev ? fmt6(ev.zmax) : ''}</span>
          <div class="aplot-bar" style={{backgroundImage: `linear-gradient(to top, ${stops.join(',')})`}} />
          <span>{ev ? fmt6(ev.zmin) : ''}</span>
        </div>
        <div class="aplot-grid-wrap" ref={wrap} tabIndex={0} role="img"
          aria-label={ev && nx && ny
            ? `Array plot, ${nx} columns by ${ny} rows, coloured from ${fmt6(ev.zmin)} to ${fmt6(ev.zmax)}`
            : 'Array plot, nothing to show'}
          onWheel={onWheel} onKeyDown={onKeyDown}
          onPointerDown={onPointerDown} onPointerMove={onPointerMove} onPointerUp={endDrag}
          onPointerCancel={endDrag} onPointerLeave={onPointerLeave}>
          <canvas ref={canvas} class="aplot-canvas" />
        </div>
      </div>
      <p class="aplot-hover" role="status">
        {hover ? `${label || `Column ${hover.col}`}, row ${hover.row}: t ${fmt6(hover.t)}, value ${fmt6(hover.value)}.` : ''}
      </p>
    </section>
  );
}
