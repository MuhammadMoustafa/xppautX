/* A 3D plot window (docs/ui-v2.md T14, GitHub issue #18): XPP's 3D is
   curves in a box, seen from angles theta and phi; this draws the
   projection on a plain canvas (plot/render3d.ts, no three.js) and turns
   it locally at once from a drag or the arrow keys (plot/project3d.ts),
   reporting where it settled to the core (session.rotate3d), throttled,
   so `state.view.theta/phi` agrees once it stops. Siblings PlotView.tsx
   (2D); ui/Plots.tsx renders this one instead when a window's `plots`
   says `three`. No zoom, pan or pick modes here: those are a 2D plot's. */
import {cssVar} from '../plot/css';
import {useEffect, useMemo, useRef, useState} from 'preact/hooks';
import {Chart3D} from '../plot/chart3d';
import {curveColor} from '../plot/colors';
import type {PlotSeries} from '../store/series';
import {buildModel3d} from '../plot/model3d';
import {KEY_STEP, KEY_STEP_FINE, rotateByDrag, rotateByKey} from '../plot/project3d';
import {setChart} from '../plot/registry';
import {windowOf} from '../store/plots';
import {useMayMain, useSession, useStore} from './context';
import {TraceDialog} from './TraceDialog';
import {nearest3d} from '../plot/render3d';
import {FitButton} from './PlotView';

interface Props {
  win: number;
  dark: boolean;
  /** this window's tab is the one shown */
  shown: boolean;
  /** shown as a tab panel (more than one window) */
  tabbed: boolean;
}

interface Drag {
  pointerId: number;
  x0: number;
  y0: number;
  theta0: number;
  phi0: number;
}

function fmt(v: number | undefined): string {
  return v === undefined ? '' : String(Math.round(v));
}

export function Plot3DView({win, dark, shown, tabbed}: Props) {
  const session = useSession();
  const pw = useStore(s => windowOf(s.plots, win));
  const info = pw?.info ?? null;
  const series = pw?.series ?? null;
  const view3d = pw?.view3d ?? null;
  const [appearance, setAppearance] = useState<Record<string, {label?: string; color?: string; visible?: boolean}>>({});
  const [hovered, setHovered] = useState<number | null>(null);
  const [editing, setEditing] = useState<number | null>(null);
  const busy = useStore(s => s.computing);
  const mayMain = useMayMain();

  const host = useRef<HTMLDivElement>(null);
  const canvas = useRef<HTMLCanvasElement>(null);
  const chart = useRef<Chart3D | null>(null);
  const drag = useRef<Drag | null>(null);

  useEffect(() => {
    if (!canvas.current) return undefined;
    const c = new Chart3D(canvas.current);
    chart.current = c;
    setChart(win, c);
    const ro = new ResizeObserver(() => {
      if (host.current?.clientWidth) c.resize(); /* not while its tab is hidden */
    });
    ro.observe(host.current!);
    return () => {
      ro.disconnect();
      setChart(win, null);
      c.destroy();
    };
  }, [win]);

  const runKeys = useRef(new WeakMap<PlotSeries, string>());
  const nextRun = useRef(0);
  const history = pw?.history;
  const runsShown = pw?.showRuns ?? true;
  const sources = useMemo(() => series ? [series, ...(history?.runs ?? [])] : [], [series, history?.runs]);
  const keys = useMemo(() => sources.map((r, i) => {
    if (i === 0) return 'current';
    let key = runKeys.current.get(r);
    if (!key) { key = `run-${++nextRun.current}`; runKeys.current.set(r, key); }
    return key;
  }), [sources]);
  const rawModels = useMemo(
    () => info?.three && view3d ? sources.map(r => buildModel3d(r, info.box, view3d.theta, view3d.phi, info.persp, info.zplane, info.zview)) : [],
    [sources, info, view3d],
  );
  const traceKeys = rawModels.flatMap((m, run) => m.curves.map((_, i) => `${keys[run]}:${i}`));
  const model = useMemo(() => rawModels.length ? ({box:rawModels[0].box, labels:rawModels[0].labels,
    curves:rawModels.flatMap((m, run) => m.curves.map((c, i) => {
      const key = `${keys[run]}:${i}`, a = appearance[key];
      return {...c, label:a?.label ?? `${c.label}${run ? ` · ${keys[run]}` : ''}`,
        cssColor:a?.color, visible:a?.visible !== false && (run === 0 ? !history?.erased : runsShown),
        highlighted:hovered === rawModels.slice(0, run).reduce((n, m) => n + m.curves.length, 0) + i};
    }))}) : null, [rawModels, keys, appearance, hovered, history?.erased, runsShown]);

  useEffect(() => {
    if (!shown || !chart.current || !view3d) return;
    const axis = cssVar('--fg-muted');
    chart.current.set(model, view3d.theta, view3d.phi, dark, axis);
  }, [model, view3d, dark, shown]);

  const rotate = (theta: number, phi: number) => session.rotate3d(win, theta, phi);

  const onPointerDown = (e: PointerEvent) => {
    if (!view3d || e.button !== 0) return;
    (e.currentTarget as Element).setPointerCapture?.(e.pointerId);
    drag.current = {pointerId: e.pointerId, x0: e.clientX, y0: e.clientY, theta0: view3d.theta, phi0: view3d.phi};
  };
  const onPointerMove = (e: PointerEvent) => {
    const d = drag.current;
    if (!d) {
      const r = canvas.current!.getBoundingClientRect();
      setHovered(model ? nearest3d(model, r.width, r.height, e.clientX - r.left, e.clientY - r.top) : null);
      return;
    }
    if (d.pointerId !== e.pointerId) return;
    const r = rotateByDrag(d.theta0, d.phi0, e.clientX - d.x0, e.clientY - d.y0);
    rotate(r.theta, r.phi);
  };
  const endDrag = (e: PointerEvent) => {
    if (drag.current?.pointerId === e.pointerId) drag.current = null;
  };

  const onKeyDown = (e: KeyboardEvent) => {
    if (!view3d || e.ctrlKey || e.metaKey || e.altKey) return;
    const r = rotateByKey(view3d.theta, view3d.phi, e.key, e.shiftKey);
    if (!r) return;
    e.preventDefault();
    e.stopPropagation(); /* not an XPP hotkey */
    rotate(r.theta, r.phi);
  };

  const noCurves = !model || model.curves.every(c => c.points.length === 0);
  const label = `3D plot${info ? ` of ${info.title}` : ''}${view3d ? `, theta ${fmt(view3d.theta)}, phi ${fmt(view3d.phi)}` : ''}`;
  const panel = tabbed
    ? {role: 'tabpanel' as const, id: `plot-panel-${win}`, 'aria-labelledby': `plot-tab-${win}`}
    : {'aria-label': 'Plot'};

  return (
    <section class="plot-view" hidden={!shown} {...panel}>
      <header class="plot-bar">
        <div class="legend" role="group" aria-label="Curves">
          {model?.curves.map((c, i) => (
            <button key={traceKeys[i]} data-trace-key={traceKeys[i]} class={'legend-item' + (c.visible === false ? ' off' : '')} aria-pressed={c.visible !== false}
              title="Click to show or hide; double click to edit name and colour"
              onMouseEnter={() => setHovered(i)} onMouseLeave={() => setHovered(null)}
              onClick={() => setAppearance(a => ({...a, [traceKeys[i]]: {...a[traceKeys[i]], visible: c.visible === false}}))}
              onDblClick={() => setEditing(i)}>
              <span class="swatch" style={{background: c.cssColor ?? curveColor(c.color, dark)}} aria-hidden="true" />
              {c.label}
            </button>
          ))}
        </div>
        <div class="plot-tools"><label title="Keep earlier trajectories on the next new run">
          <input type="checkbox" checked={info?.freeze !== 0} data-freeze-runs
            onChange={e => session.send({cmd:'display',win,freeze:e.currentTarget.checked ? 1 : 0})} /> Freeze</label>{['X', 'Y', 'Z'].map(axis => <button disabled={!mayMain('viewaxes')}
          onClick={() => session.editPlotAxes(win, true)} title="Edit axis variables and limits">{axis} axis</button>)}</div>
      </header>
      <div
        class="plot-host"
        ref={host}
        tabIndex={0}
        role="application"
        aria-roledescription="3D plot"
        aria-label={label}
        aria-describedby="plot-3d-keys-help"
        onKeyDown={onKeyDown}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={endDrag}
        onPointerLeave={() => { if (!drag.current) setHovered(null); }}
        onPointerCancel={endDrag}>
        <canvas ref={canvas} class="plot-canvas-3d" aria-hidden="true" />
        {!noCurves && (
          <FitButton onClick={() => session.fitView()} disabled={!mayMain('window')} title="Fit the window's axes to the data (Window/Fit)" />
        )}
        {hovered !== null && model && <span class="trace-tooltip" role="tooltip">{model.curves[hovered]?.label}</span>}
        {noCurves && (
          <div class="plot-empty">
            <p>{busy ? 'Integrating…' : 'No trajectory yet.'}</p>
            {!busy && <p>Choose a run action above the plot.</p>}
          </div>
        )}
      </div>
      {editing !== null && model && <TraceDialog label={model.curves[editing].label}
        color={model.curves[editing].cssColor ?? curveColor(model.curves[editing].color, dark)} close={() => setEditing(null)}
        apply={(label, color) => { setAppearance(a => ({...a, [traceKeys[editing]]: {...a[traceKeys[editing]], label, color}})); setEditing(null); }} />}
      <p id="plot-3d-keys-help" class="visually-hidden">
        {`Drag turns the view; the arrow keys turn it ${KEY_STEP} degrees at a time, Shift ${KEY_STEP_FINE}.`}
      </p>
      <footer class="readout" role="status" aria-live="polite">
        <span class="muted">
          {view3d ? <>theta {fmt(view3d.theta)} · phi {fmt(view3d.phi)} — </> : null}
          <span class="hint-mouse">Drag to turn</span>
          <span class="hint-touch">Drag to turn</span>
          {' · arrow keys turn it too'}
        </span>
      </footer>
    </section>
  );
}
