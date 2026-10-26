/* AUTO's settings in the page's own forms (docs/ui-v2.md T22): Numerics,
   and Mark values, from the `autosettings`
   data (store/autoSettings.ts). They open at any time, during a run too:
   they are settings (W106), and what OK changes goes to the core at once;
   during a run the core applies it when the run ends, to the next one (a
   continuation already running keeps what it started with; the dashed
   fields are the ones not applied yet). The core checks every value
   again and says why it refuses one (a notification, and the dialog's
   error when it is open). Follows the app's dialog pattern (AskDialog.tsx,
   SliderDialog.tsx): modal, focus in, Tab cycles and selects a field's
   text, Escape cancels, the focus goes back where it was. T23: each Numerics
   field has a plain name with AUTO's short one, its help as a tooltip, and
   its message beside it; OK waits until every value is AUTO's. */
import type {ComponentChildren} from 'preact';
import {useRef, useState} from 'preact/hooks';
import {HELP} from '../help/links';
import {symbolHelp, symbolName} from '../plot/diagramModel';
import {
  allCodim, CODIM_KINDS, fieldOf, NUM_FIELDS, NUM_GROUPS, numError, numSpec, pairErrors, pendingFields, shownSettings, type AutoSettings, type AutoSettingsPatch, type NumKey,
} from '../store/autoSettings';
import {fieldError, NUMBER} from '../store/fieldKinds';
import {useSession, useStore} from './context';
import {Field} from './Field';
import {HelpButton} from './HelpButton';
import {FOCUSABLE, useDialogFocus} from './dialogFocus';

/** the manual section each of this file's dialogs is (docs/manual/README.md's map, W12b) */
const HELP_OF = {numerics: HELP.autoNumerics, marks: HELP.autoMarks} as const;


/** the Threads choice's help: a preference of the program, never a result */
const THREADS_HELP = 'How many threads AUTO uses to compute a periodic orbit. Automatic is the physical cores of the computer, '
  + 'at most 4 (its performance cores on a hybrid processor). The results are the same for any count. '
  + 'This is a preference of the program, kept for every model and every run.';

export type AutoSettingsDialogKind = 'numerics' | 'marks';

/** the settings the forms show (the core's, with the edits not yet applied) */
function useShown(): AutoSettings | null {
  const st = useStore(s => s.autoSettings);
  return shownSettings(st);
}

function Modal({title, id, kind, onClose, children}: {
  title: string; id: string; kind: AutoSettingsDialogKind; onClose: () => void; children: ComponentChildren;
}) {
  const box = useRef<HTMLDivElement>(null);
  useDialogFocus(box, []);
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
    <div class="dialog-backdrop" onPointerDown={e => { if (e.target === e.currentTarget) onClose(); }}>
      <div class="dialog auto-settings-dialog" ref={box} role="dialog" aria-modal="true" aria-labelledby={`${id}-title`}
        data-settings={id} onKeyDown={onKeyDown}>
        <div class="dialog-title-row">
          <h2 id={`${id}-title`}>{title}</h2>
          <HelpButton target={HELP_OF[kind]} label={title} />
          <button class="dialog-close" aria-label={`Close ${title}`} onClick={onClose}>×</button>
        </div>
        {children}
      </div>
    </div>
  );
}

/** what the dialog says about when its changes apply, and the core's last refusal */
function Status({pending}: {pending: boolean}) {
  const computing = useStore(s => s.computing);
  const error = useStore(s => s.autoSettings.error);
  return (
    <>
      {computing && (
        <p class="auto-settings-note muted" role="status">
          A run is in progress: changes apply to the next run{pending ? ' (the dashed ones are sent already)' : ''}.
        </p>
      )}
      {!computing && pending && <p class="auto-settings-note muted" role="status">Dashed fields are being applied.</p>}
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
  const [specialPoints, setSpecialPoints] = useState(() => shown?.special_points ?? allCodim(true));
  const [threads, setThreads] = useState<string>(() => String(shown?.threads ?? 'auto'));
  if (!shown) return null;
  const errors = Object.fromEntries(NUM_FIELDS.map(f => [f.key, numError(shown.rules[f.key], texts[f.key] ?? '')]));
  const anyError = NUM_FIELDS.some(f => errors[f.key]);
  const values = Object.fromEntries(NUM_FIELDS.map(f => [f.key, Number(texts[f.key])])) as Record<NumKey, number>;
  /* T23: the values that must agree (DSMIN <= |DS| <= DSMAX, the limits in order), each by its field */
  const pairMessages: Partial<Record<NumKey, string>> = anyError ? {} : pairErrors(shown, values);
  const pairs = Object.keys(pairMessages).length > 0;
  const ok = () => {
    if (anyError || pairs) return;
    const changed = NUM_FIELDS.filter(f => (f.key==='suppbp' ? texts.suppbp==='true' : values[f.key]) !== shown.numerics[f.key]);
    const patch: AutoSettingsPatch = {};
    if (changed.length) patch.numerics = Object.fromEntries(changed.map(f => [f.key, f.key==='suppbp' ? texts.suppbp==='true' : values[f.key]]));
    if (threads !== String(shown.threads)) patch.threads = threads === 'auto' ? 'auto' : Number(threads);
    if (CODIM_KINDS.some(k => specialPoints[k] !== shown.special_points[k])) patch.special_points = specialPoints;
    if (patch.numerics || patch.threads !== undefined || patch.special_points) session.autoSettings(patch);
    onClose();
  };
  const anyPending = NUM_FIELDS.some(f => pending.has(`numerics.${f.key}`)) || pending.has('threads')
    || CODIM_KINDS.some(k => pending.has(`special_points.${k}`));
  const threadsQueued = pending.has('threads');
  return (
    <Modal title="AUTO Numerics" id="auto-numerics" kind="numerics" onClose={onClose}>
      <form class="auto-num-groups" onKeyDown={enterSubmits(ok)} onSubmit={e => { e.preventDefault(); ok(); }}>
        {NUM_GROUPS.map(g => (
          <fieldset key={g.title} class="form-grid auto-num-group">
            <legend>{g.title}</legend>
            {g.keys.map((k, i) => {
              const f = fieldOf(k), queued = pending.has(`numerics.${k}`);
              return (
                <label key={k} class={queued ? 'queued' : undefined}
                  title={queued ? `${f.help} (Applied when the running command ends.)` : f.help}>
                  <span>{f.name}</span>
                  {k==='suppbp' ? <input type="checkbox" data-field={k} id={`auto-num-${k}`} checked={texts[k]==='true'}
                    onChange={e => { const enabled=e.currentTarget.checked; setTexts(x => ({...x,[k]:String(enabled)})); }} /> :
                  <Field spec={numSpec(shown.rules[k])} check={t => numError(shown.rules[k], t)} value={texts[k]} data-field={k}
                    id={`auto-num-${k}`} data-queued={queued ? '1' : undefined} error={pairMessages[k] ?? null}
                    data-autofocus={g === NUM_GROUPS[0] && i === 0 ? '' : undefined}
                    onInput={t => setTexts(x => ({...x, [k]: t}))} />}
                </label>
              );
            })}
          </fieldset>
        ))}
        <fieldset class="form-grid">
          <legend>Curve special points</legend>
          <label><span>Detect all</span><input type="checkbox" checked={CODIM_KINDS.every(k => specialPoints[k])}
            onChange={e => { const on=e.currentTarget.checked; setSpecialPoints(allCodim(on)); }} /></label>
          {CODIM_KINDS.map(k => <label key={k} title={symbolHelp(k)} class={pending.has(`special_points.${k}`) ? 'queued' : undefined}>
            <span>{k} {symbolName(k)}</span><input type="checkbox" data-field={k} checked={specialPoints[k]}
              onChange={e => { const enabled=e.currentTarget.checked; setSpecialPoints(v => ({...v, [k]: enabled})); }} />
          </label>)}
        </fieldset>
        <fieldset class="form-grid auto-computer-group">
          <legend>Computer</legend>
          <label class={threadsQueued ? 'queued' : undefined} title={THREADS_HELP}>
            <span>Threads</span>
            <select value={threads} data-field="threads" id="auto-num-threads" data-queued={threadsQueued ? '1' : undefined}
              onChange={e => setThreads((e.target as HTMLSelectElement).value)}>
              <option value="auto">{`Automatic (${shown.threads_auto})`}</option>
              {Array.from({length: shown.threads_max}, (_, i) => <option key={i} value={String(i + 1)}>{i + 1}</option>)}
            </select>
          </label>
        </fieldset>
      </form>
      <Status pending={anyPending} />
      <Actions onClose={onClose} ok={ok} disabled={anyError || pairs} />
    </Modal>
  );
}

export function AutoMarksDialog({onClose}: {onClose: () => void}) {
  const session = useSession();
  const shown = useShown();
  const pending = pendingFields(useStore(s => s.autoSettings)).has('marks');
  const modelNames = useStore(s => s.core?.pars.map(([name]) => name)) ?? [];
  const [rows, setRows] = useState<[string, string][]>(() => (shown?.marks ?? []).map(([n, v]) => [n, String(v)]));
  if (!shown) return null;
  const names = [...modelNames, 'T'];
  const bad = rows.map(([, v]) => fieldError(NUMBER, v) !== null);
  const ok = () => {
    if (bad.some(b => b)) return;
    const marks = rows.map(([n, v]): [string, number] => [n, Number(v)]);
    if (JSON.stringify(marks) !== JSON.stringify(shown.marks)) session.autoSettings({marks});
    onClose();
  };
  const set = (i: number, j: 0 | 1, v: string) => setRows(r => r.map((row, k) => (k === i ? (j ? [row[0], v] : [v, row[1]]) : row)));
  return (
    <Modal title="Mark values" id="auto-marks" kind="marks" onClose={onClose}>
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
            <Field spec={NUMBER} value={v} aria-label={`Mark ${i + 1}: value`} data-field={`mark${i + 1}-value`}
              onInput={t => set(i, 1, t)} />
            <button type="button" class="small" aria-label={`Remove mark ${i + 1}`}
              onClick={() => setRows(r => r.filter((_, k) => k !== i))}>Remove</button>
          </div>
        ))}
        <button type="button" class="auto-mark-add"
          data-autofocus={rows.length === 0 ? '' : undefined}
          onClick={() => setRows(r => [...r, [names[0] ?? 'T', '0']])}>Add a value</button>
      </form>
      <Status pending={pending} />
      <Actions onClose={onClose} ok={ok} disabled={bad.some(b => b)} />
    </Modal>
  );
}

export function AutoSettingsDialog({kind, onClose}: {kind: AutoSettingsDialogKind; onClose: () => void}) {
  return kind === 'numerics' ? <AutoNumericsDialog onClose={onClose} />
      : <AutoMarksDialog onClose={onClose} />;
}
