/* The Add/Edit slider dialog (T20): a searchable, keyboard-navigable list of
   parameters and variables (each with its current value), and Min, Max and
   Step number spinners. Picking a candidate fills Min/Max/Step with the
   T19 defaults (defaultRange, defaultStep) unless it is the slider's own
   current one (editing keeps what it already has). The action is Add slider (Save slider
   when editing); a field's error shows once it was left or the action pressed
   (validateSliderFields). The one dialog shell (Dialog.tsx); local state, not a core ask. */
import {useMemo, useState} from 'preact/hooks';
import {HELP} from '../help/links';
import {
  defaultRange, defaultStep, filterCandidates, validateSliderFields, type SliderCandidate, type SliderDef,
} from '../store/sliders';
import {NUMBER, TEXT, type FieldSpec} from '../store/fieldKinds';
import {sixSig} from '../store/values';
import {useSession, useStore} from './context';
import {Field} from './Field';
import {Dialog} from './Dialog';
import {Control, FormRow, useFeedback} from './FormFields';
import {useDark} from './theme';

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

  const feedback = useFeedback();
  const dark = useDark(useStore(s => s.theme));
  const pickError = !picked ? 'Pick a parameter or variable' : null;
  const problems = [pickError, errors.lo, errors.hi, errors.step].filter((m, i) => m && (i === 0 ? feedback.attempted : feedback.shows(['pick', 'lo', 'hi', 'step'][i]))).length;

  const submit = (e?: Event) => {
    e?.preventDefault();
    if (!valid || !picked) {
      feedback.attempt();
      return;
    }
    const def: Omit<SliderDef, 'id'> = {name: picked.name, lo, hi, step};
    session.setSlider(editing ? target.id : session.store.getState().values.nextSlider, def);
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

  const title = editing ? `Edit slider ${target.name}` : 'Add slider';
  return (
    <Dialog title={title} name={title} help={HELP.valuesPanel} onClose={onClose} onSubmit={submit} problems={problems}
      action={{label: editing ? 'Save slider' : 'Add slider'}} width="list" attrs={{'data-slider-dialog': ''}}>
      <label class="slider-picker-search">
        <span class="visually-hidden">Search parameters and variables</span>
        <Field spec={TEXT} placeholder="Search parameters and variables" value={query} data-autofocus="" check={() => null}
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
      {picked ? <p class="slider-picker-picked">Picked: <b>{picked.name}</b> (currently {sixSig(picked.value)})</p>
        : feedback.attempted && <p class="field-error">{pickError}</p>}
      <div class="slider-dialog-fields">
        <FormRow name="lo" label="Min" required id="slider-lo" error={feedback.shows('lo') ? errors.lo : null}>
          <Control id="slider-lo" spec={NUMBER} value={lo} label="Min" dark={dark} onBlur={() => feedback.touch('lo')} onChange={setLo} /></FormRow>
        <FormRow name="hi" label="Max" required id="slider-hi" error={feedback.shows('hi') ? errors.hi : null}>
          <Control id="slider-hi" spec={NUMBER} value={hi} label="Max" dark={dark} onBlur={() => feedback.touch('hi')} onChange={setHi} /></FormRow>
        <FormRow name="step" label="Step" required id="slider-step" error={feedback.shows('step') ? errors.step : null}>
          <Control id="slider-step" spec={STEP} value={step} label="Step" dark={dark} onBlur={() => feedback.touch('step')} onChange={setStep} /></FormRow>
      </div>
    </Dialog>
  );
}
