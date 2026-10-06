/* The values panel (docs/ui-v2.md T3, GitHub #18, #155): parameters, the
   state (each variable's initial condition beside where the last run is
   now), boundary conditions, delay initial data, the numerics (W106), and
   the model's user buttons. A right column from 80rem, a section under
   the plot from 48rem, a full-screen sheet with a Back button below that
   (R6). The sliders are under the plot (SliderStrip.tsx).

   Every field is a setting (W106): an edit goes to the core at once as a
   `set`, during a computation too, when the core applies it as that
   computation ends (the run in progress keeps the values it started with,
   the next one uses the new ones). The field shows what was sent: the
   value shown is the value, nothing is pending. A rejected value comes
   back as a `message` `error`, shown on its field
   (A11); a text the field does not take (Field.tsx: a number or
   %formula, an expression for BCs and delays) is marked and never sent
   (T31). Each parameter and IC has a reset to the model file's value
   (its title names it) and is marked when it differs; there is no undo
   (GitHub #110): Reset is the way back. Each section folds (remembered
   per viewer), and Parameters and State save and load XPP's own files. */
import {useEffect, useRef, useState} from 'preact/hooks';
import {useFocusBackOnClose} from './focusBack';
import type {ComponentChildren} from 'preact';
import {HELP} from '../help/links';
import {EXPRESSION, FORMULA, FORMULA_HINT, NUMBER, fieldMessage, type FieldSpec} from '../store/fieldKinds';
import {fieldKey, foldKey, inspectNumber, isFolded, sentText, showsBcSection, sixSig, type ValueKind} from '../store/values';
import {sampledTailRate, STATE_TAIL_INTERVALS} from '../store/series';
import type {NumericsField} from '../protocol/types';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {Field} from './Field';
import {HelpButton} from './HelpButton';
import {FOCUSABLE} from './dialogFocus';

const NUMBER_HINT = fieldMessage(FORMULA_HINT);
const STATE_HINT = 'Edits apply on Enter or leaving the field, to the next run. Run from initial uses Initial; Run from current uses the last state and starts a new trajectory.';

/* ---- folded sections, remembered per viewer ---- */

const FOLD_KEY = 'xpp.values.folded';

function readFolded(): string[] {
  try {
    const v = JSON.parse(localStorage.getItem(FOLD_KEY) ?? '[]');
    return Array.isArray(v) ? v.filter((x): x is string => typeof x === 'string') : [];
  } catch {
    return [];
  }
}

/* a section that starts folded (startFolded) is remembered by what the person opened:
   its entry is "+id", the others' is "id" for folded */
function useFolded(id: string, startFolded = false): [boolean, () => void] {
  const key = foldKey(id, startFolded);
  const [folded, setFolded] = useState(() => isFolded(readFolded(), id, startFolded));
  const toggle = () => {
    const next = !folded;
    setFolded(next);
    try {
      const all = readFolded().filter(x => x !== key);
      localStorage.setItem(FOLD_KEY, JSON.stringify(next !== startFolded ? [...all, key] : all));
    } catch {
      /* no storage: it lasts this page */
    }
  };
  return [folded, toggle];
}

function Section({id, title, hint, tools, startFolded, children}: {
  id: string; title: string; hint?: string; tools?: ComponentChildren; startFolded?: boolean;
  children: ComponentChildren;
}) {
  const [folded, toggle] = useFolded(id, startFolded);
  const bodyId = `values-sec-${id}`;
  return (
    <section class={'value-group' + (folded ? ' folded' : '')} aria-label={title} data-section={id}>
      <div class="value-group-head">
        <h3>
          <button class="value-fold" aria-expanded={!folded} aria-controls={bodyId} onClick={toggle}
            title={hint ?? `Show or hide ${title.toLowerCase()}`}>
            <span class="value-fold-mark" aria-hidden="true">{folded ? '▸' : '▾'}</span>{title}
          </button>
        </h3>
        <HelpButton target={HELP.valuesPanel} label={title} />
      </div>
      <div id={bodyId} hidden={folded}>
        {hint && <p class="value-hint">{hint}</p>}
        {tools && <div class="value-tools">{tools}</div>}
        {children}
      </div>
    </section>
  );
}

/* ---- one field: display precision until focused, full precision while editing (A14) ---- */

function ValueField({kind, label, name, index, display, full, hint, spec, extra}: {
  kind: ValueKind; label: string; name?: string; index?: number; display: string; full: string; hint: string;
  spec: FieldSpec; extra?: ComponentChildren;
}) {
  const session = useSession();
  const field = fieldKey(kind, index ?? name!);
  const error = useStore(s => s.values.errors[field]);
  /* an edit sent whose `set` has not ended (during a computation, until
     the computation ends and the set with it) is what the field shows,
     and its draft stays put until then, so a formula the core refuses
     keeps showing it (WF-001) and the core's state during a run (the
     values that run started with) never overwrites what was typed */
  const sent = useStore(s => sentText(s.values.inflight, field));
  const settling = sent !== null;
  const def = useStore(s => (kind === 'par' || kind === 'ic' ? s.values.defaults?.[field] ?? null : null));
  const id = `value-${field}`.replace(/[^\w-]/g, '_');
  /* Field sends only a text it takes that differs from where this focus
     started: a value that moved under a focused, untouched box (a Reset, a
     slider on the same name) is not sent back as if it were an edit */
  const commit = (text: string) => {
    if (index !== undefined) session.setValueByIndex(kind as 'bc' | 'delay', index, text);
    else session.setValue(kind as 'par' | 'ic', name!, text);
  };
  const changed = def !== null && Number(sent ?? full) !== def;
  const title = def !== null ? `${hint}; default: ${sixSig(def)}` : hint;
  return (
    <div class={'value-field' + (changed ? ' changed' : '')}>
      <label htmlFor={id} class="value-name" title={label}>{label}</label>
      <Field id={id} spec={spec} value={sent ?? display} editValue={sent ?? full} onCommit={commit} error={error ?? null}
        settling={settling} onDropError={() => session.store.dispatch({type: 'values', action: {type: 'clearError', field}})}
        title={title} />
      {extra}
      {def !== null && name !== undefined && (
        <button class="value-reset icon" title={`default: ${sixSig(def)}`} disabled={!changed}
          aria-label={`Reset ${label} to its default, ${sixSig(def)}`}
          onClick={() => session.resetValue(kind as 'par' | 'ic', name)}>↺</button>
      )}
    </div>
  );
}

function fieldProps(value: string | number) {
  return {
    display: typeof value === 'number' ? inspectNumber(value) : value,
    full: typeof value === 'number' ? String(value) : value,
  };
}

/** Save and Load of a section's values, both through the core (session.ts
    saveValues/loadValues, docs/protocol.md "values", W66 review) */
function FileTools({kind}: {kind: 'par' | 'ic'}) {
  const session = useSession();
  const input = useRef<HTMLInputElement>(null);
  const what = kind === 'par' ? 'parameters' : 'initial conditions';
  /* a file written or read: data, not while a computation runs (W95) */
  const saveOff = !useMay()({cmd: 'values', op: 'write'});
  return (
    <>
      <button class="small" onClick={() => session.saveValues(kind)} disabled={saveOff}
        title={saveOff ? BUSY_TITLE : `Save the ${what} as a file XPP reads (${kind === 'par' ? 'File/Read par' : 'Initialconds/File, --icfile'})`}>
        Save
      </button>
      <button class="small" onClick={() => input.current?.click()} disabled={saveOff}
        title={saveOff ? BUSY_TITLE : `Load ${what} from a file in XPP's own format`}>Load</button>
      <input ref={input} id={`values-load-${kind}`} type="file" hidden
        onChange={async e => {
          const el = e.target as HTMLInputElement, file = el.files?.[0];
          if (file) await session.loadValues(kind, file);
          el.value = '';
        }} />
    </>
  );
}

function Parameters() {
  const session = useSession();
  const pars = useStore(s => s.core?.pars);
  if (!pars?.length) return null;
  return (
    <Section id="par" title="Parameters" hint="Edit for the next run; Enter or leave a field to apply." tools={(
      <>
        <FileTools kind="par" />
        <button class="small" onClick={() => session.defaultValues('par')} title="Every parameter from the ODE file">
          Reset all
        </button>
      </>
    )}>
      <div class="value-list">
        {pars.map(([name, value]) => (
          <ValueField key={name.toLowerCase()} kind="par" label={name} name={name} hint={NUMBER_HINT} spec={FORMULA}
            {...fieldProps(value)} />
        ))}
      </div>
    </Section>
  );
}

/** Full solver values for every state, during the run and at its end. */
function useNow(): (number | null)[] {
  const ics = useStore(s => s.core?.ics);
  const now = useStore(s => s.core?.now);
  return (ics ?? []).map((_, i) => now?.[i] ?? null);
}

function StateSection() {
  const session = useSession();
  const ics = useStore(s => s.core?.ics);
  const now = useNow();
  const series = useStore(s => s.plots.windows.find(w => w.win === s.plots.active)?.series ?? null);
  if (!ics?.length) return null;
  return (
    <Section id="ic" title="States" hint={STATE_HINT} tools={(
      <>
        <FileTools kind="ic" />
        <button class="small" onClick={() => session.defaultValues('ic')} title="Every initial condition from the ODE file">
          Reset all
        </button>
      </>
    )}>
      <div class="value-cols" aria-hidden="true"><span /><span>Initial</span><span>Current</span><span>Tail rate</span></div>
      <div class="value-list value-state">
        {ics.map(([name, value], i) => {
          const time = series?.columns.get(0);
          const column = series?.names.get(i + 1)?.toLowerCase() === name.toLowerCase() ? series.columns.get(i + 1) : undefined;
          const rate = now[i] !== null && time && column ? sampledTailRate(time, column, series!.rows) : null;
          return (
          <ValueField key={name.toLowerCase()} kind="ic" label={name} name={name} hint={NUMBER_HINT} spec={FORMULA}
            {...fieldProps(value)}
            extra={(
              <><output class={'value-now' + (now[i] === null ? ' none' : '')} data-name={name}
                aria-label={`${name} now`} title={now[i] === null ? 'No run yet' : `Now: ${now[i]}`}>
                {now[i] === null ? '–' : inspectNumber(now[i]!)}
              </output>
              <output class="value-rate" aria-label={`${name} sampled tail rate`}
                title={`Maximum |Δ${name}/Δt| over up to the last ${STATE_TAIL_INTERVALS} stored intervals; sampled float32 trajectory data, not a solver derivative.`}>
                {rate === null ? '–' : rate.toExponential(2)}
              </output></>
            )} />
        );})}
      </div>
      <p class="state-rate-hint">Tail rate: max |Δstate/Δt| in the last {STATE_TAIL_INTERVALS} stored intervals. Small rates suggest settling; extend the run to check.</p>
    </Section>
  );
}

function IndexedSection({id, title, kind, entries, hint, startFolded}: {
  id: string; title: string; kind: 'bc' | 'delay'; entries: [string, string][]; hint: string;
  startFolded?: boolean;
}) {
  if (!entries.length) return null;
  return (
    <Section id={id} title={title} startFolded={startFolded}>
      <div class="value-list">
        {entries.map(([, value], i) => (
          <ValueField key={i} kind={kind} label={`${title} ${i + 1}`} index={i} hint={hint} spec={EXPRESSION}
            {...fieldProps(value)} />
        ))}
      </div>
    </Section>
  );
}

/** a numerics field (W106): a number box, or the method's choices; the
    core checks the value and its error shows on the field */
function NumericField({f}: {f: NumericsField}) {
  const session = useSession();
  const field = fieldKey('num', f.key);
  const error = useStore(s => s.values.errors[field]);
  const sent = useStore(s => sentText(s.values.inflight, field));
  const id = `value-${field}`.replace(/[^\w-]/g, '_');
  const title = f.unused ? `${f.label}: not used by this method` : f.label;
  if (f.choices) {
    const value = sent ?? String(f.value ?? 0);
    return (
      <div class={'value-field value-num' + (f.unused ? ' unused' : '')} data-num={f.key}>
        <label htmlFor={id} class="value-name" title={title}>{f.label}</label>
        <select id={id} value={value} title={title} aria-invalid={error ? true : undefined}
          onChange={e => session.setNumeric(f.key, (e.target as HTMLSelectElement).value)}>
          {f.choices.map((c, i) => <option key={c} value={String(i)}>{c}</option>)}
        </select>
        {error && <span class="field-error" role="alert">{error}</span>}
      </div>
    );
  }
  const shown = f.value === null ? '' : String(f.value);
  const spec: FieldSpec = f.integer ? {kind: 'integer'} : NUMBER;
  return (
    <div class={'value-field value-num' + (f.unused ? ' unused' : '')} data-num={f.key}>
      <label htmlFor={id} class="value-name" title={title}>{f.label}</label>
      <Field id={id} spec={spec} value={sent ?? (f.value === null ? '' : sixSig(f.value))} editValue={sent ?? shown}
        onCommit={text => session.setNumeric(f.key, text)} error={error ?? null} settling={sent !== null}
        onDropError={() => session.store.dispatch({type: 'values', action: {type: 'clearError', field}})}
        title={title} />
    </div>
  );
}

/** the main numerics (W106, docs/protocol.md "The numerics as data"): the
    Numerics menu's values, editable during a run like the parameters */
function NumericsSection() {
  const fields = useStore(s => s.numerics);
  if (!fields?.length) return null;
  const method = fields.find(f => f.key === 'method');
  const methodName = method?.choices?.[Number(method.value)] ?? 'the selected solver';
  const unused = fields.filter(f => f.unused);
  return (
    <Section id="num" title="Numerics" startFolded hint="Changes apply to the next run: a run in progress keeps its own.">
      <p class="value-hint">Solver: <strong>{methodName}</strong>. Unused solver controls are separated below.</p>
      <div class="value-list">
        {fields.filter(f => !f.unused).map(f => <NumericField key={f.key} f={f} />)}
      </div>
      {unused.length > 0 && <details class="unused-settings">
        <summary>Settings not used by {methodName}</summary>
        <p class="value-hint">These values are kept for other solvers. Editing them does not change {methodName}'s results.</p>
        <div class="value-list">{unused.map(f => <NumericField key={f.key} f={f} />)}</div>
      </details>}
    </Section>
  );
}

function UserButtonsBlock() {
  const session = useSession();
  const busy = !useMay()({cmd: 'userbut'});
  const names = useStore(s => s.hello?.userbuttons ?? []);
  if (!names.length) return null;
  return (
    <section class="value-group" aria-label="Buttons">
      <div class="value-list value-buttons">
        {names.map((name, i) => (
          <button key={i} onClick={() => session.userButton(i)} disabled={busy}
            title={busy ? BUSY_TITLE : 'A button defined in the ODE file'}>{name}</button>
        ))}
      </div>
    </section>
  );
}

/* ---- the panel: a column, a section, or a sheet, depending on the width (R6) ---- */

export function ValuesPanel() {
  const session = useSession();
  const checkpoint = useStore(s => s.values.checkpoint);
  const recoveryOff = useStore(s => !s.core || s.busy || !!s.ask || s.values.inflight.length > 0 || Object.keys(s.values.errors).length > 0);
  const open = useStore(s => s.valuesOpen);
  const bcs = useStore(s => s.core?.bcs ?? []);
  const modelBcs = showsBcSection(bcs) ? bcs : [];
  const delays = useStore(s => s.core?.delays ?? []);
  const panel = useRef<HTMLElement>(null);
  const close = () => session.store.dispatch({type: 'valuesPanel', open: false});

  useFocusBackOnClose(open, panel, '.values-toggle');
  useEffect(() => {
    if (!open) return;
    panel.current?.querySelector<HTMLElement>(FOCUSABLE)?.focus();
    /* Escape closes the sheet wherever the focus is inside it (narrow only: CSS keeps it open
       elsewhere), before the page's hotkeys see it -- except in a box with an edit to drop
       (Field's data-own-escape, UX-001): there Escape drops the edit, and a second one closes. */
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== 'Escape' || session.store.getState().ask) return;
      if ((e.target as Element | null)?.closest?.('[data-own-escape]')) return;
      e.preventDefault();
      e.stopPropagation();
      close();
    };
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, [open]);

  return (
    <section id="values-panel" ref={panel} class={'values-panel' + (open ? ' open' : '')} aria-label="Values">
      <div class="values-header">
        <button class="values-back" onClick={close}>Back</button>
        <h2>Values</h2>
      </div>
      <div class="values-body">
        <StateSection />
        <Parameters />
        <UserButtonsBlock />
        <IndexedSection id="bc" title="Boundary conditions" kind="bc" entries={modelBcs}
          hint="An expression that is zero at the boundary" startFolded />
        <IndexedSection id="delay" title="Delay initial data" kind="delay" entries={delays}
          hint="An expression in t for t < 0" startFolded />
        <NumericsSection />
        <details class="working-values">
          <summary>Recovery</summary>
          <p>Keep parameters and initial conditions before experimenting. Reset restores model defaults.</p>
          <div class="dialog-actions">
            <button class="small" disabled={recoveryOff} onClick={() => session.captureWorkingValues()}>{checkpoint ? 'Update checkpoint' : 'Keep working values'}</button>
            <button class="small" disabled={recoveryOff || !checkpoint} onClick={() => session.restoreWorkingValues()}>Restore working values</button>
          </div>
          {checkpoint && <p role="status">Working values kept for this model.</p>}
        </details>
      </div>
    </section>
  );
}
