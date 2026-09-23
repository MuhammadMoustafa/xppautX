/* The AUTO view (docs/ui-v2.md T11a): the bifurcation diagram drawn from
   the `diagram` data (store/diagram.ts, plot/diagramModel.ts), with the AUTO
   window's buttons. It opens when the core opens window 101 (File/Auto) and
   goes when the core destroys it.

   Layout (R6): a floating panel anchored to the right from 48rem, over the
   plot, and a full-screen sheet under that; both stop above the status bar,
   whose Stop is the one way to stop a run (A10: the view has no Abort of
   its own). Back (or Escape) hides the panel and leaves AUTO open, "Show
   AUTO" brings it back; Close is "done with it": it closes AUTO's window,
   stopping a running continuation first (session.closeAuto).

   The diagram: zoom, pan, reset and undo as on the plot (plot/interactions.ts,
   plot/plotKeys.ts), all in the client; the point under the mouse, a tap,
   or the keyboard's stepping ([ ] PageUp PageDown Home End along a curve,
   { } between curves, < > from label to label) is named in the readout:
   branch, point, kind, label and values. AUTO's own hotkeys (A, N, G, R, D,
   C, U, P, F, as on its X11 window) work while the focus is in the view.

   T11b: the core's asks on the diagram are answered here. Grab is a mode of
   the diagram: the arrow keys, [ ], Page Up and Down, Home and End move its
   cursor from point to point, Tab and Shift+Tab from label to label, Enter
   takes the point, Escape cancels, and a click or a tap takes the nearest
   point (each step is a `grab` answer with the point's index, so the core's
   cursor, its info strip and circle follow). Axes/Zoom's box and
   Axes/Scroll's drag are the plot's modes (plot/pick.ts) on the diagram. A
   click on a two-parameter diagram stores the point (`auto point`), marked
   on it. The info strip and the stability circle are ui/AutoInfo.tsx. */
import {useEffect, useMemo, useRef} from 'preact/hooks';
import {DiagramChart, paletteColor, setDiagramChart} from '../plot/diagramChart';
import {buildDiagramModel, describePoint, fmt, grabStep, stepLabel, vertexOf, type DiagramModel} from '../plot/diagramModel';
import {download} from '../plot/export';
import {attachGestures, type PickSink} from '../plot/interactions';
import {pickKey, toData} from '../plot/pick';
import {plotKey} from '../plot/plotKeys';
import type {Ranges} from '../plot/viewmath';
import type {AutoOp, Session} from '../session';
import {pointCount, type DiagramHover} from '../store/diagram';
import {AutoInfo} from './AutoInfo';
import {useSession, useStore} from './context';
import {PickBar, PickOverlay, pickSink} from './PlotView';
import './auto.css';

/** label, op, key (auto_x11.c auto_keypress), in the X11 window's order */
const BUTTONS: [string, AutoOp, string][] = [
  ['Parameter', 'param', 'p'], ['Axes', 'axes', 'a'], ['Numerics', 'numerics', 'n'], ['Run', 'run', 'r'],
  ['Grab', 'grab', 'g'], ['Usr period', 'usr', 'u'], ['Clear', 'clear', 'c'], ['reDraw', 'redraw', 'd'],
  ['File', 'file', 'f'],
];
const OP_OF_KEY: Record<string, AutoOp> = Object.fromEntries(BUTTONS.map(([, op, k]) => [k, op]));

const KEYS_HELP = 'Arrow keys pan, plus and minus zoom, 0 resets, Control Z undoes a zoom, square brackets and Page Up '
  + 'or Down step through the points of a branch, braces change the branch, less than and greater than go from '
  + 'label to label, Escape clears the readout. The letters of the buttons run them.';

function setHover(session: Session, hover: DiagramHover | null): void {
  session.store.dispatch({type: 'diagram', action: {type: 'hover', hover}});
}

const WIN = 101;
const GRAB_PX = 48; /* how far from a point a click or a tap still takes it */

/** the plot mode of an ask on the diagram, while it waits for the user */
function activePick(session: Session) {
  const p = session.store.getState().pick;
  return p && !p.waiting && p.win === WIN ? p : null;
}

/** the diagram's pointer events: the plot modes' (Axes/Zoom, Axes/Scroll),
    and a grab's, where a release takes the nearest point */
function autoSink(session: Session, chart: () => DiagramChart | null, model: () => DiagramModel): PickSink {
  const picks = pickSink(session, WIN, chart);
  const grabbing = () => session.store.getState().diagram.grabbing;
  return {
    mode: () => picks.mode() ?? (grabbing() ? 'point' : null),
    press(at) { if (picks.mode()) picks.press(at); },
    drag(at) { if (picks.mode()) picks.drag(at); },
    release(at) {
      if (picks.mode()) {
        picks.release(at);
        return;
      }
      const c = chart(), a = c?.areaBox();
      if (!c || !a || !grabbing()) return;
      const hit = c.hit(at.fx * a.width, at.fy * a.height, GRAB_PX);
      const point = hit ? model().curves[hit.curve]?.idx[hit.index] : undefined;
      if (point !== undefined) session.grabPoint(point, true);
    },
    hover(at) { if (picks.mode()) picks.hover(at); },
  };
}

const GRAB_HELP = 'Arrow keys or square brackets step to the next or previous point, Page Up and Down ten points, '
  + 'Home and End the first and the last, Tab and Shift+Tab the next and previous labelled point. Enter takes it, '
  + 'Escape cancels.';

/** what a grab wants, with Take and Cancel; Escape anywhere cancels */
function GrabBar() {
  const session = useSession();
  const waiting = useStore(s => s.ask?.kind !== 'grab');
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
    <div class="pick-bar" data-pick="grab">
      <span id="grab-instruction" role="status">
        <b>Grab a point. </b>
        {touch ? 'Tap a point to take it.' : 'Click a point to take it, or step with the arrow keys and Tab, then press Enter.'}
      </span>
      <button disabled={waiting} onClick={() => session.grabTake()}>Take</button>
      <button disabled={waiting} onClick={() => session.cancelPick()}>Cancel</button>
    </div>
  );
}

/** the kinds of curve the diagram has, for its key (colour is not the only carrier, A7) */
function legendOf(m: DiagramModel): {text: string; color: number; dashed: boolean}[] {
  const seen = new Map<string, {text: string; color: number; dashed: boolean}>();
  for (const c of m.curves) {
    const text = c.kind === 'two-parameter' ? 'Two-parameter curve'
      : `${c.stable ? 'Stable' : 'Unstable'} ${c.kind === 'periodic' ? 'periodic orbits (max, min)' : 'steady states'}`;
    const key = `${text}/${c.color}`;
    if (!seen.has(key)) seen.set(key, {text, color: c.color, dashed: c.dashed});
  }
  return [...seen.values()];
}

function AutoPanel({dark}: {dark: boolean}) {
  const session = useSession();
  const points = useStore(s => s.diagram.points);
  const labels = useStore(s => s.diagram.labels);
  const axes = useStore(s => s.diagram.axes);
  const viewport = useStore(s => s.diagram.viewport);
  const canUndo = useStore(s => s.diagram.viewportHistory.length > 0);
  const hover = useStore(s => s.diagram.hover);
  const busy = useStore(s => s.busy);
  const hints = useStore(s => (s.hello as {auto_hints?: string[]} | null)?.auto_hints);
  const grabbing = useStore(s => s.diagram.grabbing);
  const info = useStore(s => s.diagram.info);
  const stored = useStore(s => (s.diagram.axes?.plot === 4 ? s.diagram.stored : null));
  const pick = useStore(s => (s.pick?.win === WIN && !s.pick.waiting ? s.pick : null));
  const panel = useRef<HTMLElement>(null);
  const host = useRef<HTMLDivElement>(null);
  const chart = useRef<DiagramChart | null>(null);
  /** the curve the readout's point was last found on (a point can be on two) */
  const hint = useRef(-1);

  const model = useMemo(() => buildDiagramModel(points, labels, axes), [points, labels, axes]);
  const modelRef = useRef(model);
  modelRef.current = model;
  const core: Ranges | null = useMemo(() => (axes && axes.xmax > axes.xmin && axes.ymax > axes.ymin
    ? {x: {min: axes.xmin, max: axes.xmax}, y: {min: axes.ymin, max: axes.ymax}} : null), [axes]);

  const hoverVertex = (curve: number, index: number) => {
    const c = modelRef.current.curves[curve];
    if (!c || index < 0 || index >= c.idx.length) return;
    const point = c.idx[index], p = session.store.getState().diagram.points;
    hint.current = curve;
    setHover(session, {point, low: c.which === 'y2' && p.y2[point] !== p.y[point]});
  };

  useEffect(() => {
    const c = new DiagramChart(host.current!, {
      onViewport: (viewport, push) => session.store.dispatch({type: 'diagram', action: {type: 'viewport', viewport, push}}),
    });
    let detach = () => {};
    c.onArea = area => {
      detach();
      const off = attachGestures(c, area, {hover: hoverVertex, leave: () => setHover(session, null)},
        autoSink(session, () => chart.current, () => modelRef.current));
      /* a click (not a box) on a two-parameter diagram stores the point for AUTO's File/sElect 2par pt */
      let down: {x: number; y: number} | null = null;
      const onDown = (e: MouseEvent) => { down = e.button === 0 && !e.shiftKey ? {x: e.clientX, y: e.clientY} : null; };
      const onClick = (e: MouseEvent) => {
        const st = session.store.getState(), at = down;
        down = null;
        if (!at || Math.hypot(e.clientX - at.x, e.clientY - at.y) > 4) return;
        if (st.diagram.axes?.plot !== 4 || st.diagram.grabbing || st.pick || st.ask || st.busy) return;
        const r = area.getBoundingClientRect();
        const d = toData(c.ranges(), {fx: (e.clientX - r.left) / r.width, fy: (e.clientY - r.top) / r.height});
        session.autoPoint(d.x, d.y);
      };
      area.addEventListener('mousedown', onDown, true);
      area.addEventListener('click', onClick);
      detach = () => {
        off();
        area.removeEventListener('mousedown', onDown, true);
        area.removeEventListener('click', onClick);
      };
    };
    chart.current = c;
    setDiagramChart(c);
    const ro = new ResizeObserver(() => {
      if (host.current?.clientWidth) c.resize();
    });
    ro.observe(host.current!);
    return () => {
      ro.disconnect();
      detach();
      setDiagramChart(null);
      c.destroy();
    };
  }, [session]);

  useEffect(() => {
    chart.current!.set(model, core, session.store.getState().diagram.viewport, dark);
  }, [model, core, dark]);

  useEffect(() => {
    chart.current!.applyViewport(viewport);
  }, [viewport]);

  /* the status bar stays visible below the panel: its Stop is how a run stops (A10) */
  useEffect(() => {
    const bar = document.querySelector<HTMLElement>('.status-bar'), el = panel.current;
    if (!bar || !el) return;
    const ro = new ResizeObserver(() => el.style.setProperty('--auto-bottom', `${bar.offsetHeight}px`));
    ro.observe(bar);
    return () => ro.disconnect();
  }, []);

  /* the focus moves in when the panel appears, so AUTO's keys work at once */
  useEffect(() => {
    if (!session.store.getState().ask) host.current?.focus({preventScroll: true});
  }, []);

  /* a grab or a plot mode takes the focus, so its keys work at once (A3) */
  useEffect(() => {
    if (grabbing || pick) host.current?.focus({preventScroll: true});
  }, [grabbing, pick?.ask]);

  /* the readout follows the grab's cursor */
  useEffect(() => {
    if (grabbing && info && info.point >= 0 && info.point < pointCount(points)) {
      hint.current = -1;
      setHover(session, {point: info.point, low: false});
    }
  }, [grabbing, info]);

  /* a grab's keys: each step answers the grab with the point's index */
  const grabKey = (e: KeyboardEvent): boolean => {
    const st = session.store.getState();
    if (!st.diagram.grabbing || e.ctrlKey || e.metaKey || e.altKey) return false;
    const ask = st.ask?.kind === 'grab' ? st.ask : null;
    if (e.key === 'Enter' || e.key === 'Escape') {
      if (ask) {
        if (e.key === 'Enter') session.grabTake();
        else session.cancelPick();
      }
      return true;
    }
    const to = grabStep(e.key, e.shiftKey, st.diagram.info?.point ?? -1, pointCount(st.diagram.points), st.diagram.labels);
    if (to === null) return false;
    if (ask) session.grabPoint(to);
    return true; /* a step while the last one is answered is dropped: the cursor is the core's */
  };

  /* a plot mode's keys (Axes/Zoom's box, Axes/Scroll's drag), as on the plot */
  const pickKeyDown = (e: KeyboardEvent): boolean => {
    const c = chart.current, p = activePick(session);
    if (!c || !p || e.ctrlKey || e.metaKey || e.altKey) return false;
    const r = pickKey(p, e.key, e.shiftKey);
    if (!r) return false;
    if ('pick' in r) session.movePick(r.pick);
    else if ('confirm' in r) session.confirmPick(r.confirm, c.ranges());
    else if ('drag' in r) {
      const ranges = c.ranges();
      for (const st of r.drag) {
        const d = toData(ranges, st.at);
        session.dragEvent(st.what, d.x, d.y);
      }
    } else session.cancelPick();
    return true;
  };

  const onHostKey = (e: KeyboardEvent) => {
    const c = chart.current, m = modelRef.current;
    if (grabKey(e) || pickKeyDown(e)) {
      e.preventDefault();
      e.stopPropagation();
      return;
    }
    if (!c || e.altKey) return;
    const h = session.store.getState().diagram.hover;
    const at = h ? vertexOf(m, h.point, h.low, hint.current) : null;
    if ((e.key === '<' || e.key === '>') && !e.ctrlKey && !e.metaKey) {
      const next = stepLabel(labels, h ? h.point : -1, e.key === '>' ? 1 : -1);
      if (next === null) return;
      e.preventDefault();
      e.stopPropagation();
      hint.current = -1;
      setHover(session, {point: next, low: false});
      return;
    }
    const key = (e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z' ? 'Undo' : e.ctrlKey || e.metaKey ? '' : e.key;
    const r = plotKey(key, {ranges: c.ranges(), hover: at, counts: m.curves.map(cd => cd.xs.length)});
    if (!r) return;
    e.preventDefault();
    e.stopPropagation(); /* not an XPP hotkey */
    if ('view' in r) c.setView(r.view, true);
    else if ('reset' in r) c.reset();
    else if ('undo' in r) session.store.dispatch({type: 'diagram', action: {type: 'undoViewport'}});
    else if (r.hover) hoverVertex(r.hover.curve, r.hover.index);
    else setHover(session, null);
  };

  /* keys anywhere in the panel: AUTO's hotkeys (not from a control), Escape hides it */
  const onPanelKey = (e: KeyboardEvent) => {
    if (e.defaultPrevented || e.ctrlKey || e.metaKey || e.altKey) return;
    const {ask, busy: running} = session.store.getState();
    if (ask) return;
    const t = e.target as HTMLElement;
    if (e.key === 'Escape' && !running) {
      e.preventDefault();
      e.stopPropagation();
      session.showAuto(false);
      return;
    }
    const op = OP_OF_KEY[e.key.toLowerCase()];
    if (op && !running && !t.closest('button, input, select, textarea')) {
      e.preventDefault();
      e.stopPropagation();
      session.autoOp(op);
    }
  };

  const at = hover ? vertexOf(model, hover.point, hover.low, hint.current) : null;
  const marker = at && chart.current ? chart.current.position(at.curve, at.index) : null;
  const cursor = grabbing && info && info.x !== null && info.y !== null && chart.current
    ? chart.current.place(info.x, info.y) : null;
  const storedAt = stored && chart.current ? chart.current.place(stored.x, stored.y) : null;
  const zoomed = viewport.x !== null || viewport.y !== null;
  const empty = !model.curves.length;
  const said = hover && hover.point < points.x.length ? describePoint(points, labels, axes, hover.point) : null;
  const what = axes ? `${axes.ylabel} against ${axes.xlabel}` : '';
  const label = empty ? 'AUTO diagram, no branches yet'
    : `AUTO diagram of ${what}: ${new Set(model.curves.map(c => c.branch)).size} branches, ${points.x.length} points, `
      + `${labels.length} labelled points`;

  return (
    <section id="auto-panel" ref={panel} class="auto-panel" aria-label="AUTO" onKeyDown={onPanelKey}>
      <div class="auto-header">
        <button class="auto-back" onClick={() => session.showAuto(false)}
          title="Hide the AUTO view (AUTO stays open; Show AUTO brings it back)">Back</button>
        <h2>AUTO <span class="muted auto-what">{what}</span></h2>
        <button class="auto-close" onClick={() => session.closeAuto()}
          title="Done with AUTO: close its window (a running continuation is stopped first; File/Auto opens it again)">
          Close
        </button>
      </div>
      <div class="auto-tools" role="toolbar" aria-label="AUTO">
        {BUTTONS.map(([text, op, k], i) => (
          <button key={op} disabled={busy} title={hints?.[i] ?? text} aria-keyshortcuts={k.toUpperCase()}
            onClick={() => session.autoOp(op)}>{text}</button>
        ))}
      </div>
      <div class="auto-view">
        {grabbing && <GrabBar />}
        {pick && <PickBar pick={pick} />}
        <header class="plot-bar">
          <ul class="auto-legend" aria-label="Key">
            {legendOf(model).map(l => (
              <li key={`${l.text}/${l.color}`}>
                <span class={'auto-swatch' + (l.dashed ? ' dashed' : '')} aria-hidden="true"
                  style={{borderColor: paletteColor(l.color, dark)}} />
                {l.text}
              </li>
            ))}
          </ul>
          <div class="plot-tools">
            <button disabled={!canUndo} onClick={() => session.store.dispatch({type: 'diagram', action: {type: 'undoViewport'}})}
              title="Undo the last zoom or pan (Ctrl+Z on the diagram)">Undo zoom</button>
            <button disabled={!zoomed} onClick={() => chart.current!.reset()}
              title="Back to AUTO's axes (double click, or 0 on the diagram)">Reset view</button>
            <button disabled={empty} onClick={() => { const u = chart.current!.png(); if (u) download('xpp-auto.png', u); }}
              title="Save the diagram as a PNG picture">PNG</button>
          </div>
        </header>
        <div class={'plot-host auto-host' + (grabbing ? ' picking pick-grab' : pick ? ` picking pick-${pick.mode}` : '')}
          ref={host} tabIndex={0} role="application" aria-roledescription="diagram"
          aria-label={label} onKeyDown={onHostKey} aria-describedby={grabbing ? 'grab-instruction grab-keys-help'
            : pick ? 'pick-instruction auto-keys-help' : 'auto-keys-help'}>
          {marker && <span class="hover-dot" style={{left: `${marker.left}px`, top: `${marker.top}px`}} />}
          {cursor && <span class="auto-cursor" aria-hidden="true" style={{left: `${cursor.left}px`, top: `${cursor.top}px`}} />}
          {storedAt && (
            <span class="auto-stored" style={{left: `${storedAt.left}px`, top: `${storedAt.top}px`}}
              title={`Stored point: ${fmt(stored!.x)}, ${fmt(stored!.y)}`} />
          )}
          {pick && chart.current && <PickOverlay pick={pick} chart={chart.current} />}
          {empty && (
            <div class="plot-empty">
              <p>{busy ? 'AUTO is running…' : 'No branches yet: Run starts a continuation from the current point.'}</p>
            </div>
          )}
        </div>
        <footer class="readout auto-readout" role="status" aria-live="polite">
          {said ? (
            <span>
              <b>{said.head}</b> · {said.kind}
              {said.label && <> · <b class="auto-label">{said.label}</b></>}
              {said.values.map(v => <span key={v}> · {v}</span>)}
            </span>
          ) : (
            <span class="muted">
              <span class="hint-mouse">Drag to zoom · wheel zooms · Shift+drag pans · double click resets · &lt; &gt; step through the labels</span>
              <span class="hint-touch">Pinch zooms · drag pans · tap a point to read it</span>
            </span>
          )}
        </footer>
      </div>
      <AutoInfo />
      {stored && (
        <p class="auto-stored-text muted">Stored point for File/sElect 2par pt: {fmt(stored.x)}, {fmt(stored.y)}</p>
      )}
      <p id="auto-keys-help" class="visually-hidden">{KEYS_HELP}</p>
      <p id="grab-keys-help" class="visually-hidden">{GRAB_HELP}</p>
    </section>
  );
}

export function AutoView({dark}: {dark: boolean}) {
  const session = useSession();
  const open = useStore(s => s.diagram.open);
  const shown = useStore(s => s.diagram.shown);
  const showButton = useRef<HTMLButtonElement>(null);
  const wasShown = useRef(false);

  /* the focus follows the panel: to Show AUTO when it is hidden, to the plot when AUTO closes */
  useEffect(() => {
    const inPanel = !!document.activeElement?.closest?.('.auto-panel') || document.activeElement === document.body;
    if (wasShown.current && !shown && inPanel) {
      if (open) showButton.current?.focus();
      else document.querySelector<HTMLElement>('.plot-view:not([hidden]) .plot-host')?.focus();
    }
    wasShown.current = open && shown;
  }, [open, shown]);

  if (!open) return null;
  if (shown) return <AutoPanel dark={dark} />;
  return (
    <button ref={showButton} class="auto-show" aria-controls="auto-panel" aria-expanded="false"
      onClick={() => session.showAuto(true)} title="Show the AUTO view again">Show AUTO</button>
  );
}
