/* The sliders, under the plot where the eye already is (GitHub #18, T20):
   any number of them (the model's `@ s1=..` presets first, then "Add
   slider"), each compact: the variable's name, a range track with its low
   and high end shown as small muted labels, the current value as a
   box (Field.tsx: a number; ArrowUp/ArrowDown step by the slider's own step), a small edit
   icon (opens SliderDialog.tsx prefilled) and a remove button. A drag
   sends `slide` while the core is idle, and only the latest position
   while it is busy (session.ts slide, sent in one `set` when the command
   ends); the release always leaves its final value. Typing or spinning
   the value box does not send on every keystroke or step: it waits
   ~400ms after the last one, or commits at once on blur/Enter, so a fast
   run of edits sends once. They lay out as a responsive grid (theme.css
   .slider-strip): 3 per row at 1280px, 2 on tablets, 1 on phones. */
import {useEffect, useMemo, useRef, useState} from 'preact/hooks';
import {fromPosition, RANGE_STEPS, sliderRange, sliderStep, toPosition, type SliderDef} from '../store/sliders';
import {NUMBER} from '../store/fieldKinds';
import {fieldKey, sixSig} from '../store/values';
import {Field} from './Field';
import {SliderDialog} from './SliderDialog';
import {useSession, useStore} from './context';

/** typing or spinning the value box waits this long after the last change
    before it sends, so a run of keystrokes or spinner clicks sends once */
const VALUE_DEBOUNCE_MS = 400;

function Slider({def, index, onEdit}: {def: SliderDef; index: number; onEdit: () => void}) {
  const session = useSession();
  const pars = useStore(s => s.core?.pars);
  const ics = useStore(s => s.core?.ics);
  const queue = useStore(s => s.values.queue);
  const names = useMemo(() => [...(pars ?? []), ...(ics ?? [])].map(([n]) => n), [pars, ics]);
  const match = names.find(n => n.toLowerCase() === def.name.toLowerCase()) ?? '';
  const kind: 'par' | 'ic' = pars?.some(([n]) => n.toLowerCase() === match.toLowerCase()) ? 'par' : 'ic';
  const core = match ? [...(pars ?? []), ...(ics ?? [])].find(([n]) => n.toLowerCase() === match.toLowerCase())?.[1] : undefined;
  /* a position waiting for the running command shows as the value */
  const waiting = match ? queue.find(q => q.name !== undefined && fieldKey(q.kind, q.name) === fieldKey(kind, match)) : undefined;
  const value = waiting ? Number(waiting.text) : core;
  const range = sliderRange(def);
  const step = sliderStep(def);
  /* the value the last `slide` sent, so the release does not send it twice */
  const sent = useRef<number | null>(null);
  const gesture = useRef<string | null>(null);
  /* the value box: the value undo should go back to, updated after each commit */
  const focusValue = useRef<string | null>(null);
  useEffect(() => { sent.current = null; }, [match]);
  const slideTo = (v: number) => {
    if (!match) return;
    const st = session.store.getState();
    /* the value it sent last, with nothing else waiting: no second run of it */
    if (v === sent.current && !waiting) return;
    if (!st.busy) sent.current = v;
    session.slide(kind, match, v);
  };
  /* Field.tsx commits a number only, changed, after the pause, on Enter or on blur */
  const commitValue = (text: string) => {
    const v = Number(text);
    if (!match) return;
    const clamped = range ? Math.max(Math.min(range.lo, range.hi), Math.min(Math.max(range.lo, range.hi), v)) : v;
    const startedFrom = focusValue.current;
    /* unchanged from what this focus (or the last commit in it) started
       from: nothing to send. Comparing to the *live* value here would be
       wrong: if something else moves it while the box is still focused
       (Undo, another slider on the same name) a blur with nothing typed
       would resend this box's now-stale value as if it were a fresh edit */
    if (startedFrom !== null && clamped === Number(startedFrom)) return;
    slideTo(clamped);
    if (startedFrom !== null) session.recordEdit({kind, name: match, previous: startedFrom});
    focusValue.current = String(clamped);
  };
  const label = def.name || `Slider ${index + 1}`;
  const id = (part: string) => `slider-${part}-${def.id}`;
  return (
    <div class={'slider-card' + (waiting ? ' queued' : '')} data-slider={def.id}>
      <div class="slider-card-head">
        <span class="slider-card-name" title={label}>{label}</span>
        <label class="visually-hidden" htmlFor={id('val')}>{label}: value</label>
        <Field id={id('val')} class="slider-card-value" spec={NUMBER} disabled={!match}
          step={step ?? null} min={range ? Math.min(range.lo, range.hi) : undefined}
          max={range ? Math.max(range.lo, range.hi) : undefined}
          title="The value: type it, or step it with the arrow keys (sends after a short pause, or at once on Enter/blur)"
          value={value !== undefined ? sixSig(value) : ''} editValue={core !== undefined ? String(core) : ''}
          commitAfter={VALUE_DEBOUNCE_MS} onCommit={commitValue}
          onFocus={() => { focusValue.current = core !== undefined ? String(core) : null; }} />
        <button class="icon slider-card-edit" aria-label={`Edit slider ${label}`}
          title="Change the parameter or variable, min, max or step" onClick={onEdit}>&#9998;</button>
        <button class="icon slider-card-remove" aria-label={`Remove slider ${label}`} title="Remove this slider"
          onClick={() => session.store.dispatch({type: 'values', action: {type: 'removeSlider', id: def.id}})}>&#10005;</button>
      </div>
      <div class="slider-card-track">
        <span class="slider-card-lim muted">{range ? sixSig(Math.min(range.lo, range.hi)) : ''}</span>
        <label class="visually-hidden" htmlFor={id('range')}>{label}: drag to change the value</label>
        <input type="range" id={id('range')} class="slider-card-range" min={0} max={RANGE_STEPS} step={1}
          value={value !== undefined && range ? toPosition(value, range) : RANGE_STEPS / 2} disabled={!match || !range}
          aria-label={`${label} slider`} aria-valuetext={value !== undefined ? sixSig(value) : undefined}
          title="Drag, or use the arrow keys, to change the value and integrate again"
          onInput={e => {
            if (!match || !range) return;
            if (gesture.current === null) gesture.current = core !== undefined ? String(core) : null;
            slideTo(fromPosition(Number((e.target as HTMLInputElement).value), range));
          }}
          onChange={e => {
            if (match && range) slideTo(fromPosition(Number((e.target as HTMLInputElement).value), range));
            if (match && gesture.current !== null) session.recordEdit({kind, name: match, previous: gesture.current});
            gesture.current = null;
          }}
        />
        <span class="slider-card-lim muted">{range ? sixSig(Math.max(range.lo, range.hi)) : ''}</span>
      </div>
    </div>
  );
}

export function SliderStrip() {
  const sliders = useStore(s => s.values.sliders);
  const ready = useStore(s => !!s.core);
  const [dialog, setDialog] = useState<SliderDef | 'new' | null>(null);
  if (!ready) return null;
  return (
    <>
      <section class="slider-strip" aria-label="Sliders">
        {sliders.map((d, i) => <Slider key={d.id} def={d} index={i} onEdit={() => setDialog(d)} />)}
        <button class="small slider-add" onClick={() => setDialog('new')}
          title="Add a slider on a parameter or variable">Add slider</button>
      </section>
      {dialog !== null && <SliderDialog key={dialog === 'new' ? 'new' : dialog.id} target={dialog} onClose={() => setDialog(null)} />}
    </>
  );
}
