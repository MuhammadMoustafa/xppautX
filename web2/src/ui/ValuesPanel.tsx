/* The values panel (docs/ui-v2.md T3, GitHub #18, #155): parameters, the
   state (each variable's initial condition beside where the last run is
   now), boundary conditions, delay initial data, the numerics (W106), and
   the model's user buttons. A right column from 48rem, a full-screen sheet
   with a Back button below that (R6). The sliders are in Parameters (SliderStrip.tsx).

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
   per viewer), and Parameters, States and Numerics each have a sets row (SetsRow.tsx, W323). */
import {openPanel} from './hotkeys';
import {useEffect, useRef, useState} from 'preact/hooks';
import {useFocusBackOnClose} from './focusBack';
import type {ComponentChildren, JSX} from 'preact';
import {HELP} from '../help/links';
import {EXPRESSION, FORMULA, FORMULA_HINT, fieldMessage, type FieldSpec} from '../store/fieldKinds';
import {fieldKey, foldKey, inputText, isFolded, sentText, showsBcSection, sixSig, type ValueKind} from '../store/values';
import {useOpenOptions, useOptionRows} from './OptionsPanel';
import {panelSpec} from './navigation';
import type {OptionGroup} from '../protocol/types';

/** the command whose panel is the main numerics' group */
const NUM_COMMAND = 'run.configure';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {CommandLabel, useCommandLabel} from './CommandButton';
import {SetsRow} from './SetsRow';
import {SliderStrip} from './SliderStrip';
import {Field} from './Field';
import {ShownName, ShownNumber} from './CutText';
import {HelpButton} from './HelpButton';
import {FOCUSABLE} from './dialogFocus';
import {readPref, writePref} from '../store/prefs';

const NUMBER_HINT = fieldMessage(FORMULA_HINT);

/** the room of the changed mark " •" after a name that differs from its default (values.css), in characters */
const CHANGED_MARK_CHARS = 2;
/** a name/box/reset list's style (values.css `.value-fill`): its name column is as wide as its longest name, with
    the changed mark's room where its fields carry the mark (`marked`: Parameters; a numerics field never does), at
    most --value-name-max; every list of fields uses this one helper */
function fillStyle(names: string[], marked: boolean): JSX.CSSProperties {
  const chars = Math.max(0, ...names.map(n => n.length)) + (marked ? CHANGED_MARK_CHARS : 0);
  return {'--name-chars': String(chars)} as JSX.CSSProperties;
}
const STATE_HINT = 'Edits apply on Enter or leaving the field, to the next run. Run from initial uses Initial; Run from last state uses the last state and starts a new trajectory.';

/* ---- folded sections, remembered per viewer ---- */

const FOLD_KEY = 'xpp.values.folded';

const readFolded = (): string[] =>
  readPref(FOLD_KEY, v => (Array.isArray(v) ? v.filter((x): x is string => typeof x === 'string') : null), []);

/* a section that starts folded (startFolded) is remembered by what the person opened:
   its entry is "+id", the others' is "id" for folded */
function useFolded(id: string, startFolded = false): [boolean, () => void] {
  const key = foldKey(id, startFolded);
  const [folded, setFolded] = useState(() => isFolded(readFolded(), id, startFolded));
  const toggle = () => {
    const next = !folded;
    setFolded(next);
    const all = readFolded().filter(x => x !== key);
    writePref(FOLD_KEY, next !== startFolded ? [...all, key] : all);
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
      <label htmlFor={id} class="value-name"><ShownName name={label} /></label>
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
    display: typeof value === 'number' ? inputText(value) : value,
    full: typeof value === 'number' ? String(value) : value,
  };
}

/** Load of a section's XPPAUT .par/.ic, an import through the core (session.ts loadValues,
    docs/protocol.md "value.import_file", W66 review); nothing of ours writes these files (W321) */
function FileTools({kind}: {kind: 'par' | 'ic'}) {
  const session = useSession();
  const input = useRef<HTMLInputElement>(null);
  const what = kind === 'par' ? 'parameters' : 'initial conditions';
  /* a file read: a setting, not while a computation runs (W95) */
  const loadOff = !useMay()({cmd: 'value.import_file'});
  return (
    <>
      <button class="small" onClick={() => input.current?.click()} disabled={loadOff}
        title={loadOff ? BUSY_TITLE : `Import ${what} from a file XPPAUT wrote (.${kind})`}>Load</button>
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
  const pars = useStore(s => s.core?.pars);
  /* the sliders live with the parameters, Add slider among them (W639); a model without parameters keeps them alone */
  if (!pars?.length) return <SliderStrip />;
  return (
    <Section id="par" title="Parameters" hint="Edit for the next run; Enter or leave a field to apply." tools={(
      <>
        <FileTools kind="par" />
        <SetsRow scope="parameters" />
      </>
    )}>
      <div class="value-list value-fill" style={fillStyle(pars.map(([name]) => name), true)}>
        {pars.map(([name, value]) => (
          <ValueField key={name.toLowerCase()} kind="par" label={name} name={name} hint={NUMBER_HINT} spec={FORMULA}
            {...fieldProps(value)} />
        ))}
      </div>
      <SliderStrip />
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
  const ics = useStore(s => s.core?.ics);
  const now = useNow();
  const rates = useStore(s => s.core?.rates);
  const intervals = useStore(s => s.hello?.state_inspection.tail_intervals);
  if (!ics?.length) return null;
  return (
    <Section id="ic" title="States" hint={STATE_HINT} tools={(
      <>
        <FileTools kind="ic" />
        <SetsRow scope="initial" />
      </>
    )}>
      <div class="value-list value-state">
        <div class="value-cols" aria-hidden="true"><span /><span>Initial</span><span>Current</span><span>Tail rate</span><span /></div>
        {ics.map(([name, value], i) => {
          const rate = rates?.[i] ?? null;
          return (
          <ValueField key={name.toLowerCase()} kind="ic" label={name} name={name} hint={NUMBER_HINT} spec={FORMULA}
            {...fieldProps(value)}
            extra={(
              <><output class={'value-now' + (now[i] === null ? ' none' : '')} data-name={name}
                aria-label={`${name} now`} title={now[i] === null ? 'No run yet' : undefined}>
                {now[i] === null ? '–' : <ShownNumber value={now[i]!} />}
              </output>
              <output class="value-rate muted" aria-label={`${name} sampled tail rate`}
                title={`Maximum |Δ${name}/Δt| over up to the last ${intervals} stored intervals; sampled float32 trajectory data, not a solver derivative.`}>
                {rate === null ? '–' : <ShownNumber value={rate} />}
              </output></>
            )} />
        );})}
      </div>
      <p class="state-rate-hint">Tail rate: max |Δstate/Δt| in the last {intervals} stored intervals. Small rates suggest settling; extend the run to check.</p>
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

/** the main numerics (W106, docs/protocol.md "The numerics as data"): the integration group's rows, the same fields as Run > Settings, each change
    sent at once like a parameter's; edits during a run apply to the next run, never the one in progress. A row the method or model does not
    use is in the closed list below, a choice the model cannot have says why. */
function NumericsSection() {
  const group = useStore(s => s.hello?.options.find(g => g.group === panelSpec(s.hello?.commands.find(c => c.cmd === NUM_COMMAND)?.panel)?.group));
  const numerics = useStore(s => s.numerics);
  return group && numerics?.length ? <NumericsRows group={group} /> : null;
}

function NumericsRows({group}: {group: OptionGroup}) {
  /* the values are asked for once when the numerics arrive, and again only when what the numerics event says changed (an edit made elsewhere) */
  useOpenOptions(group.group, useStore(s => JSON.stringify(s.numerics)));
  const {fields, unusedFields, unused} = useOptionRows(group, null, true);
  return (
    <Section id="num" title="Numerics" startFolded hint="Changes apply to the next run: a run in progress keeps its own."
      tools={<SetsRow scope="integration" />}>
      <div class="value-options">{fields}</div>
      {unused > 0 && <details class="unused-settings">
        <summary>Settings not used by this method or model</summary>
        <p class="value-hint">These values are kept for other solvers. Editing them does not change this method's results.</p>
        <div class="value-options">{unusedFields}</div>
      </details>}
    </Section>
  );
}

function UserButtonsBlock() {
  const session = useSession();
  const busy = !useMay()({cmd: 'button.run'});
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
  const valuesLabel = useCommandLabel('values.open');
  const session = useSession();
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
    /* Escape closes the sheet through hotkeys.ts's one owner (openPanel); a box with an edit to drop
       keeps its Escape (Field's data-own-escape, UX-001) */
    return openPanel(close);
  }, [open]);

  return (
    <section id="values-panel" ref={panel} class={'values-panel' + (open ? ' open' : '')} aria-label={valuesLabel}>
      <div class="panel-header">
        <button class="values-back" onClick={close}>Back</button>
        <h2><CommandLabel command="values.open" /></h2>
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
      </div>
    </section>
  );
}
