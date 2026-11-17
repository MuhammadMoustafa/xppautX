/* The Settings window's Pins list (docs/studies/w775-settings-mockup.html, W775): each pin a name and its steps, each step a command in
   W772's written form with its own check under it (runs, writes a file, the file exists, cannot run now), added by search or as text and
   checked by the core as it is typed (keymap.check_step); a bad step is shown under it and never saved. One kind of pin: a pin of one
   step is the command a toolbar button runs. Every change is the `keymap` command's `set` (session.setKeymap), and Run goes through the
   batch runner Export several uses (session.runPin). */
import {useEffect, useState} from 'preact/hooks';
import type {Pin} from '../protocol/types';
import {planStep, stepRun, stepStatus} from '../pins';
import {exportRows} from '../exportSeveral';
import {freshName, movePinned, moveStep, pinKeys, replacePin, withPinned, withRenamedPin} from '../store/keymap';
import {bindableRows, registryRows} from './navigation';
import {editorRows} from '../store/keymap';
import {useSession, useStore} from './context';

type Check = {run: Record<string, string | boolean>} | {error: string} | null;

export function PinsPane() {
  const session = useSession();
  const info = useStore(s => s.keymap.info)!;
  const hello = useStore(s => s.hello);
  const [creating, setCreating] = useState(false);
  /* a step's check says whether its file exists: the folder as it is now */
  useEffect(() => { void session.refreshListing(); }, [session]);
  const rows = bindableRows(hello, info);
  const save = (pinned: Pin[]) => session.setKeymap(withPinned(info, pinned));
  return <>
    <ol class="pin-list" data-pins>
      {info.pinned.map((pin, i) => <PinCard key={pin.name} pin={pin} index={i} save={save} keys={pinKeys(info, rows, pin)} />)}
      {info.pinned.length === 0 && !creating && <li class="pin-none">Nothing is pinned: use Pin in the list below, or New pin.</li>}
    </ol>
    {creating
      ? <NewPin close={() => setCreating(false)} save={save} />
      : <button type="button" data-pin-new onClick={() => setCreating(true)}>+ New pin</button>}
    {/* the file rows depend on hello only through the cards */}
  </>;
}

function PinCard({pin, index, save, keys}: {pin: Pin; index: number; save(pinned: Pin[]): void; keys: string[]}) {
  const session = useSession();
  const info = useStore(s => s.keymap.info)!;
  const pinned = info.pinned;
  const change = (steps: string[]) => save(replacePin(pinned, pin.name, {...pin, steps}));
  return <li class="pin-card" data-pinned={pin.name}>
    <div class="pin-head">
      <span class="input-box pin-name"><input type="text" aria-label="Pin name" data-pin-name={pin.name} defaultValue={pin.name}
        onChange={e => { const name = e.currentTarget.value; if (name !== pin.name) session.setKeymap(withRenamedPin(info, pin.name, {...pin, name})); }} /></span>
      {keys.map(key => <kbd key={key}>{key}</kbd>)}
      <button type="button" data-pin-run={pin.name} onClick={() => void session.runPin(pin.name)}>Run</button>
      <button type="button" aria-label={`Move ${pin.name} up`} disabled={index === 0} data-pin-up={pin.name}
        onClick={() => save(movePinned(pinned, pin.name, index - 1))}>↑</button>
      <button type="button" aria-label={`Move ${pin.name} down`} disabled={index === pinned.length - 1} data-pin-down={pin.name}
        onClick={() => save(movePinned(pinned, pin.name, index + 1))}>↓</button>
      <button type="button" aria-label={`Unpin ${pin.name}`} data-pin-remove={pin.name} onClick={() => save(replacePin(pinned, pin.name, null))}>✕</button>
    </div>
    <ol class="pin-steps">
      {pin.steps.map((step, i) => <StepItem key={`${i}:${step}`} pin={pin} index={i} change={change} />)}
    </ol>
    <AddStep onAdd={step => change([...pin.steps, step])} />
  </li>;
}

function StepItem({pin, index, change}: {pin: Pin; index: number; change(steps: string[]): void}) {
  const session = useSession();
  const info = useStore(s => s.keymap.info)!;
  const hello = useStore(s => s.hello);
  const unavailable = useStore(s => s.core?.unavailable ?? []);
  const listing = useStore(s => s.files.listing);
  const text = pin.steps[index];
  const [draft, setDraft] = useState<{text: string; check: Check} | null>(null);
  const items = exportRows(registryRows(hello), hello?.menu_separator ?? '', hello?.output_names ?? {}, unavailable).flatMap(row => row.items);
  const plan = planStep(text, stepRun(info, hello, text), items);
  const status = stepStatus(plan, unavailable, !!plan.file && !!listing?.some(f => f.name === plan.file));
  const edit = (value: string) => {
    setDraft({text: value, check: null});
    void session.checkStep(value).then(check => setDraft(d => (d && d.text === value ? {text: value, check} : d)));
  };
  const commit = () => {
    if (draft && draft.check && 'run' in draft.check && draft.text !== text) change(pin.steps.map((s, i) => (i === index ? draft.text : s)));
    setDraft(null);
  };
  return <li class={'pin-step' + (draft && draft.check && 'error' in draft.check ? ' bad' : '')} data-pin-step={`${pin.name}:${index}`}>
    <span class="pin-n">{index + 1}</span>
    {draft
      ? <span class="input-box pin-code"><input type="text" data-step-input aria-label={`Step ${index + 1} of ${pin.name}`} value={draft.text} data-autofocus
        onInput={e => edit(e.currentTarget.value)}
        onKeyDown={e => { if (e.key === 'Enter') { e.preventDefault(); e.stopPropagation(); commit(); } else if (e.key === 'Escape') { e.preventDefault(); e.stopPropagation(); setDraft(null); } }} /></span>
      : <code class="pin-code" data-step-text>{text}</code>}
    {draft
      ? <button type="button" data-step-save disabled={!(draft.check && 'run' in draft.check)} onClick={commit}>Save</button>
      : <button type="button" data-step-edit aria-label={`Edit step ${index + 1} of ${pin.name}`} onClick={() => setDraft({text, check: null})}>Edit</button>}
    <button type="button" aria-label={`Move step ${index + 1} up`} disabled={index === 0} data-step-up onClick={() => change(moveStep(pin.steps, index, index - 1))}>↑</button>
    <button type="button" aria-label={`Move step ${index + 1} down`} disabled={index === pin.steps.length - 1} data-step-down onClick={() => change(moveStep(pin.steps, index, index + 1))}>↓</button>
    <button type="button" aria-label={`Remove step ${index + 1}`} data-step-remove disabled={pin.steps.length === 1} title={pin.steps.length === 1 ? 'A pin has at least one step: unpin it instead' : undefined}
      onClick={() => change(pin.steps.filter((_, i) => i !== index))}>✕</button>
    {draft && draft.check && 'error' in draft.check
      ? <div class="pin-status no" data-step-status="error">✕ {draft.check.error} · not saved</div>
      : <div class={`pin-status ${status.kind}`} data-step-status={status.kind}>{status.text}</div>}
  </li>;
}

/** "+ Add step…": search the commands (Search commands' rows, every item of a choice too) or write the step as text; the text is checked as
    it is typed and added only when the core reads it */
function AddStep({onAdd}: {onAdd(step: string): void}) {
  const session = useSession();
  const hello = useStore(s => s.hello);
  const info = useStore(s => s.keymap.info);
  const [open, setOpen] = useState(false);
  const [query, setQuery] = useState('');
  const [check, setCheck] = useState<Check>(null);
  if (!open) return <div class="pin-add"><button type="button" data-step-add onClick={() => setOpen(true)}>+ Add step…</button></div>;
  const found = query.trim() ? editorRows(info, bindableRows(hello, info), query).slice(0, 8) : [];
  const type = (value: string) => {
    setQuery(value);
    setCheck(null);
    if (value.trim()) void session.checkStep(value.trim()).then(result => setQuery(q => { if (q === value) setCheck(result); return q; }));
  };
  const add = (step: string) => { setOpen(false); setQuery(''); setCheck(null); onAdd(step); };
  return <div class="pin-add" data-step-adding>
    <span class="input-box"><input type="search" data-autofocus data-step-search aria-label="Search commands, or write a step" placeholder="Search commands, or write a step: plot.export --format pdf"
      value={query} onInput={e => type(e.currentTarget.value)} onKeyDown={e => { if (e.key === 'Escape') { e.preventDefault(); e.stopPropagation(); setOpen(false); } }} /></span>
    <button type="button" data-step-cancel onClick={() => { setOpen(false); setQuery(''); }}>Cancel</button>
    {check && 'run' in check && <button type="button" class="primary" data-step-add-text onClick={() => add(query.trim())}>Add “{query.trim()}”</button>}
    {check && 'error' in check && <div class="pin-status no" data-step-check-error>✕ {check.error}</div>}
    <ul class="pin-found">{found.map(row => <li key={row.binding}>
      <button type="button" data-step-pick={row.binding} onClick={() => add(row.binding)}>{row.name} <code>{row.binding}</code></button></li>)}</ul>
  </div>;
}

/** the card of a pin being made: its name and its first step, which makes it (a pin has at least one step) */
function NewPin({close, save}: {close(): void; save(pinned: Pin[]): void}) {
  const info = useStore(s => s.keymap.info)!;
  const [name, setName] = useState('');
  const made = (step: string) => { save([...info.pinned, {name: freshName(info.pinned, name.trim() || 'New pin'), steps: [step]}]); close(); };
  return <div class="pin-card" data-pin-creating>
    <div class="pin-head">
      <span class="input-box pin-name"><input type="text" aria-label="New pin's name" data-new-pin-name placeholder="Name" value={name} onInput={e => setName(e.currentTarget.value)} /></span>
      <button type="button" data-new-pin-cancel onClick={close}>Cancel</button>
    </div>
    <AddStep onAdd={made} />
  </div>;
}
