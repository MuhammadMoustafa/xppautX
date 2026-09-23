/* The values panel (docs/ui-v2.md T3): parameters, initial conditions,
   boundary conditions, delay initial data, sliders (the model's `@ s1=`
   ones and the user's own), the model's user buttons, and Default. A right
   column from 80rem, a section under the plot from 48rem, a full-screen
   sheet with a Back button below that (R6). An edit sends `set`; a slider
   drag sends `slide`, throttled like the classic panel while a command is
   busy; Ctrl+Z (while the focus is inside) or the Undo button restores the
   previous value (A12) through session.undoValue(). A rejected value comes
   back as a `message` `error`, shown on the field it belongs to (A11), not
   as a modal. */
import {useEffect, useMemo, useRef, useState} from 'preact/hooks';
import type {Session} from '../session';
import {fieldKey, sixSig, type ValueKind} from '../store/values';
import {useSession, useStore} from './context';

const NUMBER_HINT = 'A number, or %formula such as %2*pi';
const FOCUSABLE = 'button:not([disabled]), input, select, [tabindex]:not([tabindex="-1"])';

/* ---- one field: display precision until focused, full precision while editing (A14) ---- */

function ValueField({kind, label, name, index, display, full, hint, numeric}: {
  kind: ValueKind; label: string; name?: string; index?: number; display: string; full: string; hint: string;
  numeric: boolean;
}) {
  const session = useSession();
  const field = fieldKey(kind, index ?? name!);
  const error = useStore(s => s.values.errors[field]);
  const [editing, setEditing] = useState(false);
  const [draft, setDraft] = useState(display);
  useEffect(() => { if (!editing) setDraft(display); }, [display, editing]);
  const id = `value-${field}`.replace(/[^\w-]/g, '_');
  const errId = error ? `${id}-err` : undefined;
  /* Escape blurs the field too: its blur must not commit the draft it drops */
  const dropped = useRef(false);
  const revert = () => setDraft(display);
  const commit = () => {
    setEditing(false);
    if (dropped.current) { dropped.current = false; revert(); return; }
    const text = draft.trim();
    if (text === '' || text === full) { revert(); return; }
    if (numeric && !text.startsWith('%') && !Number.isFinite(Number(text))) { revert(); return; }
    if (index !== undefined) session.setValueByIndex(kind as 'bc' | 'delay', index, text, full);
    else session.setValue(kind as 'par' | 'ic', name!, text, full);
  };
  return (
    <div class="value-field">
      <label htmlFor={id}>
        <span class="value-name" title={label}>{label}</span>
        <input id={id} value={editing ? draft : display} title={hint} spellcheck={false} autocomplete="off"
          aria-invalid={error ? 'true' : undefined} aria-describedby={errId}
          onFocus={() => { setEditing(true); setDraft(full); }}
          onInput={e => setDraft((e.target as HTMLInputElement).value)}
          onBlur={commit}
          onKeyDown={e => {
            if (e.key === 'Enter') { e.preventDefault(); (e.target as HTMLInputElement).blur(); }
            else if (e.key === 'Escape') { e.stopPropagation(); dropped.current = true; (e.target as HTMLInputElement).blur(); }
          }}
        />
      </label>
      {error && <p class="field-error" id={errId} role="alert">{error}</p>}
    </div>
  );
}

function ValueTable({title, kind, entries, byIndex, hint, numeric, onDefault}: {
  title: string; kind: ValueKind; entries: [string, string | number][]; byIndex: boolean;
  hint: string; numeric: boolean; onDefault?: () => void;
}) {
  if (!entries.length) return null;
  return (
    <section class="value-group" aria-label={title}>
      <div class="value-group-head">
        <h3>{title}</h3>
        {onDefault && <button class="small" onClick={onDefault} title={`${title} from the ODE file`}>Default</button>}
      </div>
      <div class="value-list">
        {entries.map(([name, value], i) => {
          const display = typeof value === 'number' ? sixSig(value) : value;
          const full = typeof value === 'number' ? String(value) : value;
          return (
            <ValueField key={byIndex ? i : name.toLowerCase()} kind={kind} label={byIndex ? `${title} ${i + 1}` : name}
              name={byIndex ? undefined : name} index={byIndex ? i : undefined}
              display={display} full={full} hint={hint} numeric={numeric} />
          );
        })}
      </div>
    </section>
  );
}

/* ---- sliders: the model's `@ s1=`.. presets, or the user's own pick and range ---- */

/** only the latest position matters while a command is running (like the classic panel) */
function useSlide(session: Session) {
  const pending = useRef<{name: string; value: number} | null>(null);
  const busy = useStore(s => s.busy);
  const busyRef = useRef(busy);
  busyRef.current = busy;
  useEffect(() => {
    if (!busy && pending.current) {
      const p = pending.current;
      pending.current = null;
      session.slide(p.name, p.value);
    }
  }, [busy]);
  return (name: string, value: number) => {
    if (busyRef.current) { pending.current = {name, value}; return; }
    session.slide(name, value);
  };
}

function Slider({index, def}: {index: number; def?: {name: string; lo: number; hi: number}}) {
  const session = useSession();
  const pars = useStore(s => s.core?.pars ?? []);
  const ics = useStore(s => s.core?.ics ?? []);
  const names = useMemo(() => [...pars, ...ics].map(([n]) => n), [pars, ics]);
  const [name, setName] = useState(def?.name ?? '');
  const [lo, setLo] = useState(String(def?.lo ?? 0));
  const [hi, setHi] = useState(String(def?.hi ?? 1));
  const seeded = useRef(false);
  useEffect(() => {
    if (!seeded.current && def) {
      seeded.current = true;
      setName(def.name);
      setLo(String(def.lo));
      setHi(String(def.hi));
    }
  }, [def]);
  const slide = useSlide(session);
  const gesture = useRef<string | null>(null);
  const match = names.find(n => n.toLowerCase() === name.toLowerCase()) ?? '';
  const value = match
    ? [...pars, ...ics].find(([n]) => n.toLowerCase() === match.toLowerCase())?.[1]
    : undefined;
  const a = Number(lo), b = Number(hi);
  const ranged = Number.isFinite(a) && Number.isFinite(b) && a !== b;
  const pos = value !== undefined && ranged ? Math.max(0, Math.min(1000, Math.round(1000 * (value - a) / (b - a)))) : 500;
  const label = `Slider ${index + 1}`;
  return (
    <div class="value-slider">
      <label class="visually-hidden" htmlFor={`slider-pick-${index}`}>{label}: parameter or variable</label>
      <select id={`slider-pick-${index}`} value={match} onChange={e => setName((e.target as HTMLSelectElement).value)}>
        <option value="">Par/Var…</option>
        {names.map(n => <option key={n} value={n}>{n}</option>)}
      </select>
      <label class="visually-hidden" htmlFor={`slider-lo-${index}`}>{label}: low end</label>
      <input id={`slider-lo-${index}`} class="value-slider-lim" title="Low end" value={lo}
        onInput={e => setLo((e.target as HTMLInputElement).value)} />
      <input type="range" class="value-slider-range" min={0} max={1000} step={1} value={pos} disabled={!match}
        aria-label={match ? `${match} slider` : label}
        title="Drag or use the arrow keys to change the value and integrate again"
        onInput={e => {
          if (!match || !ranged) return;
          if (gesture.current === null) gesture.current = value !== undefined ? String(value) : null;
          const v = a + (b - a) * Number((e.target as HTMLInputElement).value) / 1000;
          slide(match, v);
        }}
        onChange={() => {
          if (match && gesture.current !== null) {
            const kind: ValueKind = pars.some(([n]) => n.toLowerCase() === match.toLowerCase()) ? 'par' : 'ic';
            session.recordEdit({kind, name: match, previous: gesture.current});
          }
          gesture.current = null;
        }}
      />
      <span class="value-slider-val" aria-hidden="true">{value !== undefined ? sixSig(value) : ''}</span>
      <label class="visually-hidden" htmlFor={`slider-hi-${index}`}>{label}: high end</label>
      <input id={`slider-hi-${index}`} class="value-slider-lim" title="High end" value={hi}
        onInput={e => setHi((e.target as HTMLInputElement).value)} />
    </div>
  );
}

function SlidersBlock() {
  const defs = useStore(s => s.hello?.sliders ?? []);
  return (
    <section class="value-group" aria-label="Sliders">
      <div class="value-group-head"><h3>Sliders</h3></div>
      <div class="value-list">
        {[0, 1, 2].map(i => <Slider key={i} index={i} def={defs[i]} />)}
      </div>
    </section>
  );
}

function UserButtonsBlock() {
  const session = useSession();
  const names = useStore(s => s.hello?.userbuttons ?? []);
  if (!names.length) return null;
  return (
    <section class="value-group" aria-label="Buttons">
      <div class="value-list value-buttons">
        {names.map((name, i) => (
          <button key={i} onClick={() => session.userButton(i)} title="A button defined in the ODE file">{name}</button>
        ))}
      </div>
    </section>
  );
}

/* ---- the panel: a column, a section, or a sheet, depending on the width (R6) ---- */

export function ValuesPanel() {
  const session = useSession();
  const open = useStore(s => s.valuesOpen);
  const history = useStore(s => s.values.history);
  const pars = useStore(s => s.core?.pars ?? []);
  const ics = useStore(s => s.core?.ics ?? []);
  const bcs = useStore(s => s.core?.bcs ?? []);
  const delays = useStore(s => s.core?.delays ?? []);
  const panel = useRef<HTMLElement>(null);
  const close = () => session.store.dispatch({type: 'valuesPanel', open: false});

  useEffect(() => {
    if (!open) {
      if (panel.current?.contains(document.activeElement)) document.querySelector<HTMLElement>('.values-toggle')?.focus();
      return;
    }
    panel.current?.querySelector<HTMLElement>(FOCUSABLE)?.focus();
    /* Escape closes the sheet wherever the focus is inside it (narrow only: CSS keeps it open elsewhere) */
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== 'Escape' || session.store.getState().ask) return;
      e.preventDefault();
      e.stopPropagation();
      close();
    };
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, [open]);

  const last = history[history.length - 1];
  const undoLabel = last ? `Undo: restore ${last.name ?? `${last.kind} ${(last.index ?? 0) + 1}`}` : 'Nothing to undo';

  return (
    <section id="values-panel" ref={panel} class={'values-panel' + (open ? ' open' : '')} aria-label="Values"
      onKeyDown={e => {
        if ((e.ctrlKey || e.metaKey) && (e.key === 'z' || e.key === 'Z')) {
          e.preventDefault();
          session.undoValue();
        }
      }}>
      <div class="values-header">
        <button class="values-back" onClick={close}>Back</button>
        <h2>Values</h2>
        <button class="small" disabled={!history.length} title={undoLabel} onClick={() => session.undoValue()}>
          Undo
        </button>
      </div>
      <div class="values-body">
        <SlidersBlock />
        <UserButtonsBlock />
        <ValueTable title="Parameters" kind="par" entries={pars} byIndex={false} hint={NUMBER_HINT} numeric
          onDefault={() => session.defaultValues('par')} />
        <ValueTable title="Initial conditions" kind="ic" entries={ics} byIndex={false} hint={NUMBER_HINT} numeric
          onDefault={() => session.defaultValues('ic')} />
        <ValueTable title="Boundary conditions" kind="bc" entries={bcs} byIndex
          hint="An expression that is zero at the boundary" numeric={false} />
        <ValueTable title="Delay initial data" kind="delay" entries={delays ?? []} byIndex
          hint="An expression in t for t < 0" numeric={false} />
      </div>
    </section>
  );
}
