/* AUTO's settings in the page's own forms (docs/ui-v2.md T22): Numerics,
   AUTO's parameters (Parameter) and Mark values, from the `autosettings`
   data (store/autoSettings.ts). They open at any time, during a run too:
   what OK changes goes to the core at once when it is idle, and otherwise
   waits, marked pending, until the running command ends (a continuation
   already running keeps what it started with). The core checks every value
   again and says why it refuses one (a notification, and the dialog's
   error when it is open). Follows the app's dialog pattern (AskDialog.tsx,
   SliderDialog.tsx): modal, focus in, Tab cycles and selects a field's
   text, Escape cancels, the focus goes back where it was. T23: each Numerics
   field has a plain name with AUTO's short one, its help as a tooltip, and
   its message beside it; OK waits until every value is AUTO's. */
import type {ComponentChildren} from 'preact';
import {useEffect, useRef, useState} from 'preact/hooks';
import {
  fieldOf, NUM_FIELDS, NUM_GROUPS, numError, pairErrors, pendingFields, shownSettings, type AutoSettings, type NumKey,
} from '../store/autoSettings';
import {useSession, useStore} from './context';

const FOCUSABLE = 'button:not([disabled]), input, select, [tabindex]:not([tabindex="-1"])';

export type AutoSettingsDialogKind = 'numerics' | 'pars' | 'marks';

/** the settings the forms show (the core's, with the edits not yet applied) */
function useShown(): AutoSettings | null {
  const st = useStore(s => s.autoSettings);
  return shownSettings(st);
}

function Modal({title, id, onClose, children}: {title: string; id: string; onClose: () => void; children: ComponentChildren}) {
  const box = useRef<HTMLDivElement>(null);
  useEffect(() => {
    const before = document.activeElement as HTMLElement | null;
    const first = box.current?.querySelector<HTMLElement>('[data-autofocus]') ?? box.current?.querySelector<HTMLElement>(FOCUSABLE);
    first?.focus();
    if (first instanceof HTMLInputElement) first.select();
    return () => before?.focus?.();
  }, []);
  const onKeyDown = (e: KeyboardEvent) => {
    if (e.key === 'Escape') {
      e.preventDefault();
      e.stopPropagation();
      onClose();
    } else if (e.key === 'Tab') {
      const all = [...box.current!.querySelectorAll<HTMLElement>(FOCUSABLE)];
      if (!all.length) return;
      const i = all.indexOf(document.activeElement as HTMLElement);
      const next = e.shiftKey ? (i <= 0 ? all.length - 1 : i - 1) : (i === all.length - 1 ? 0 : i + 1);
      e.preventDefault();
      const to = all[next];
      to.focus();
      if (to instanceof HTMLInputElement) to.select(); /* typing then replaces the text, as a native Tab */
    } else {
      e.stopPropagation(); /* not AUTO's hotkeys, nor the page's */
    }
  };
  return (
    <div class="dialog-backdrop">
      <div class="dialog auto-settings-dialog" ref={box} role="dialog" aria-modal="true" aria-labelledby={`${id}-title`}
        data-settings={id} onKeyDown={onKeyDown}>
        <h2 id={`${id}-title`}>{title}</h2>
        {children}
      </div>
    </div>
  );
}

/** what the dialog says about when its changes apply, and the core's last refusal */
function Status({pending}: {pending: boolean}) {
  const busy = useStore(s => s.busy);
  const error = useStore(s => s.autoSettings.error);
  return (
    <>
      {busy && (
        <p class="auto-settings-note muted" role="status">
          AUTO is busy: changes apply when the current run ends{pending ? ' (the dashed ones wait already)' : ''}.
        </p>
      )}
      {!busy && pending && <p class="auto-settings-note muted" role="status">Dashed fields are being applied.</p>}
      {error && <p class="field-error" role="alert">{error}</p>}
    </>
  );
}

function Actions({onClose, ok, disabled}: {onClose: () => void; ok: () => void; disabled?: boolean}) {
  return (
    <div class="dialog-actions">
      <button type="button" onClick={onClose}>Cancel</button>
      <button type="submit" class="primary" disabled={disabled} onClick={e => { e.preventDefault(); ok(); }}>OK</button>
    </div>
  );
}

/** Enter in a field is OK */
const enterSubmits = (ok: () => void) => (e: KeyboardEvent) => {
  const tag = (e.target as HTMLElement).tagName;
  if (e.key === 'Enter' && (tag === 'INPUT' || tag === 'SELECT')) {
    e.preventDefault();
    ok();
  }
};

export function AutoNumericsDialog({onClose}: {onClose: () => void}) {
  const session = useSession();
  const shown = useShown();
  const pending = pendingFields(useStore(s => s.autoSettings));
  const [texts, setTexts] = useState<Record<string, string>>(() =>
    Object.fromEntries(NUM_FIELDS.map(f => [f.key, shown ? String(shown.numerics[f.key]) : ''])));
  if (!shown) return null;
  const errors = Object.fromEntries(NUM_FIELDS.map(f => [f.key, numError(f.key, texts[f.key] ?? '')]));
  const anyError = NUM_FIELDS.some(f => errors[f.key]);
  const values = Object.fromEntries(NUM_FIELDS.map(f => [f.key, Number(texts[f.key])])) as Record<NumKey, number>;
  /* T23: the values that must agree (DSMIN <= |DS| <= DSMAX, the limits in order), each by its field */
  const pairMessages: Partial<Record<NumKey, string>> = anyError ? {} : pairErrors(values);
  const pairs = Object.keys(pairMessages).length > 0;
  const ok = () => {
    if (anyError || pairs) return;
    const changed = NUM_FIELDS.filter(f => values[f.key] !== shown.numerics[f.key]);
    if (changed.length) session.autoSettings({numerics: Object.fromEntries(changed.map(f => [f.key, values[f.key]]))});
    onClose();
  };
  const anyPending = NUM_FIELDS.some(f => pending.has(`numerics.${f.key}`));
  return (
    <Modal title="AUTO Numerics" id="auto-numerics" onClose={onClose}>
      <form class="auto-num-groups" onKeyDown={enterSubmits(ok)} onSubmit={e => { e.preventDefault(); ok(); }}>
        {NUM_GROUPS.map(g => (
          <fieldset key={g.title} class="form-grid auto-num-group">
            <legend>{g.title}</legend>
            {g.keys.map((k, i) => {
              const f = fieldOf(k), err = errors[k] ?? pairMessages[k], queued = pending.has(`numerics.${k}`);
              return (
                <label key={k} class={queued ? 'queued' : undefined}
                  title={queued ? `${f.help} (Sent when the running command ends.)` : f.help}>
                  <span>{f.name}</span>
                  <input type="text" inputMode={f.integer ? 'numeric' : 'decimal'} value={texts[k]} data-field={k}
                    data-queued={queued ? '1' : undefined} aria-invalid={err ? 'true' : undefined}
                    aria-describedby={err ? `auto-num-${k}-err` : undefined}
                    data-autofocus={g === NUM_GROUPS[0] && i === 0 ? '' : undefined}
                    onInput={e => setTexts(t => ({...t, [k]: (e.target as HTMLInputElement).value}))} />
                  {err && <p class="field-error" id={`auto-num-${k}-err`}>{err}</p>}
                </label>
              );
            })}
          </fieldset>
        ))}
      </form>
      <Status pending={anyPending} />
      <Actions onClose={onClose} ok={ok} disabled={anyError || pairs} />
    </Modal>
  );
}

export function AutoParsDialog({onClose}: {onClose: () => void}) {
  const session = useSession();
  const shown = useShown();
  const pending = pendingFields(useStore(s => s.autoSettings)).has('pars');
  const model = useStore(s => s.core?.pars) ?? [];
  const [names, setNames] = useState<string[]>(() => (shown?.pars ?? []).map(n => n ?? ''));
  if (!shown) return null;
  const ok = () => {
    if (names.some((n, i) => n !== (shown.pars[i] ?? ''))) session.autoSettings({pars: names});
    onClose();
  };
  return (
    <Modal title="AUTO's parameters" id="auto-pars" onClose={onClose}>
      <p class="muted auto-settings-note">The parameters AUTO can continue in; the axes and Mark values name them.</p>
      <form class="form-grid auto-pars" onKeyDown={enterSubmits(ok)} onSubmit={e => { e.preventDefault(); ok(); }}>
        {names.map((n, i) => (
          <label key={i} class={pending ? 'queued' : undefined}>
            <span>Par{i + 1}</span>
            <select value={n} data-field={`par${i + 1}`} data-autofocus={i === 0 ? '' : undefined}
              onChange={e => { const v = names.slice(); v[i] = (e.target as HTMLSelectElement).value; setNames(v); }}>
              {!model.some(([m]) => m === n) && <option value={n}>{n || '(none)'}</option>}
              {model.map(([m]) => <option key={m} value={m}>{m}</option>)}
            </select>
          </label>
        ))}
      </form>
      <Status pending={pending} />
      <Actions onClose={onClose} ok={ok} />
    </Modal>
  );
}

const MARKS_MAX = 9;

export function AutoMarksDialog({onClose}: {onClose: () => void}) {
  const session = useSession();
  const shown = useShown();
  const pending = pendingFields(useStore(s => s.autoSettings)).has('marks');
  const [rows, setRows] = useState<[string, string][]>(() => (shown?.marks ?? []).map(([n, v]) => [n, String(v)]));
  if (!shown) return null;
  const names = [...shown.pars.filter((n): n is string => !!n), 'T'];
  const bad = rows.map(([, v]) => !v.trim() || !Number.isFinite(Number(v)));
  const ok = () => {
    if (bad.some(b => b)) return;
    const marks = rows.map(([n, v]): [string, number] => [n, Number(v)]);
    if (JSON.stringify(marks) !== JSON.stringify(shown.marks)) session.autoSettings({marks});
    onClose();
  };
  const set = (i: number, j: 0 | 1, v: string) => setRows(r => r.map((row, k) => (k === i ? (j ? [row[0], v] : [v, row[1]]) : row)));
  return (
    <Modal title="Mark values" id="auto-marks" onClose={onClose}>
      <p class="muted auto-settings-note">
        AUTO labels (UZ) the points where a parameter, or the period T, reaches one of these values.
      </p>
      <form class="auto-marks" onKeyDown={enterSubmits(ok)} onSubmit={e => { e.preventDefault(); ok(); }}>
        {rows.length === 0 && <p class="muted">No Mark values.</p>}
        {rows.map(([n, v], i) => (
          <div key={i} class={'auto-mark-row' + (pending ? ' queued' : '')}>
            <select value={n} aria-label={`Mark ${i + 1}: parameter`} data-field={`mark${i + 1}-name`}
              data-autofocus={i === 0 ? '' : undefined} onChange={e => set(i, 0, (e.target as HTMLSelectElement).value)}>
              {!names.includes(n) && <option value={n}>{n}</option>}
              {names.map(m => <option key={m} value={m}>{m === 'T' ? 'T (period)' : m}</option>)}
            </select>
            <span aria-hidden="true">=</span>
            <input type="text" inputMode="decimal" value={v} aria-label={`Mark ${i + 1}: value`} data-field={`mark${i + 1}-value`}
              aria-invalid={bad[i] ? 'true' : undefined} onInput={e => set(i, 1, (e.target as HTMLInputElement).value)} />
            <button type="button" class="small" aria-label={`Remove mark ${i + 1}`}
              onClick={() => setRows(r => r.filter((_, k) => k !== i))}>Remove</button>
          </div>
        ))}
        <button type="button" class="auto-mark-add" disabled={rows.length >= MARKS_MAX}
          data-autofocus={rows.length === 0 ? '' : undefined}
          onClick={() => setRows(r => [...r, [names[0] ?? 'T', '0']])}>Add a value</button>
      </form>
      {bad.some(b => b) && <p class="field-error" role="alert">Each value must be a number.</p>}
      <Status pending={pending} />
      <Actions onClose={onClose} ok={ok} disabled={bad.some(b => b)} />
    </Modal>
  );
}

export function AutoSettingsDialog({kind, onClose}: {kind: AutoSettingsDialogKind; onClose: () => void}) {
  return kind === 'numerics' ? <AutoNumericsDialog onClose={onClose} />
    : kind === 'pars' ? <AutoParsDialog onClose={onClose} />
      : <AutoMarksDialog onClose={onClose} />;
}
