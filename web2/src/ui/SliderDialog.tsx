/* The Add/Edit slider dialog (T20): a searchable, keyboard-navigable list of
   parameters and variables (each with its current value), and Min, Max and
   Step number spinners. Picking a candidate fills Min/Max/Step with the
   T19 defaults (defaultRange, defaultStep) unless it is the slider's own
   current one (editing keeps what it already has). OK is disabled until a
   candidate is picked and the fields validate (validateSliderFields);
   errors show inline. Follows the app's dialog pattern (AskDialog.tsx,
   theme.css .dialog) but is local state, not a core ask. */
import {useMemo, useRef, useState} from 'preact/hooks';
import {HELP} from '../help/links';
import {
  defaultRange, defaultStep, filterCandidates, validateSliderFields, type SliderCandidate, type SliderDef,
} from '../store/sliders';
import {NUMBER, TEXT, type FieldSpec} from '../store/fieldKinds';
import {sixSig} from '../store/values';
import {useSession, useStore} from './context';
import {Field} from './Field';
import {HelpButton} from './HelpButton';
import {FOCUSABLE, useDialogFocus} from './dialogFocus';

const STEP: FieldSpec = {kind: 'number', positive: true};


export interface SliderDialogProps {
  /** an existing slider to edit, or 'new' to add one */
  target: SliderDef | 'new';
  onClose: () => void;
}

function candidateKey(c: Pick<SliderCandidate, 'kind' | 'name'>): string {
  return `${c.kind}:${c.name.toLowerCase()}`;
}

export function SliderDialog({target, onClose}: SliderDialogProps) {
  const session = useSession();
  const pars = useStore(s => s.core?.pars) ?? [];
  const ics = useStore(s => s.core?.ics) ?? [];
  const editing = target !== 'new';
  const candidates = useMemo<SliderCandidate[]>(() => [
    ...pars.map(([name, value]): SliderCandidate => ({kind: 'par', name, value})),
    ...ics.map(([name, value]): SliderCandidate => ({kind: 'ic', name, value})),
  ], [pars, ics]);

  const initial = editing ? candidates.find(c => c.name.toLowerCase() === target.name.toLowerCase()) ?? null : null;
  const [query, setQuery] = useState('');
  const [picked, setPicked] = useState<SliderCandidate | null>(initial);
  const [lo, setLo] = useState(editing ? target.lo : '');
  const [hi, setHi] = useState(editing ? target.hi : '');
  const [step, setStep] = useState(editing ? target.step : '');
  const [active, setActive] = useState(0);

  const filtered = useMemo(() => filterCandidates(candidates, query), [candidates, query]);

  const pick = (c: SliderCandidate) => {
    const same = picked && candidateKey(picked) === candidateKey(c);
    setPicked(c);
    if (!same) {
      const r = defaultRange(c.value);
      setLo(String(r.lo));
      setHi(String(r.hi));
      setStep(String(defaultStep(r.lo, r.hi)));
    }
  };

  const errors = validateSliderFields(lo, hi, step);
  const valid = !!picked && !errors.lo && !errors.hi && !errors.step;

  const box = useRef<HTMLDivElement>(null);
  useDialogFocus(box, []);

  const submit = () => {
    if (!valid || !picked) return;
    const def: Omit<SliderDef, 'id'> = {name: picked.name, lo, hi, step};
    if (editing) session.store.dispatch({type: 'values', action: {type: 'setSlider', id: target.id, patch: def}});
    else session.store.dispatch({type: 'values', action: {type: 'addSliderWith', def}});
    onClose();
  };

  const onListKeyDown = (e: KeyboardEvent) => {
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      e.preventDefault();
      if (!filtered.length) return;
      const d = e.key === 'ArrowDown' ? 1 : -1;
      setActive(a => (a + d + filtered.length) % filtered.length);
    } else if (e.key === 'Enter') {
      e.preventDefault();
      if (filtered[active]) pick(filtered[active]);
    }
  };

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
      all[next].focus();
    }
  };

  const title = editing ? `Edit slider ${target.name}` : 'Add slider';
  return (
    <div class="dialog-backdrop">
      <div class="dialog" ref={box} role="dialog" aria-modal="true" aria-labelledby="slider-dialog-title" onKeyDown={onKeyDown}>
        <div class="dialog-title-row">
          <h2 id="slider-dialog-title">{title}</h2>
          <HelpButton target={HELP.valuesPanel} label={title} />
        </div>
        <label class="slider-picker-search">
          <span class="visually-hidden">Search parameters and variables</span>
          <Field spec={TEXT} placeholder="Search parameters and variables" value={query} data-autofocus=""
            onInput={t => { setQuery(t); setActive(0); }} onKeyDown={onListKeyDown} />
        </label>
        <ul class="slider-picker-list" role="listbox" aria-label="Parameters and variables" tabIndex={-1}
          onKeyDown={onListKeyDown}>
          {filtered.length === 0 && <li class="slider-picker-empty muted">No match</li>}
          {filtered.map((c, i) => {
            const key = candidateKey(c);
            const isPicked = picked && candidateKey(picked) === key;
            return (
              <li key={key} role="option" aria-selected={!!isPicked} class={'slider-picker-item' + (i === active ? ' active' : '') + (isPicked ? ' picked' : '')}
                onMouseEnter={() => setActive(i)} onClick={() => pick(c)}>
                <span class="slider-picker-name">{c.name}</span>
                <span class="slider-picker-value muted">{sixSig(c.value)}</span>
              </li>
            );
          })}
        </ul>
        {picked && <p class="slider-picker-picked">Picked: <b>{picked.name}</b> (currently {sixSig(picked.value)})</p>}
        <div class="form-grid slider-dialog-fields">
          <label>
            <span>Min</span>
            <Field id="slider-lo" spec={NUMBER} value={lo} error={errors.lo} onInput={setLo} />
          </label>
          <label>
            <span>Max</span>
            <Field id="slider-hi" spec={NUMBER} value={hi} error={errors.hi} onInput={setHi} />
          </label>
          <label>
            <span>Step</span>
            <Field id="slider-step" spec={STEP} value={step} error={errors.step} onInput={setStep} />
          </label>
        </div>
        <div class="dialog-actions">
          <button type="button" onClick={onClose}>Cancel</button>
          <button type="button" class="primary" disabled={!valid} onClick={submit}>OK</button>
        </div>
      </div>
    </div>
  );
}
