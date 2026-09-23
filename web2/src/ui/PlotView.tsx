/* One plot window: its curves from its series event, drawn by
   plot/chart.ts. Zoom, pan and the point readout go through the store (the
   window's viewport, hover), so they are state a test can read, and work by
   mouse, touch and keyboard alike (plot/interactions.ts, plot/plotKeys.ts).
   A window whose tab is not shown keeps its chart but draws nothing until
   it is shown again (ui/Plots.tsx). The core's mouse, rubber and drag asks
   for this window are plot modes here (plot/pick.ts): an instruction bar
   with Cancel, a crosshair, a box or a line drawn over the plot, answered
   in data coordinates. Its nullclines, direction field and flows (T7) and
   its marks (T8: equilibria, text, arrows, markers, frozen curves) are
   drawn by the chart too, and listed in the legend after the curves. */
import {useEffect, useMemo, useRef, useState} from 'preact/hooks';
import {Chart} from '../plot/chart';
import {curveColor} from '../plot/colors';
import {download, downloadCsv} from '../plot/export';
import {attachGestures, type PickSink} from '../plot/interactions';
import {pickInstruction, pickKey, toData, type Frac, type PickState} from '../plot/pick';
import {buildModel, type PlotModel} from '../plot/model';
import {markLayers, type MarkLayer} from '../plot/marks';
import {phaseLayers, type Layer} from '../plot/phase';
import {plotKey} from '../plot/plotKeys';
import {setChart} from '../plot/registry';
import type {Ranges} from '../plot/viewmath';
import type {PlotWindowInfo, View} from '../protocol/types';
import type {Session} from '../session';
import {HOME, windowOf} from '../store/plots';
import {useSession, useStore} from './context';

type Axes = Pick<View, 'xlo' | 'xhi' | 'ylo' | 'yhi' | 'three'>;

/** the core's axes (Viewaxes, Window/Zoom) of window `win`: from `plots`, else
    from `state.view` when that is the window's */
function coreView(info: PlotWindowInfo | null, view: View | undefined, win: number): Ranges | null {
  const a: Axes | null = info ?? (view && view.win === win ? view : null);
  if (!a || a.three) return null;
  if (!(a.xlo < a.xhi && a.ylo < a.yhi)) return null;
  return {x: {min: a.xlo, max: a.xhi}, y: {min: a.ylo, max: a.yhi}};
}

function fmt(v: number): string {
  return Number.isFinite(v) ? Number(v.toPrecision(6)).toString() : String(v);
}

/** the store's hover for point `index` of curve `curve`, or none */
function setHover(session: Session, m: PlotModel | null, curve: number, index: number): void {
  const c = m?.curves[curve];
  if (!m || !c || index < 0 || index >= c.xs.length) return;
  const row = c.row0 + index, h = session.store.getState().hover;
  if (h && h.curve === curve && h.row === row) return;
  session.store.dispatch({type: 'hover', hover: {curve, row, x: c.xs[index], y: c.ys[index], t: m.t ? m.t[row] : null}});
}

function clearHover(session: Session): void {
  if (session.store.getState().hover) session.store.dispatch({type: 'hover', hover: null});
}

/** the plot mode waiting for the user on window `win`, if any */
function activePick(session: Session, win: number): PickState | null {
  const p = session.store.getState().pick;
  return p && !p.waiting && p.win === win ? p : null;
}

/** pointer events of a plot mode (plot/interactions.ts) as moves and answers
    (the AUTO view's too, for window 101) */
export function pickSink(session: Session, win: number, chart: () => Pick<Chart, 'ranges'> | null): PickSink {
  const drag = (what: 'down' | 'move' | 'up', at: Frac) => {
    const c = chart();
    if (!c) return;
    const d = toData(c.ranges(), at);
    session.dragEvent(what, d.x, d.y);
  };
  const confirm = (p: PickState) => {
    const c = chart();
    if (c) session.confirmPick(p, c.ranges());
  };
  return {
    mode: () => activePick(session, win)?.mode ?? null,
    press(at) {
      const p = activePick(session, win);
      if (!p) return;
      if (p.mode === 'drag') drag('down', at);
      session.movePick({...p, cursor: at, anchor: p.mode === 'box' || p.mode === 'line' ? at : null});
    },
    drag(at) {
      const p = activePick(session, win);
      if (!p) return;
      if (p.mode === 'drag') drag('move', at);
      else session.movePick({...p, cursor: at});
    },
    release(at) {
      const p = activePick(session, win);
      if (!p) return;
      if (p.mode === 'drag') {
        drag('up', at);
        return;
      }
      const q = {...p, cursor: at, anchor: p.mode === 'point' ? null : p.anchor ?? at};
      session.movePick(q);
      confirm(q);
    },
    hover(at) {
      const p = activePick(session, win);
      if (p && p.mode !== 'drag') session.movePick({...p, cursor: at});
    },
  };
}

/** the crosshair, and the box or line from its fixed corner, over the plotting area */
export function PickOverlay({pick, chart}: {pick: PickState; chart: Pick<Chart, 'areaBox'>}) {
  const a = chart.areaBox();
  if (!a || pick.mode === 'drag') return null;
  const x = pick.cursor.fx * a.width, y = pick.cursor.fy * a.height;
  const ax = pick.anchor ? pick.anchor.fx * a.width : x, ay = pick.anchor ? pick.anchor.fy * a.height : y;
  return (
    <svg class="pick-overlay" aria-hidden="true" width={a.width} height={a.height}
      style={{left: `${a.left}px`, top: `${a.top}px`}}>
      <line x1={x} y1={0} x2={x} y2={a.height} />
      <line x1={0} y1={y} x2={a.width} y2={y} />
      {pick.anchor && pick.mode === 'box' && (
        <rect class="pick-shape" x={Math.min(ax, x)} y={Math.min(ay, y)} width={Math.abs(x - ax)} height={Math.abs(y - ay)} />
      )}
      {pick.anchor && pick.mode === 'line' && <line class="pick-shape" x1={ax} y1={ay} x2={x} y2={y} />}
      {pick.anchor && <circle class="pick-corner" cx={ax} cy={ay} r={4} />}
    </svg>
  );
}

/** what the plot mode wants, and Cancel (Done for a drag); Escape anywhere cancels too */
export function PickBar({pick}: {pick: PickState}) {
  const session = useSession();
  const hint = useStore(s => s.box);
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== 'Escape' || e.defaultPrevented) return;
      e.preventDefault();
      e.stopPropagation();
      session.cancelPick();
    };
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, [session]);
  const touch = typeof matchMedia === 'function' && matchMedia('(pointer: coarse)').matches;
  return (
    <div class="pick-bar" data-pick={pick.mode}>
      <span id="pick-instruction" role="status">
        {hint && pick.mode !== 'drag' && <b>{hint.replace(/[\s.:]+$/, '')}. </b>}
        {pickInstruction(pick, touch)}
        <span class="visually-hidden">{pick.anchor ? ' First corner set.' : ''}</span>
      </span>
      <button onClick={() => session.cancelPick()}>{pick.mode === 'drag' ? 'Done' : 'Cancel'}</button>
    </div>
  );
}

interface Props {
  win: number;
  dark: boolean;
  /** this window's tab is the one shown */
  shown: boolean;
  /** shown as a tab panel (more than one window) */
  tabbed: boolean;
}

export function PlotView({win, dark, shown, tabbed}: Props) {
  const session = useSession();
  const pw = useStore(s => windowOf(s.plots, win));
  const series = pw?.series ?? null;
  const info = pw?.info ?? null;
  const nullclines = pw?.nullclines ?? null;
  const dfield = pw?.dfield ?? null;
  const marks = pw?.marks ?? null;
  const layers: (Layer | MarkLayer)[] = useMemo(() => [...phaseLayers(nullclines, dfield), ...markLayers(marks)],
    [nullclines, dfield, marks]);
  const viewport = pw?.viewport ?? HOME;
  const canUndo = !!pw?.viewportHistory.length;
  const view = useStore(s => s.core?.view);
  const hover = useStore(s => (shown ? s.hover : null));
  const busy = useStore(s => s.busy);
  const pick = useStore(s => (s.pick?.win === win ? s.pick : null));
  const picking = pick && !pick.waiting ? pick : null;
  const host = useRef<HTMLDivElement>(null);
  const chart = useRef<Chart | null>(null);
  const [, setShown] = useState(0); /* the legend's toggles live in the chart */
  const model: PlotModel | null = useMemo(() => (series ? buildModel(series) : null), [series]);
  const modelRef = useRef(model);
  modelRef.current = model;

  const axes = useMemo(() => coreView(info, view, win), [info, view, win]);

  useEffect(() => {
    const c = new Chart(host.current!, {
      onViewport: (v, push) => session.store.dispatch({type: 'viewport', viewport: v, push, win}),
    });
    let detach = () => {};
    c.onArea = area => {
      detach();
      detach = attachGestures(c, area, {
        hover: (curve, index) => setHover(session, modelRef.current, curve, index),
        leave: () => clearHover(session),
      }, pickSink(session, win, () => chart.current));
    };
    chart.current = c;
    setChart(win, c);
    const ro = new ResizeObserver(() => {
      if (host.current?.clientWidth) c.resize(); /* not while its tab is hidden */
    });
    ro.observe(host.current!);
    return () => {
      ro.disconnect();
      detach();
      setChart(win, null);
      c.destroy();
    };
  }, [session, win]);

  /* a hidden tab draws nothing: the chart catches up when it is shown */
  useEffect(() => {
    const w = windowOf(session.store.getState().plots, win);
    if (model && shown) chart.current!.set(model, axes, w?.viewport ?? HOME, dark);
  }, [model, axes, dark, shown]);

  useEffect(() => {
    if (shown) chart.current!.setPhase(nullclines, dfield);
  }, [nullclines, dfield, shown]);

  useEffect(() => {
    if (shown) chart.current!.setMarks(marks);
  }, [marks, shown]);

  useEffect(() => {
    if (shown) chart.current!.applyViewport(viewport);
  }, [viewport]);

  /* a plot mode takes the focus, so its keys work at once (A3) */
  useEffect(() => {
    if (picking) host.current?.focus({preventScroll: true});
  }, [picking?.ask]);

  const onKeyDown = (e: KeyboardEvent) => {
    const c = chart.current, m = modelRef.current, p = activePick(session, win);
    if (c && p && !e.ctrlKey && !e.metaKey && !e.altKey) {
      const r = pickKey(p, e.key, e.shiftKey);
      if (r) {
        e.preventDefault();
        e.stopPropagation();
        if ('pick' in r) session.movePick(r.pick);
        else if ('confirm' in r) session.confirmPick(r.confirm, c.ranges());
        else if ('drag' in r) {
          const ranges = c.ranges();
          for (const s of r.drag) {
            const d = toData(ranges, s.at);
            session.dragEvent(s.what, d.x, d.y);
          }
        } else session.cancelPick();
        return;
      }
    }
    if (!c || !m || !m.curves.length) return;
    const h = session.store.getState().hover, curve = h ? m.curves[h.curve] : null;
    const key = (e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z' ? 'Undo'
      : e.ctrlKey || e.metaKey || e.altKey ? '' : e.key;
    const r = plotKey(key, {
      ranges: c.ranges(),
      hover: h && curve ? {curve: h.curve, index: h.row - curve.row0} : null,
      counts: m.curves.map((cd, i) => (c.isVisible(i) ? cd.xs.length : 0)),
    });
    if (!r) return;
    e.preventDefault();
    e.stopPropagation(); /* not an XPP hotkey */
    if ('view' in r) c.setView(r.view, true);
    else if ('reset' in r) c.reset();
    else if ('undo' in r) session.store.dispatch({type: 'undoViewport', win});
    else if (r.hover) setHover(session, m, r.hover.curve, r.hover.index);
    else clearHover(session);
  };

  const marker = hover && model && chart.current
    ? chart.current.position(hover.curve, hover.row - (model.curves[hover.curve]?.row0 ?? 0)) : null;
  const zoomed = viewport.x !== null || viewport.y !== null;
  const noCurves = !model || model.curves.every(c => c.xs.length === 0);
  const empty = noCurves && !layers.length;
  const texts = marks?.text.length ? `; text: ${marks.text.map(t => t.plain).join('; ')}` : '';
  const withLayers = layers.length ? `; ${layers.map(l => l.label).join(', ')}${texts}` : '';
  const label = model?.curves.length
    ? `Plot of ${model.curves.map(c => c.label).join(', ')}, ${model.curves[0].xs.length} points${withLayers}`
    : `Plot, no data yet${withLayers}`;
  const panel = tabbed
    ? {role: 'tabpanel' as const, id: `plot-panel-${win}`, 'aria-labelledby': `plot-tab-${win}`}
    : {'aria-label': 'Plot'};

  return (
    <section class="plot-view" hidden={!shown} {...panel}>
      <header class="plot-bar">
        <div class="legend" role="group" aria-label="Curves">
          {model?.curves.map((c, i) => (
            <button
              key={i}
              class={'legend-item' + (chart.current?.isVisible(i) === false ? ' off' : '')}
              aria-pressed={chart.current?.isVisible(i) !== false}
              title="Show or hide this curve"
              onClick={() => {
                chart.current!.setVisible(i, !chart.current!.isVisible(i));
                setShown(n => n + 1);
              }}>
              <span class="swatch" style={{background: curveColor(c.color, dark)}} aria-hidden="true" />
              {c.label}
            </button>
          ))}
          {layers.map(l => (
            <button
              key={l.key}
              class={'legend-item layer' + (chart.current?.isLayerVisible(l.key) === false ? ' off' : '')}
              data-layer={l.key}
              aria-pressed={chart.current?.isLayerVisible(l.key) !== false}
              title={`Show or hide the ${l.label}`}
              onClick={() => {
                chart.current!.setLayerVisible(l.key, !chart.current!.isLayerVisible(l.key));
                setShown(n => n + 1);
              }}>
              <span class={`swatch swatch-${l.key.replace(/-\d+$/, '')}`} style={{background: curveColor(l.color, dark)}}
                aria-hidden="true" />
              {l.label}
            </button>
          ))}
        </div>
        <div class="plot-tools">
          <button disabled={!canUndo} onClick={() => session.store.dispatch({type: 'undoViewport', win})}
            title="Undo the last zoom or pan (Ctrl+Z on the plot)">Undo zoom</button>
          <button disabled={!zoomed} onClick={() => chart.current!.reset()}
            title="Back to the window's axes (double click, or 0 on the plot)">Reset view</button>
          <button disabled={!zoomed} onClick={() => session.useThisView(win, chart.current!.ranges())}
            title="Make this zoom the window's own axes (Window/Window), for PostScript/SVG export and Restore">
            Use this view
          </button>
          <button disabled={noCurves} onClick={() => session.fitView()}
            title="Fit the window's axes to the data (Window/Fit)">Fit</button>
          <button disabled={empty} onClick={() => { const u = chart.current!.png(); if (u) download('xpp-plot.png', u); }}
            title="Save the plot as a PNG picture">PNG</button>
          <button disabled={noCurves} onClick={() => model && downloadCsv('xpp-curves.csv', model)}
            title="Save the plotted numbers as CSV">CSV</button>
        </div>
      </header>
      {picking && <PickBar pick={picking} />}
      <div
        class={'plot-host' + (picking ? ` picking pick-${picking.mode}` : '')}
        ref={host}
        tabIndex={0}
        role="application"
        aria-roledescription="plot"
        aria-label={label}
        aria-describedby={picking ? 'pick-instruction plot-keys-help' : 'plot-keys-help'}
        onKeyDown={onKeyDown}>
        {picking && chart.current && <PickOverlay pick={picking} chart={chart.current} />}
        {marker && <span class="hover-dot" style={{left: `${marker.left}px`, top: `${marker.top}px`}} />}
        {empty && (
          <div class="plot-empty">
            <p>{busy ? 'Integrating…' : 'No trajectory yet.'}</p>
            {!busy && (
              <button class="primary" onClick={() => session.keys('i', 'g')}>Integrate (I, G)</button>
            )}
          </div>
        )}
      </div>
      <footer class="readout" role="status" aria-live="polite">
        {hover && model ? (
          <span>
            <b>{model.curves[hover.curve]?.label}</b>
            {' '}row {hover.row}
            {hover.t !== null && <> · T = {fmt(hover.t)}</>}
            {' '}· {model.curves[hover.curve]?.xName} = {fmt(hover.x)} · {model.curves[hover.curve]?.yName} = {fmt(hover.y)}
          </span>
        ) : (
          <span class="muted">
            <span class="hint-mouse">Drag to zoom · wheel zooms · Shift+drag pans · double click resets</span>
            <span class="hint-touch">Pinch zooms · drag pans · tap a point to read it</span>
          </span>
        )}
      </footer>
    </section>
  );
}
