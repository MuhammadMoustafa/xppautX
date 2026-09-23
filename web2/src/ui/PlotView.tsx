/* The main plot: the active window's curves from the series event, drawn by
   plot/chart.ts. Zoom, pan and the point readout go through the store
   (viewport, hover), so they are state a test can read, and work by mouse,
   touch and keyboard alike (plot/interactions.ts, plot/plotKeys.ts). */
import {useEffect, useMemo, useRef, useState} from 'preact/hooks';
import {Chart} from '../plot/chart';
import {curveColor} from '../plot/colors';
import {download, downloadCsv} from '../plot/export';
import {attachGestures} from '../plot/interactions';
import {buildModel, type PlotModel} from '../plot/model';
import {PLOT_KEYS_HELP, plotKey} from '../plot/plotKeys';
import {setCurrentChart} from '../plot/registry';
import type {Ranges} from '../plot/viewmath';
import type {View} from '../protocol/types';
import type {Session} from '../session';
import {useSession, useStore} from './context';

/** the core's window (Viewaxes, Window/Zoom) when it is the plot's window */
function coreView(view: View | undefined, win: number | undefined): Ranges | null {
  if (!view || view.win !== win || view.three) return null;
  if (!(view.xlo < view.xhi && view.ylo < view.yhi)) return null;
  return {x: {min: view.xlo, max: view.xhi}, y: {min: view.ylo, max: view.yhi}};
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

export function PlotView({dark}: {dark: boolean}) {
  const session = useSession();
  const series = useStore(s => s.series);
  const view = useStore(s => s.core?.view);
  const viewport = useStore(s => s.viewport);
  const canUndo = useStore(s => s.viewportHistory.length > 0);
  const hover = useStore(s => s.hover);
  const busy = useStore(s => s.busy);
  const host = useRef<HTMLDivElement>(null);
  const chart = useRef<Chart | null>(null);
  const [, setShown] = useState(0); /* the legend's toggles live in the chart */
  const model: PlotModel | null = useMemo(() => (series ? buildModel(series) : null), [series]);
  const modelRef = useRef(model);
  modelRef.current = model;

  useEffect(() => {
    const c = new Chart(host.current!, {
      onViewport: (v, push) => session.store.dispatch({type: 'viewport', viewport: v, push}),
    });
    let detach = () => {};
    c.onArea = area => {
      detach();
      detach = attachGestures(c, area, {
        hover: (curve, index) => setHover(session, modelRef.current, curve, index),
        leave: () => clearHover(session),
      });
    };
    chart.current = c;
    setCurrentChart(c);
    const ro = new ResizeObserver(() => c.resize());
    ro.observe(host.current!);
    return () => {
      ro.disconnect();
      detach();
      setCurrentChart(null);
      c.destroy();
    };
  }, [session]);

  useEffect(() => {
    if (model) chart.current!.set(model, coreView(view, series?.win), session.store.getState().viewport, dark);
  }, [model, view, dark]);

  useEffect(() => {
    chart.current!.applyViewport(viewport);
  }, [viewport]);

  const onKeyDown = (e: KeyboardEvent) => {
    const c = chart.current, m = modelRef.current;
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
    else if ('undo' in r) session.store.dispatch({type: 'undoViewport'});
    else if (r.hover) setHover(session, m, r.hover.curve, r.hover.index);
    else clearHover(session);
  };

  const marker = hover && model && chart.current
    ? chart.current.position(hover.curve, hover.row - (model.curves[hover.curve]?.row0 ?? 0)) : null;
  const zoomed = viewport.x !== null || viewport.y !== null;
  const empty = !model || model.curves.every(c => c.xs.length === 0);
  const label = model?.curves.length
    ? `Plot of ${model.curves.map(c => c.label).join(', ')}, ${model.curves[0].xs.length} points`
    : 'Plot, no data yet';

  return (
    <section class="plot-view" aria-label="Plot">
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
        </div>
        <div class="plot-tools">
          <button disabled={!canUndo} onClick={() => session.store.dispatch({type: 'undoViewport'})}
            title="Undo the last zoom or pan (Ctrl+Z on the plot)">Undo zoom</button>
          <button disabled={!zoomed} onClick={() => chart.current!.reset()}
            title="Back to the window's axes (double click, or 0 on the plot)">Reset view</button>
          <button disabled={empty} onClick={() => { const u = chart.current!.png(); if (u) download('xpp-plot.png', u); }}
            title="Save the plot as a PNG picture">PNG</button>
          <button disabled={empty} onClick={() => model && downloadCsv('xpp-curves.csv', model)}
            title="Save the plotted numbers as CSV">CSV</button>
        </div>
      </header>
      <div
        class="plot-host"
        ref={host}
        tabIndex={0}
        role="application"
        aria-roledescription="plot"
        aria-label={label}
        aria-describedby="plot-keys-help"
        onKeyDown={onKeyDown}>
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
      <p id="plot-keys-help" class="visually-hidden">{PLOT_KEYS_HELP}</p>
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
