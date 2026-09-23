/* The sliders, under the plot where the eye already is (GitHub #18): any
   number of them (the model's `@ s1=..` presets first, then "Add slider"),
   each a parameter or variable with a low end, the track, its value and a
   high end. Picking one sets the range around its value (store/sliders.ts
   defaultRange). A drag sends `slide` while the core is idle, and only the
   latest position while it is busy (session.ts slide, sent in one `set`
   when the command ends); the release always leaves its final value. On a
   phone the parts stack. */
import {useEffect, useMemo, useRef, useState} from 'preact/hooks';
import {defaultRange, fromPosition, RANGE_STEPS, sliderRange, toPosition, type SliderDef} from '../store/sliders';
import {fieldKey, sixSig} from '../store/values';
import {useSession, useStore} from './context';

function Slider({def, index}: {def: SliderDef; index: number}) {
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
  const set = (patch: Partial<Omit<SliderDef, 'id'>>) =>
    session.store.dispatch({type: 'values', action: {type: 'setSlider', id: def.id, patch}});
  /* the value the last `slide` sent, so the release does not send it twice */
  const sent = useRef<number | null>(null);
  const gesture = useRef<string | null>(null);
  const [draft, setDraft] = useState<string | null>(null);
  useEffect(() => { sent.current = null; }, [match]);
  const slideTo = (v: number) => {
    if (!match) return;
    const st = session.store.getState();
    /* the value it sent last, with nothing else waiting: no second run of it */
    if (v === sent.current && !waiting) return;
    if (!st.busy) sent.current = v;
    session.slide(kind, match, v);
  };
  const label = `Slider ${index + 1}`;
  const id = (part: string) => `slider-${part}-${def.id}`;
  return (
    <div class={'value-slider' + (waiting ? ' queued' : '')} data-slider={def.id}>
      <label class="visually-hidden" htmlFor={id('pick')}>{label}: parameter or variable</label>
      <select id={id('pick')} value={match} onChange={e => {
        const name = (e.target as HTMLSelectElement).value;
        const v = [...(pars ?? []), ...(ics ?? [])].find(([n]) => n === name)?.[1] ?? 0;
        const r = defaultRange(v);
        set({name, lo: String(r.lo), hi: String(r.hi)});
      }}>
        <option value="">Par/Var…</option>
        {names.map(n => <option key={n} value={n}>{n}</option>)}
      </select>
      <label class="visually-hidden" htmlFor={id('lo')}>{label}: low end</label>
      <input id={id('lo')} class="value-slider-lim" title="Low end" value={def.lo} inputMode="decimal"
        onInput={e => set({lo: (e.target as HTMLInputElement).value})} />
      <input type="range" id={id('range')} class="value-slider-range" min={0} max={RANGE_STEPS} step={1}
        value={value !== undefined && range ? toPosition(value, range) : RANGE_STEPS / 2} disabled={!match || !range}
        aria-label={match ? `${match} slider` : label} aria-valuetext={value !== undefined ? sixSig(value) : undefined}
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
      <label class="visually-hidden" htmlFor={id('val')}>{label}: value</label>
      <input id={id('val')} class="value-slider-val" title="The value: type one to set it" inputMode="decimal"
        disabled={!match} value={draft ?? (value !== undefined ? sixSig(value) : '')}
        onFocus={() => setDraft(core !== undefined ? String(core) : '')}
        onInput={e => setDraft((e.target as HTMLInputElement).value)}
        onKeyDown={e => {
          if (e.key === 'Enter') (e.target as HTMLInputElement).blur();
          else if (e.key === 'Escape') { e.stopPropagation(); setDraft(null); }
        }}
        onBlur={() => {
          const text = draft?.trim() ?? '';
          setDraft(null);
          if (match && text && Number.isFinite(Number(text)) && Number(text) !== core)
            session.setValue(kind, match, text, core !== undefined ? String(core) : '');
        }} />
      <label class="visually-hidden" htmlFor={id('hi')}>{label}: high end</label>
      <input id={id('hi')} class="value-slider-lim" title="High end" value={def.hi} inputMode="decimal"
        onInput={e => set({hi: (e.target as HTMLInputElement).value})} />
      <button class="value-slider-remove icon" aria-label={`Remove ${label}`} title="Remove this slider"
        onClick={() => session.store.dispatch({type: 'values', action: {type: 'removeSlider', id: def.id}})}>✕</button>
    </div>
  );
}

export function SliderStrip() {
  const session = useSession();
  const sliders = useStore(s => s.values.sliders);
  const ready = useStore(s => !!s.core);
  if (!ready) return null;
  return (
    <section class="slider-strip" aria-label="Sliders">
      {sliders.map((d, i) => <Slider key={d.id} def={d} index={i} />)}
      <button class="small slider-add" onClick={() => session.store.dispatch({type: 'values', action: {type: 'addSlider'}})}
        title="A slider for a parameter or variable">Add slider</button>
    </section>
  );
}
