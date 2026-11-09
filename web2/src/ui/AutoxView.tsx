import {openPanel} from './hotkeys';
import {WindowName} from './WindowName';
/* The autoX view (docs/ui-v2.md T11a): the bifurcation diagram drawn from
   the `diagram` data (store/diagram.ts, plot/diagramModel.ts), with the autoX
   window's buttons. It opens when the core opens window 101 (File/autoX) and
   goes when the core destroys it.

   Layout (T26): a full-screen sheet at every width. It covers the main menu, the plot and the main status bar with a plain
   background, so its own status strip is then the only status line on
   screen and also carries what the main one says that matters there (T21:
   what autoX does and has the Stop, ui/AutoxStatus.tsx; the status bar's Stop
   is the same one, A10), and its Output panel shows autoX's table. Back (or
   Escape) hides the panel and leaves autoX open, showing the page again;
   "Show autoX" brings it back; Close is "done with it": it closes autoX's
   window, stopping a running continuation first (session.closeAutox). The
   core's asks/prompts/messages and any dialog autoX opens stay on top of
   this view (the dialog backdrop's z-index is above every panel's).

   T21: the diagram is always the current one (the core draws it again
   after Axes and File/Load, so there is no reDraw); Clear is the view's:
   the branches so far become "earlier branches", hidden until their key
   entry shows them. A click on an axis name opens its dialog
   (ui/AutoxAxes.tsx).

   T22: Numerics and Mark values are the page's own forms on the
   `autoxsettings` data (ui/AutoxSettings.tsx), and so is what the axis dialog
   plots: they work during a run too, their changes pending until it ends.
   Only Run, Grab, Axes (its menu: zoom, fit, scroll ...) and File wait.

   The diagram: zoom, pan and reset as on the plot (plot/interactions.ts,
   plot/plotKeys.ts), all in the client; the point under the mouse, a tap,
   or the keyboard's stepping ([ ] PageUp PageDown Home End along a curve,
   { } between curves, < > from label to label) is named in the readout:
   branch, point, kind, label and values. autoX's own hotkeys (A, N, G, R, D,
   C, U, F, as on its X11 window) work while the focus is in the view.

   W50: the one diagram has any number of views (store/diagram.ts), side by
   side and wrapping to a new row when narrow, each with its own axes, zoom,
   key and axis names, a title and a close button while there are two or
   more (the last one stays); a press in one makes it the active view (the
   core's: autoX's Axes menu, its zoom and a run go by it; the menu's New
   view adds one with its axes). Every view marks the point under the mouse
   and a grab's cursor: a point's index is the same in all of them, so a
   grab takes a point from any view.

   T11b: the core's asks on the diagram are answered here. Grab is a mode of
   the diagram: the arrow keys, [ ], Page Up and Down, Home and End move its
   cursor from point to point, Tab and Shift+Tab from label to label, Enter
   takes the point, Escape cancels, and a click or a tap takes the nearest
   point (each step is a `grab` answer with the point's index, so the core's
   cursor, its info strip and circle follow). Axes/Zoom's box and
   Axes/Scroll's drag are the plot's modes (plot/pick.ts) on the diagram. A
   click on a two-parameter diagram stores the point (`autox point`), marked
   on it. The info strip and the stability circle are ui/AutoxInfo.tsx. */
import {TraceDialog} from './TraceDialog';
import {ShownName, ShownNumber} from './CutText';
import {sixSig} from '../store/values';
import {useEffect, useLayoutEffect, useMemo, useRef, useState} from 'preact/hooks';
import {DiagramChart, setDiagramChart} from '../plot/diagramChart';
import {pointLabel, pointMark} from '../store/pointTypes';
import {cssColor} from '../plot/colors';
import {stopPoint} from '../plot/autoxStatus';
import {
  buildDiagramModel, carryAppearance, describePoint, grabStep, labelShape, LABEL_GLYPH, labelTypes, legendEntries, stepLabel,
  symbolHelp, vertexOf, type DiagramModel,
} from '../plot/diagramModel';
import {attachGestures, type PickSink} from '../plot/interactions';
import {pickKey, toData} from '../plot/pick';
import {plotKey} from '../plot/plotKeys';
import type {Ranges} from '../plot/viewmath';
import {HELP} from '../help/links';
import type {AutoxOp, Session} from '../session';
import {pendingFields} from '../store/autoxSettings';
import {activeView, autoxWindow, EMPTY_VIEW, pointCount, type DiagramHover, type DiagramView} from '../store/diagram';
import {branchesBefore} from '../store/diagram';
import type {AppState} from '../store/state';
import {AutoxAxisDialog, type AxisName} from './AutoxAxes';
type AutoxSettingsDialogKind = 'numerics' | 'marks';
import {AutoxInfo} from './AutoxInfo';
import {AutoxOutput, AutoxStatus} from './AutoxStatus';
import {BUSY_TITLE, useMayKey, useSession, useStore} from './context';
import {HelpButton} from './HelpButton';
import {toolbarRow} from './navigation';
import {SetsRow} from './SetsRow';
import {AxisTitle, FitButton, PickBar, PickOverlay, pickSink} from './PlotView';

/** the ops (the registry command, its label and description, are navigation.ts toolbarCommand's), in the X11 window's order; no
    reDraw (T21): the diagram is always the current one */
const BUTTONS: AutoxOp[] = ['axes', 'numerics', 'run', 'grab', 'usr', 'clear'];
/** the view's own words for a button, over the core's hint */
const TITLES: Partial<Record<AutoxOp, string>> = {
  axes: "The active view's axes: what they plot, zoom, Fit, and New view (another view of the diagram)",
  clear: 'Hide the branches computed so far: new runs draw alone (the key shows them again)',
  numerics: "autoX's numerical settings: mesh, steps, limits, tolerances",
  usr: "Label the points where a parameter or the period reaches a value (autoX's user points, UZ)",
};
/** the buttons that open the page's own forms on autoX's settings (T22), and the pending edits each shows */
const SETTINGS_DIALOG: Partial<Record<AutoxOp, AutoxSettingsDialogKind>> = {numerics: 'numerics', usr: 'marks'};
const PENDING_OF: Record<AutoxSettingsDialogKind, (field: string) => boolean> = {
  numerics: f => f.startsWith('numerics.'), marks: f => f === 'marks',
};

const KEYS_HELP = 'Arrow keys pan, plus and minus zoom, 0 resets, Control Z undoes a zoom, square brackets and Page Up '
  + 'or Down step through the points of a branch, braces change the branch, less than and greater than go from '
  + 'label to label, Escape clears the readout. The letters of the buttons run them. A click in a view makes it '
  + "the active one, which autoX's Axes menu changes.";

function setHover(session: Session, hover: DiagramHover | null): void {
  session.store.dispatch({type: 'diagram', action: {type: 'hover', hover}});
}

/** view k of the store's diagram (an empty one for a view that has just gone) */
const viewOf = (s: AppState, k: number): DiagramView => s.diagram.views[k] ?? EMPTY_VIEW;

const GRAB_PX = 48; /* how far from a point a click or a tap still takes it */

/** the plot mode of an ask on the diagram, while it waits for the user */
function activePick(session: Session) {
  const p = session.store.getState().pick;
  return p && !p.waiting && p.win === autoxWindow(session.store.getState()) ? p : null;
}

/** a view's pointer events: the plot modes' (Axes/Zoom, Axes/Scroll: the active view's), and a
    grab's, where a release takes the nearest point (in any view: a point's index is the same in all) */
function autoxSink(session: Session, active: () => boolean, chart: () => DiagramChart | null, model: () => DiagramModel): PickSink {
  const picks = pickSink(session, () => autoxWindow(session.store.getState()), chart);
  const grabbing = () => session.store.getState().diagram.grabbing;
  const mode = () => (active() ? picks.mode() : null);
  return {
    mode: () => mode() ?? (grabbing() ? 'point' : null),
    press(at) { if (mode()) picks.press(at); },
    drag(at) { if (mode()) picks.drag(at); },
    release(at) {
      if (mode()) {
        picks.release(at);
        return;
      }
      const c = chart(), a = c?.areaBox();
      if (!c || !a || !grabbing()) return;
      const hit = c.hit(at.fx * a.width, at.fy * a.height, GRAB_PX);
      const point = hit ? model().curves[hit.curve]?.idx[hit.index] : undefined;
      if (point !== undefined) session.grabPoint(point, true);
    },
    hover(at) { if (mode()) picks.hover(at); },
  };
}

const GRAB_HELP = 'Arrow keys or square brackets step to the next or previous point, Page Up and Down ten points, '
  + 'Home and End the first and the last, Tab and Shift+Tab the next and previous labelled point. Enter takes it, '
  + 'Escape cancels.';

/** what a grab wants, with Take and Cancel; Escape anywhere cancels */
function GrabBar() {
  const session = useSession();
  const waiting = useStore(s => s.ask?.kind !== 'grab');
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

/** the kinds of curve the diagram has, for its key (color is not the only carrier, A7) */
function legendOf(m: DiagramModel): {text: string; color: string; dashed: boolean}[] {
  const seen = new Map<string, {text: string; color: string; dashed: boolean}>();
  for (const c of m.curves) {
    const text = c.kind === 'two-parameter' ? 'Two-parameter curve'
      : `${c.stable ? 'Stable' : 'Unstable'} ${c.kind === 'periodic' ? 'periodic orbits (max, min)' : 'steady states'}`;
    const key = `${text}/${c.color}`;
    if (!seen.has(key)) seen.set(key, {text, color: c.color, dashed: c.dashed});
  }
  return [...seen.values()];
}

/** what a view plots, in words */
const whatOf = (v: DiagramView) => (v.axes ? `${v.axes.ylabel} against ${v.axes.xlabel}` : '');

/** one view of the diagram (W50): its title (its axes, and a close button while it is not the only
    one), its key and view tools, its chart and axis names. A click in it makes it the active view. */
function DiagramPane({view, dark}: {view: number; dark: boolean}) {
  const session = useSession();
  const points = useStore(s => viewOf(s, view).points);
  const labels = useStore(s => viewOf(s, view).labels);
  const axes = useStore(s => viewOf(s, view).axes);
  const viewport = useStore(s => viewOf(s, view).viewport);
  const active = useStore(s => s.diagram.active === view);
  const views = useStore(s => s.diagram.views.length);
  const hover = useStore(s => s.diagram.hover);
  const busy = useStore(s => s.computing);
  const grabbing = useStore(s => s.diagram.grabbing);
  const info = useStore(s => s.diagram.info);
  const stored = useStore(s => (viewOf(s, view).axes?.plot === 'two_parameter' ? s.diagram.stored : null));
  const pick = useStore(s => (s.diagram.active === view && s.pick?.win === autoxWindow(s) && !s.pick.waiting ? s.pick : null));
  const earlier = useStore(s => Math.min(s.diagram.earlier, pointCount(viewOf(s, view).points)));
  const showEarlier = useStore(s => s.diagram.showEarlier);
  const liveKeys = useStore(s => s.diagram.liveKeys);
  const joined = useStore(s => s.diagram.joined);
  const [appearance, setAppearance] = useState<Record<string, {label?: string; color?: string; visible?: boolean}>>({});
  /* the join (W631): a job's legend settings follow it to its branch, the dropped jobs' and the live entries' go */
  useEffect(() => {
    if (joined) setAppearance(a => carryAppearance(a, joined.jobs));
  }, [joined]);
  const [editing, setEditing] = useState<{key: string; label: string; color: string} | null>(null);
  const [trace, setTrace] = useState<number | null>(null);
  const [axisOpen, setAxisOpen] = useState<AxisName | null>(null);
  /* the chart's area moved (a resize, a new model): the axis names follow */
  const [, setArea] = useState(0);
  const host = useRef<HTMLDivElement>(null);
  const chart = useRef<DiagramChart | null>(null);
  /** the curve the readout's point was last found on (a point can be on two) */
  const hint = useRef(-1);
  const activeRef = useRef(active);
  activeRef.current = active;

  const hidden = showEarlier ? 0 : earlier;
  const rawModel = useMemo(() => buildDiagramModel(points, labels, axes, hidden), [points, labels, axes, hidden]);
  /* the legend's entries (W630): a branch's or running job's curves by stability and kind; the appearance is keyed by the entry */
  const {entries, of: entryOf} = useMemo(() => legendEntries(rawModel.curves, liveKeys), [rawModel, liveKeys]);
  const model = useMemo(() => ({...rawModel, curves: rawModel.curves.map((c, i) => {
    const e = entries[entryOf[i]], a = appearance[e.key];
    return {...c, cssColor: a?.color, visible: a?.visible, legend: a?.label ?? e.name};
  })}), [rawModel, entries, entryOf, appearance]);
  const modelRef = useRef(model);
  modelRef.current = model;
  /* a hovered curve highlights all its entry's curves, read from a ref: the chart's hover handlers were made by the first render */
  const entriesRef = useRef({entries, of: entryOf});
  entriesRef.current = {entries, of: entryOf};
  const entryCurves = (curve: number) => entriesRef.current.entries[entriesRef.current.of[curve]].curves;
  const core: Ranges | null = useMemo(() => (axes && axes.xmax > axes.xmin && axes.ymax > axes.ymin
    ? {x: {min: axes.xmin, max: axes.xmax}, y: {min: axes.ymin, max: axes.ymax}} : null), [axes]);

  const hoverVertex = (curve: number, index: number) => {
    const c = modelRef.current.curves[curve];
    if (!c || index < 0 || index >= c.idx.length) return;
    const point = c.idx[index], p = viewOf(session.store.getState(), view).points;
    hint.current = curve;
    setTrace(curve); chart.current?.highlight(entryCurves(curve));
    setHover(session, {point, low: c.which === 'y2' && p.y2[point] !== p.y[point], view});
  };

  useEffect(() => {
    const c = new DiagramChart(host.current!, {
      onViewport: v => session.setDiagramViewport(view, v),
    });
    let detach = () => {};
    c.onArea = area => {
      detach();
      const off = attachGestures(c, area, {hover: hoverVertex, leave: () => { setHover(session, null); setTrace(null); c.highlight(null); }},
        autoxSink(session, () => activeRef.current, () => chart.current, () => modelRef.current));
      /* a click (not a box) on a two-parameter view stores the point for autoX's File/sElect 2par pt */
      let down: {x: number; y: number} | null = null;
      const onDown = (e: MouseEvent) => { down = e.button === 0 && !e.shiftKey ? {x: e.clientX, y: e.clientY} : null; };
      const onClick = (e: MouseEvent) => {
        const st = session.store.getState(), at = down;
        down = null;
        if (!at || Math.hypot(e.clientX - at.x, e.clientY - at.y) > 4) return;
        if (viewOf(st, view).axes?.plot !== 'two_parameter' || st.diagram.grabbing || st.pick || st.ask || st.busy) return;
        const r = area.getBoundingClientRect();
        const d = toData(c.ranges(), {fx: (e.clientX - r.left) / r.width, fy: (e.clientY - r.top) / r.height});
        session.autoxPoint(d.x, d.y);
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
    setDiagramChart(view, c);
    const ro = new ResizeObserver(() => {
      if (host.current?.clientWidth) c.resize();
      setArea(n => n + 1);
    });
    ro.observe(host.current!);
    return () => {
      ro.disconnect();
      detach();
      setDiagramChart(view, null, c);
      c.destroy();
    };
  }, [session, view]);

  useEffect(() => {
    chart.current!.set(model, core, viewOf(session.store.getState(), view).viewport, dark);
    setArea(n => n + 1);
  }, [model, core, dark]);

  useEffect(() => {
    chart.current!.applyViewport(viewport);
  }, [viewport]);

  /* the focus moves into the active view when the panel appears, so autoX's keys work at once */
  useEffect(() => {
    if (activeRef.current && !session.store.getState().ask) host.current?.focus({preventScroll: true});
  }, []);

  /* a grab or a plot mode takes the focus to the active view, so its keys work at once (A3) */
  useEffect(() => {
    if (active && (grabbing || pick)) host.current?.focus({preventScroll: true});
  }, [grabbing, pick?.ask, active]);

  /* the readout follows the grab's cursor */
  useEffect(() => {
    if (active && grabbing && info && info.point >= 0 && info.point < pointCount(points)) {
      hint.current = -1;
      setHover(session, {point: info.point, low: false, view});
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
    const v = viewOf(st, view);
    const to = grabStep(e.key, e.shiftKey, st.diagram.info?.point ?? -1, pointCount(v.points), v.labels);
    if (to === null) return false;
    if (ask) session.grabPoint(to);
    return true; /* a step while the last one is answered is dropped: the cursor is the core's */
  };

  /* a plot mode's keys (Axes/Zoom's box, Axes/Scroll's drag), as on the plot: the active view's */
  const pickKeyDown = (e: KeyboardEvent): boolean => {
    const c = chart.current, p = activePick(session);
    if (!c || !p || !activeRef.current || e.ctrlKey || e.metaKey || e.altKey) return false;
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
    const at = h ? vertexOf(m, h.point, h.low, h.view === view ? hint.current : -1) : null;
    if ((e.key === '<' || e.key === '>') && !e.ctrlKey && !e.metaKey) {
      const next = stepLabel(labels, h ? h.point : -1, e.key === '>' ? 1 : -1);
      if (next === null) return;
      e.preventDefault();
      e.stopPropagation();
      hint.current = -1;
      setHover(session, {point: next, low: false, view});
      return;
    }
    const key = e.ctrlKey || e.metaKey ? '' : e.key;
    if (key === 'Escape' && session.store.getState().busy) return; /* Escape stops the running command (hotkeys.ts), whatever the readout holds */
    const r = plotKey(key, {ranges: c.ranges(), hover: at, counts: m.curves.map(cd => cd.xs.length)});
    if (!r) return;
    e.preventDefault();
    e.stopPropagation(); /* not an XPP hotkey */
    if ('view' in r) c.setView(r.view);
    else if ('reset' in r) c.reset();
    else if (r.hover) hoverVertex(r.hover.curve, r.hover.index);
    else setHover(session, null);
  };

  /* a press anywhere in the view makes it the active one (not while the core asks: a grab takes
     its point from any view, a plot mode is the active view's) */
  const onPress = () => {
    const st = session.store.getState();
    if (!st.ask && !st.pick) session.activateView(view);
  };

  const at = hover ? vertexOf(model, hover.point, hover.low, hover.view === view ? hint.current : -1) : null;
  const marker = at && chart.current ? chart.current.position(at.curve, at.index) : null;
  /* the grab's cursor: the point in this view's own data (the same index everywhere); the core's
     place for it in the active view when the data do not have it */
  const cursorAt = !grabbing || !info ? null
    : info.point >= 0 && info.point < pointCount(points) ? {x: points.x[info.point], y: points.y[info.point]}
      : active ? {x: info.x ?? NaN, y: info.y ?? NaN} : null;
  const cursor = cursorAt && Number.isFinite(cursorAt.x) && Number.isFinite(cursorAt.y) && chart.current
    ? chart.current.place(cursorAt.x, cursorAt.y) : null;
  const storedAt = stored && chart.current ? chart.current.place(stored.x, stored.y) : null;
  const zoomed = viewport.x !== null || viewport.y !== null;
  const empty = !model.curves.length;
  const what = whatOf({name:session.store.getState().diagram.views[view].name,axes, points, labels, viewport});
  const label = empty ? `autoX diagram${views > 1 ? `, view ${view + 1}` : ''}, no branches yet`
    : `autoX diagram of ${what}${views > 1 ? ` (view ${view + 1})` : ''}: `
      + `${new Set(model.curves.map(c => c.branch)).size} branches, ${pointCount(points)} points, ${labels.length} labelled points`;
  const nEarlier = earlier ? branchesBefore(points, earlier) : 0;

  const area = chart.current?.areaBox() ?? null;
  const axisButton = (which: AxisName, text: string, box: NonNullable<typeof area>) => (
    <AxisTitle which={which} area={box} text={text || (which === 'x' ? 'x axis' : 'y axis')}
      aria-haspopup="dialog" aria-expanded={axisOpen === which}
      data-axis={which} title={`Change the ${which === 'x' ? 'horizontal' : 'vertical'} axis: what it plots and its range`}
      onKeyDown={e => e.stopPropagation()}
      onClick={() => setAxisOpen(axisOpen === which ? null : which)} />
  );
  const closeAxis = () => {
    const which = axisOpen;
    setAxisOpen(null);
    host.current?.querySelector<HTMLElement>(`.autox-axis-name[data-axis="${which}"]`)?.focus();
  };

  return (
    <section class={'autox-pane' + (active ? ' active' : '')} data-view={view} aria-current={active ? 'true' : undefined}
      aria-label={`${session.store.getState().diagram.views[view].name}${active ? ' (active)' : ''}`} onPointerDownCapture={onPress}
      onFocusCapture={onPress}>
      <header class="plot-bar autox-pane-bar">
        <WindowName name={session.store.getState().diagram.views[view].name}/>
        {views > 1 && (
          <h3 class="autox-pane-title" title={active ? 'The active view: autoX\'s Axes menu changes it' : 'Click to make it the active view'}>
            {what || `View ${view + 1}`}
          </h3>
        )}
        <ul class="autox-legend" aria-label="Key">
          {legendOf(model).map(l => (
            <li key={`${l.text}/${l.color}`}>
              <span class={'autox-swatch' + (l.dashed ? ' dashed' : '')} aria-hidden="true"
                style={{borderColor: cssColor(l.color, dark)}} />
              {l.text}
            </li>
          ))}
          {labelTypes(model.labels).map(sym => {
            const shape = labelShape(sym);
            return (
              <li key={sym} class="autox-legend-label" data-sym={sym} data-shape={shape} title={symbolHelp(sym)}>
                <span class={`autox-label-mark autox-label-mark-${shape}`} aria-hidden="true">{LABEL_GLYPH[shape]}</span>
                <b>{pointMark(sym)}</b> {pointLabel(sym)}
              </li>
            );
          })}
          {earlier > 0 && (
            <li>
              <button class={'small autox-earlier' + (showEarlier ? ' active' : '')} aria-pressed={showEarlier}
                title={showEarlier ? 'Hide the branches computed before Clear' : 'Show the branches computed before Clear'}
                onClick={() => session.setShowEarlier(!showEarlier)}>
                Earlier branches ({nEarlier})
              </button>
            </li>
          )}
        </ul>
        <div class="legend" role="group" aria-label="Branches">
          {entries.map(e => {
            const a = appearance[e.key], off = a?.visible === false, color = a?.color ?? cssColor(e.color, dark);
            return (
              <button key={e.key} data-auto-trace={e.key}
                class={'legend-item' + (off ? ' off' : '')} aria-pressed={!off}
                title="Click to show or hide; double click to edit name and color"
                onMouseEnter={() => chart.current?.highlight(e.curves)} onMouseLeave={() => chart.current?.highlight(null)}
                onClick={() => setAppearance(p => ({...p, [e.key]: {...p[e.key], visible: off}}))}
                onDblClick={() => setEditing({key: e.key, label: a?.label ?? e.name, color})}>
                <span class="swatch" style={{background: color}} aria-hidden="true" /><ShownName name={a?.label ?? e.name} copy={false} />
              </button>
            );
          })}
        </div>
        <div class="plot-tools">
          <button disabled={!zoomed} onClick={() => chart.current!.reset()}
            title="Back to autoX's axes (double click, or 0 on the diagram)">Reset zoom</button>
          <button disabled={empty} onClick={() => chart.current!.fit()}
            title="Fit the view to the branches shown (client-side; earlier branches only if Earlier branches is on)">
            Fit
          </button>
          {views > 1 && (
            <button class="autox-pane-close" aria-label={`Close ${session.store.getState().diagram.views[view].name}`}
              title="Close this view of the diagram (the diagram stays, and so do the other views)"
              onClick={() => session.closeView(view)}>×</button>
          )}
        </div>
      </header>
      {editing && <TraceDialog label={editing.label} color={editing.color} close={() => setEditing(null)}
        apply={(label, color) => { setAppearance(a => ({...a, [editing.key]: {...a[editing.key], label, color}})); setEditing(null); }} />}
      <div class={'plot-host autox-host' + (grabbing ? ' picking pick-grab' : pick ? ` picking pick-${pick.mode}` : '')}
        ref={host} tabIndex={0} role="application" aria-roledescription="diagram"
        aria-label={label} onKeyDown={onHostKey} aria-describedby={grabbing ? 'grab-instruction grab-keys-help'
          : pick ? 'pick-instruction autox-keys-help' : 'autox-keys-help'}>
        {trace !== null && model.curves[trace] && <span class="trace-tooltip" role="tooltip">{model.curves[trace].legend}</span>}
        {!empty && (
          <FitButton onClick={() => chart.current!.fit()}
            title="Fit the view to the branches shown (client-side; earlier branches only if Earlier branches is on)" />
        )}
        {marker && <span class="hover-dot" style={{left: `${marker.left}px`, top: `${marker.top}px`}} />}
        {cursor && <span class="autox-cursor" aria-hidden="true" style={{left: `${cursor.left}px`, top: `${cursor.top}px`}} />}
        {storedAt && (
          <span class="autox-cursor autox-stored" style={{left: `${storedAt.left}px`, top: `${storedAt.top}px`}}
            title={`Stored point: ${sixSig(stored!.x)}, ${sixSig(stored!.y)}`} />
        )}
        {pick && chart.current && <PickOverlay pick={pick} chart={chart.current} />}
        {area && axes && axisButton('x', axes.xlabel, area)}
        {area && axes && axisButton('y', axes.ylabel, area)}
        {empty && (
          <div class="plot-empty autox-empty">
            <p>{busy ? 'autoX is running…' : 'No branches yet: Run starts a continuation from the current point.'}</p>
          </div>
        )}
      </div>
      {axisOpen && <AutoxAxisDialog key={axisOpen} axis={axisOpen} view={view} onClose={closeAxis} />}
    </section>
  );
}

/** what the readout says: the point under the mouse, in the view it is on */
function Readout() {
  const hover = useStore(s => s.diagram.hover);
  const v = useStore(s => viewOf(s, s.diagram.hover?.view ?? s.diagram.active));
  const activePoints = useStore(s => activeView(s.diagram).points);
  const run = useStore(s => s.diagram.run);
  const stop = useStore(s => s.diagram.stop);
  const stopAt = stopPoint(run, activePoints, stop);
  const said = hover && hover.point < pointCount(v.points)
    ? describePoint(v.points, v.labels, v.axes, hover.point, stopAt >= 0 && stop && !run?.active ? {point: stopAt, text: stop.text} : null)
    : null;
  return (
    <footer class="readout autox-readout" role="status" aria-live="polite">
      {said ? (
        <span>
          <b>{said.head}</b> · {said.kind}
          {said.label && <> · <b class="autox-label">{said.label}</b></>}
          {said.values.map(v => <span key={v.name}> · {v.name} = <ShownNumber value={v.value} /></span>)}
        </span>
      ) : (
        <span class="muted">
          <span class="hint-mouse">Drag to zoom · wheel zooms · Shift+drag pans · double click resets · &lt; &gt; step through the labels</span>
          <span class="hint-touch">Pinch zooms · drag pans · tap a point to read it</span>
        </span>
      )}
    </footer>
  );
}

function AutoxPanel({dark}: {dark: boolean}) {
  const session = useSession();
  /* a button that opens the page's own form works during a run too (its edits wait); the others
     by the kind of the key they send (W95: Axes and Clear are views, Run, Grab and File wait) */
  const mayKey = useMayKey();
  const off = (op: AutoxOp) => !SETTINGS_DIALOG[op] && !mayKey('autox', op);
  const grabbing = useStore(s => s.diagram.grabbing);
  const stored = useStore(s => s.diagram.stored);
  const pick = useStore(s => (s.pick?.win === autoxWindow(s) && !s.pick.waiting ? s.pick : null));
  const views = useStore(s => s.diagram.views.length);
  const what = useStore(s => whatOf(activeView(s.diagram)));
  const pending = pendingFields(useStore(s => s.autoxSettings));
  /* a button: its form (T22), else its command */
  const act = (op: AutoxOp) => session.autoxOp(op);

  const panel = useRef<HTMLElement>(null);

  /* Escape hides the panel through hotkeys.ts's one owner (openPanel), only while nothing runs: the
     autoX view is where its run is watched, so there Escape stops the run (W536), never hides it */
  const running = useStore(s => s.busy);
  const hello = useStore(s => s.hello);
  useEffect(() => running ? undefined : openPanel(() => session.showAutox(false)), [session, running]);

  return (
    <section id="autox-panel" ref={panel} class="autox-panel" aria-label="autoX">
      <div class="panel-header autox-header">
        <button class="autox-back" onClick={() => session.showAutox(false)}
          title="autoX stays open, with its diagram and a running continuation; Show autoX brings it back">Hide autoX</button>
        <h2>autoX <span class="muted autox-what">{views > 1 ? `${views} views, the active one ${what}` : what}</span></h2>
        <HelpButton target={HELP.autoxView} label="autoX" />
        <button class="autox-close" onClick={() => session.closeAutox()}
          title="Done with autoX: close its window (a running continuation is stopped first; File/autoX opens it again)">
          Close
        </button>
      </div>
      <div class="autox-tools" role="toolbar" aria-label="autoX">
        {BUTTONS.map(op => {
          const row = toolbarRow(hello, 'autox', op);
          const k = '';
          const kind = SETTINGS_DIALOG[op];
          const waits = !!kind && [...pending].some(PENDING_OF[kind]);
          return (
            <button key={op} disabled={off(op)} aria-keyshortcuts={k.toUpperCase()}
              class={waits ? 'autox-pending' : undefined} data-op={op} data-button={`autox:${op}`}
              title={off(op) ? BUSY_TITLE
                : (TITLES[op] ?? row?.description ?? '') + (waits ? ' (changes wait for the run to end)' : '')}
              onClick={() => act(op)}>{row?.label}</button>
          );
        })}
        <SetsRow scope="continuation" />
      </div>
      <div class="autox-view">
        {grabbing && <GrabBar />}
        {pick && <PickBar pick={pick} />}
        <div class="autox-views" data-views={views}>
          {Array.from({length: views}, (_, k) => <DiagramPane key={k} view={k} dark={dark} />)}
        </div>
        <Readout />
      </div>
      <AutoxInfo />
      <AutoxOutput />
      {stored && (
        <p class="autox-stored-text muted">Stored point for File/sElect 2par pt: <ShownNumber value={stored.x} />, <ShownNumber value={stored.y} /></p>
      )}
      {/* at the bottom, as the main window's status bar (T24) */}
      <AutoxStatus />
      <p id="autox-keys-help" class="visually-hidden">{KEYS_HELP}</p>
      <p id="grab-keys-help" class="visually-hidden">{GRAB_HELP}</p>
    </section>
  );
}

export function AutoxView({dark}: {dark: boolean}) {
  const open = useStore(s => s.diagram.open);
  const shown = useStore(s => s.diagram.shown);
  const wasShown = useRef(false);

  /* the focus goes back to the main plot when the panel is hidden or autoX closes (T21):
     the keys typed next are the main window's (Show autoX is a Tab away). A layout effect, in the commit
     that removes the panel */
  useLayoutEffect(() => {
    const inPanel = !!document.activeElement?.closest?.('.autox-panel') || document.activeElement === document.body;
    if (wasShown.current && (!shown || !open) && inPanel)
      document.querySelector<HTMLElement>('.plot-view:not([hidden]) .plot-host')?.focus();
    wasShown.current = open && shown;
  }, [open, shown]);

  if (open && shown) return <AutoxPanel dark={dark} />;
  return null;
}

/* Show autoX (the view hidden): a row at the top of the workspace, above the plot
   (always in view, a touch target on a phone), in the flow, so it never covers the message strip (W98) */
export function AutoxShow() {
  const session = useSession();
  const open = useStore(s => s.diagram.open);
  const shown = useStore(s => s.diagram.shown);
  if (!open || shown) return null;
  return (
    <button class="autox-show" aria-controls="autox-panel" aria-expanded="false"
      onClick={() => session.showAutox(true)} title="Show the autoX view again">Show autoX</button>
  );
}
