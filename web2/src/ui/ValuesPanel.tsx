/* The values panel (docs/ui-v2.md T3, GitHub #18): parameters, the state
   (each variable's initial condition beside where the last run is now),
   boundary conditions, delay initial data, and the model's user buttons.
   A right column from 80rem, a section under the plot from 48rem, a
   full-screen sheet with a Back button below that (R6). The sliders are
   under the plot (SliderStrip.tsx).

   An edit sends `set` (and a run, with "Run on change"); while a command
   runs it waits, the latest per field, marked on the field, and goes out
   in one `set` when it ends (session.ts submit). Ctrl+Z (while the focus is
   inside) or the Undo button restores the previous value (A12). A rejected
   value comes back as a `message` `error`, shown on its field (A11); a text
   the field does not take (Field.tsx: a number or %formula, an expression
   for BCs and delays) is marked and never sent (T31). Each
   parameter and IC has a reset to the model file's value (its title names
   it) and is marked when it differs; each section folds (remembered per
   viewer), and Parameters and State save and load XPP's own files. */
import {useEffect, useRef, useState} from 'preact/hooks';
import type {ComponentChildren} from 'preact';
import {HELP} from '../help/links';
import type {Session} from '../session';
import {EXPRESSION, FORMULA, FORMULA_HINT, fieldMessage, type FieldSpec} from '../store/fieldKinds';
import {fieldKey, isQueued, sixSig, type ValueKind} from '../store/values';
import {useSession, useStore} from './context';
import {Field} from './Field';
import {HelpButton} from './HelpButton';

const NUMBER_HINT = fieldMessage(FORMULA_HINT);
const FOCUSABLE = 'button:not([disabled]), input, select, [tabindex]:not([tabindex="-1"])';
const STATE_HINT = 'Go runs from Initial; Last copies Now into Initial, then runs.';

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

function useFolded(id: string): [boolean, () => void] {
  const [folded, setFolded] = useState(() => readFolded().includes(id));
  const toggle = () => {
    const next = !folded;
    setFolded(next);
    try {
      const all = readFolded().filter(x => x !== id);
      localStorage.setItem(FOLD_KEY, JSON.stringify(next ? [...all, id] : all));
    } catch {
      /* no storage: it lasts this page */
    }
  };
  return [folded, toggle];
}

function Section({id, title, hint, tools, children}: {
  id: string; title: string; hint?: string; tools?: ComponentChildren; children: ComponentChildren;
}) {
  const [folded, toggle] = useFolded(id);
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
  const queued = useStore(s => isQueued(s.values.queue, field));
  const def = useStore(s => (kind === 'par' || kind === 'ic' ? s.values.defaults?.[field] ?? null : null));
  const id = `value-${field}`.replace(/[^\w-]/g, '_');
  /* Field sends only a text it takes that differs from where this focus
     started: a value that moved under a focused, untouched box (Undo, a
     slider on the same name) is not sent back as if it were an edit */
  const commit = (text: string) => {
    if (index !== undefined) session.setValueByIndex(kind as 'bc' | 'delay', index, text, full);
    else session.setValue(kind as 'par' | 'ic', name!, text, full);
  };
  const changed = def !== null && Number(full) !== def;
  const title = queued ? 'Sent when the running command ends' : def !== null ? `${hint}; default: ${sixSig(def)}` : hint;
  return (
    <div class={'value-field' + (queued ? ' queued' : '') + (changed ? ' changed' : '')}>
      <label htmlFor={id} class="value-name" title={label}>{label}</label>
      <Field id={id} spec={spec} value={display} editValue={full} onCommit={commit} error={error ?? null}
        title={title} data-queued={queued ? '1' : undefined} />
      {extra}
      {def !== null && name !== undefined && (
        <button class="value-reset icon" title={`default: ${sixSig(def)}`} disabled={!changed}
          aria-label={`Reset ${label} to its default, ${sixSig(def)}`}
          onClick={() => session.resetValue(kind as 'par' | 'ic', name, full)}>↺</button>
      )}
    </div>
  );
}

function fieldProps(value: string | number) {
  return {
    display: typeof value === 'number' ? sixSig(value) : value,
    full: typeof value === 'number' ? String(value) : value,
  };
}

/** Save and Load of a section's values (store/valueFiles.ts) */
function FileTools({kind}: {kind: 'par' | 'ic'}) {
  const session = useSession();
  const input = useRef<HTMLInputElement>(null);
  const what = kind === 'par' ? 'parameters' : 'initial conditions';
  return (
    <>
      <button class="small" onClick={() => session.saveValues(kind)}
        title={`Save the ${what} as a file XPP reads (${kind === 'par' ? 'File/Read par' : 'Initialconds/File, -icfile'})`}>
        Save
      </button>
      <button class="small" onClick={() => input.current?.click()}
        title={`Load ${what} from a saved file (XPP's, or "name value" lines)`}>Load</button>
      <input ref={input} id={`values-load-${kind}`} type="file" hidden
        onChange={async e => {
          const el = e.target as HTMLInputElement, file = el.files?.[0];
          if (file) session.loadValues(kind, await file.text());
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
    <Section id="par" title="Parameters" tools={(
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

/** where each variable is now: the active window's last stored row while a
    run goes (the column when its series has it), else `state.now` (where
    the last run ended or stopped); null before any run */
function useNow(): (number | null)[] {
  const ics = useStore(s => s.core?.ics);
  const now = useStore(s => s.core?.now);
  const busy = useStore(s => s.busy);
  const series = useStore(s => (s.busy ? s.plots.windows.find(w => w.win === s.plots.active)?.series ?? null : null));
  return (ics ?? []).map(([name], i) => {
    const col = busy && series && series.rows > 0 ? series.columns.get(i + 1) : undefined;
    if (col && series!.names.get(i + 1)?.toLowerCase() === name.toLowerCase()) return col[series!.rows - 1];
    return now?.[i] ?? null;
  });
}

function StateSection() {
  const session = useSession();
  const ics = useStore(s => s.core?.ics);
  const hasNow = useStore(s => !!s.core?.now);
  const busy = useStore(s => s.busy);
  const now = useNow();
  if (!ics?.length) return null;
  return (
    <Section id="ic" title="State" hint={STATE_HINT} tools={(
      <>
        <button class="small" disabled={busy || !hasNow} onClick={() => session.useCurrentState()}
          title="Copy Now into Initial (as Initialconds/Last does), without running">← Use current state</button>
        <FileTools kind="ic" />
        <button class="small" onClick={() => session.defaultValues('ic')} title="Every initial condition from the ODE file">
          Reset all
        </button>
      </>
    )}>
      <div class="value-cols" aria-hidden="true"><span /><span>Initial</span><span>Now</span></div>
      <div class="value-list value-state">
        {ics.map(([name, value], i) => (
          <ValueField key={name.toLowerCase()} kind="ic" label={name} name={name} hint={NUMBER_HINT} spec={FORMULA}
            {...fieldProps(value)}
            extra={(
              <output class={'value-now' + (now[i] === null ? ' none' : '')} data-name={name}
                aria-label={`${name} now`} title={now[i] === null ? 'No run yet' : `Now: ${now[i]}`}>
                {now[i] === null ? '–' : sixSig(now[i]!)}
              </output>
            )} />
        ))}
      </div>
    </Section>
  );
}

function IndexedSection({id, title, kind, entries, hint}: {
  id: string; title: string; kind: 'bc' | 'delay'; entries: [string, string][]; hint: string;
}) {
  if (!entries.length) return null;
  return (
    <Section id={id} title={title}>
      <div class="value-list">
        {entries.map(([, value], i) => (
          <ValueField key={i} kind={kind} label={`${title} ${i + 1}`} index={i} hint={hint} spec={EXPRESSION}
            {...fieldProps(value)} />
        ))}
      </div>
    </Section>
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

function RunOnChange({session}: {session: Session}) {
  const on = useStore(s => s.values.runOnChange);
  return (
    <label class="value-runs" title="Integrate again after a parameter or initial condition changes (like a slider)">
      <input type="checkbox" checked={on} onChange={e => session.setRunOnChange((e.target as HTMLInputElement).checked)} />
      Run on change
    </label>
  );
}

/* ---- the panel: a column, a section, or a sheet, depending on the width (R6) ---- */

export function ValuesPanel() {
  const session = useSession();
  const open = useStore(s => s.valuesOpen);
  const history = useStore(s => s.values.history);
  const queued = useStore(s => s.values.queue.length);
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
        <RunOnChange session={session} />
        <button class="small" disabled={!history.length} title={undoLabel} onClick={() => session.undoValue()}>
          Undo
        </button>
      </div>
      {queued > 0 && (
        <p class="values-queued" role="status">
          {queued === 1 ? '1 change waits' : `${queued} changes wait`} for the running command to end.
        </p>
      )}
      <div class="values-body">
        <UserButtonsBlock />
        <Parameters />
        <StateSection />
        <IndexedSection id="bc" title="Boundary conditions" kind="bc" entries={bcs}
          hint="An expression that is zero at the boundary" />
        <IndexedSection id="delay" title="Delay initial data" kind="delay" entries={delays}
          hint="An expression in t for t < 0" />
      </div>
    </section>
  );
}
