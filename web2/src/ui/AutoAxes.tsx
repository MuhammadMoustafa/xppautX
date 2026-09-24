/* The AUTO diagram's axis dialog (docs/ui-v2.md T21): a click on an axis
   name opens it beside the diagram. Its minimum and maximum change the view
   at once, in the page only (a zoom: Undo zoom and Reset view take it
   back), so they work while AUTO runs. What the axis plots (the parameter,
   the y axis's plot type and its variable or second parameter) is AUTO's
   Axes setting: a change goes to the core (session.autoAxes: Axes with the
   new names at the view's ranges, then Fit) and is disabled while a
   command runs ("can change when AUTO stops"). Not modal: Escape or Close
   closes it, the focus goes back to the axis name. */
import {useEffect, useRef, useState} from 'preact/hooks';
import {axesNames, boundText, PLOT_TYPES, spinStep, typedRange, yNeeds} from '../plot/axisDialog';
import type {Range} from '../store/plots';
import {BUSY_TITLE, useSession, useStore} from './context';

export type AxisName = 'x' | 'y';

export function AutoAxisDialog({axis, onClose}: {axis: AxisName; onClose: () => void}) {
  const session = useSession();
  const axes = useStore(s => s.diagram.axes);
  const viewport = useStore(s => s.diagram.viewport);
  const busy = useStore(s => s.busy);
  const ics = useStore(s => s.core?.ics), parValues = useStore(s => s.core?.pars);
  const vars = (ics ?? []).map(([n]) => n), pars = (parValues ?? []).map(([n]) => n);
  const box = useRef<HTMLDivElement>(null);
  const edited = useRef(false);
  const shown = (): Range | null => viewport[axis]
    ?? (axes ? (axis === 'x' ? {min: axes.xmin, max: axes.xmax} : {min: axes.ymin, max: axes.ymax}) : null);
  const [minText, setMin] = useState(() => (shown() ? boundText(shown()!.min) : ''));
  const [maxText, setMax] = useState(() => (shown() ? boundText(shown()!.max) : ''));
  /* the texts as typed, for two edits in one tick (the state is only read at the next render) */
  const typed = useRef({min: minText, max: maxText});
  const setMinText = (t: string) => { typed.current.min = t; setMin(t); };
  const setMaxText = (t: string) => { typed.current.max = t; setMax(t); };

  /* the fields follow the view when it changes elsewhere (a zoom, the core's Fit), not while typed in */
  useEffect(() => {
    const r = shown();
    if (!r || box.current?.contains(document.activeElement) && document.activeElement?.tagName === 'INPUT') return;
    setMinText(boundText(r.min));
    setMaxText(boundText(r.max));
  }, [viewport, axes]);

  useEffect(() => {
    box.current?.querySelector<HTMLElement>('select:not([disabled]), input')?.focus();
  }, []);

  const apply = (lo: string, hi: string) => {
    const r = typedRange(lo, hi);
    if (!r) return;
    const v = {...session.store.getState().diagram.viewport, [axis]: r};
    if (!v.x && axes) v.x = {min: axes.xmin, max: axes.xmax};
    if (!v.y && axes) v.y = {min: axes.ymin, max: axes.ymax};
    /* the first change of the dialog is one Undo away, the rest are the same step */
    session.store.dispatch({type: 'diagram', action: {type: 'viewport', viewport: v, push: !edited.current}});
    edited.current = true;
  };
  const r = typedRange(minText, maxText);
  const step = spinStep((r ?? shown() ?? {min: 0, max: 10}).max - (r ?? shown() ?? {min: 0, max: 10}).min);

  const names = axesNames(axes);
  const plot = axes?.plot ?? 0;
  const need = yNeeds(plot);
  const ranges = () => {
    const d = session.store.getState().diagram, a = d.axes!;
    return {x: d.viewport.x ?? {min: a.xmin, max: a.xmax}, y: d.viewport.y ?? {min: a.ymin, max: a.ymax}};
  };
  const change = (c: {plot?: number; yvar?: string; par1?: string; par2?: string}) => {
    if (!axes || session.store.getState().busy) return;
    session.autoAxes({...c, ranges: ranges()});
  };
  const select = (label: string, value: string, options: {value: string; text: string}[], on: (v: string) => void,
    field: string) => (
    <label>
      <span>{label}</span>
      <select value={value} disabled={busy} data-field={field} aria-describedby={busy ? 'auto-axis-busy' : undefined}
        title={busy ? BUSY_TITLE : undefined}
        onChange={e => on((e.target as HTMLSelectElement).value)}>
        {!options.some(o => o.value === value) && <option value={value}>{value || '(as set)'}</option>}
        {options.map(o => <option key={o.value} value={o.value}>{o.text}</option>)}
      </select>
    </label>
  );
  const listOf = (l: string[]) => l.map(n => ({value: n, text: n}));

  const onKeyDown = (e: KeyboardEvent) => {
    if (e.key !== 'Escape') return;
    e.preventDefault();
    e.stopPropagation();
    onClose();
  };
  const title = axis === 'x' ? 'Horizontal axis' : 'Vertical axis';
  return (
    <div class="auto-axis-dialog" ref={box} role="dialog" aria-modal="false" aria-label={title} data-axis={axis}
      onKeyDown={onKeyDown}>
      <h3>{title}</h3>
      <div class="auto-axis-fields">
        {axis === 'x'
          ? select('Parameter', names.par1, listOf(pars), v => change({par1: v}), 'par1')
          : select('Plots', String(plot), PLOT_TYPES.map(t => ({value: String(t.plot), text: t.text})),
            v => change({plot: Number(v)}), 'plot')}
        {axis === 'y' && need === 'var' && select('Variable', names.yvar, listOf(vars), v => change({yvar: v}), 'yvar')}
        {axis === 'y' && need === 'par2' && select('Second parameter', names.par2, listOf(pars), v => change({par2: v}), 'par2')}
        <label>
          <span>Minimum</span>
          <input type="number" step={step} value={minText} data-field="min" aria-invalid={!r}
            onInput={e => { const t = (e.target as HTMLInputElement).value; setMinText(t); apply(t, typed.current.max); }} />
        </label>
        <label>
          <span>Maximum</span>
          <input type="number" step={step} value={maxText} data-field="max" aria-invalid={!r}
            onInput={e => { const t = (e.target as HTMLInputElement).value; setMaxText(t); apply(typed.current.min, t); }} />
        </label>
      </div>
      {busy && <p id="auto-axis-busy" class="muted">What the axis plots can change when AUTO stops; the range changes now.</p>}
      {!r && <p class="error-text" role="alert">The minimum must be a number below the maximum.</p>}
      <div class="dialog-actions">
        <button onClick={onClose}>Close</button>
      </div>
    </div>
  );
}
