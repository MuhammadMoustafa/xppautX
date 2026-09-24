/* The core's prompts (docs/protocol.md "Asks") as a modal dialog: focus
   moves into it, stays in it (Tab cycles), Escape cancels, and focus goes
   back where it was when it closes. Menus, yes/no choices, string boxes,
   forms (a field that picks from `hello.lists` is a select), checklists and
   files (FileDialog.tsx); alerts are notifications (session.ts); mouse,
   rubber and drag asks are plot modes (PlotView.tsx); other kinds say they
   are not offered yet and offer Cancel (A13, docs/ui-v2.md). */
import type {ComponentChildren} from 'preact';
import {useEffect, useRef, useState} from 'preact/hooks';
import {askHelp} from '../help/links';
import {fieldSpec, selectOptions} from '../protocol/lists';
import type {AskEvent} from '../protocol/types';
import {FileAsk} from './FileDialog';
import {HelpButton} from './HelpButton';
import {MENU_ONE_COLUMN, menuRows} from './menuLayout';
import {useSession, useStore} from './context';

const FOCUSABLE = 'button:not([disabled]), input, select, textarea, [tabindex]:not([tabindex="-1"])';

function MenuAsk({ask}: {ask: AskEvent}) {
  const session = useSession();
  const keys = ask.keys ?? '';
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.ctrlKey || e.metaKey || e.altKey || e.key.length !== 1) return;
      const i = keys.toLowerCase().indexOf(e.key.toLowerCase());
      if (i < 0) return;
      e.preventDefault();
      e.stopPropagation();
      session.answer(ask, {key: keys[i]});
    };
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, [ask]);
  const items = ask.kind === 'menu' ? ask.items ?? [] : ask.choices ?? [];
  return (
    <>
      {ask.question && <p>{ask.question}</p>}
      <ul class={'menu-list' + (items.length > MENU_ONE_COLUMN ? ' menu-columns' : '')} role="menu"
        aria-label={ask.title || ask.name || 'Choices'} style={{'--menu-rows': String(menuRows(items.length))}}>
        {items.map((item, i) => (
          <li key={i} role="none">
            <button role="menuitem" class="menu-item" title={ask.hints?.[i]}
              aria-keyshortcuts={keys[i]} onClick={() => session.answer(ask, {key: keys[i]})}>
              <kbd aria-hidden="true">{keys[i]?.toUpperCase()}</kbd>
              <span>{item}</span>
            </button>
          </li>
        ))}
      </ul>
      <div class="dialog-actions">
        <button onClick={() => session.cancel(ask)}>Cancel</button>
      </div>
    </>
  );
}

function FormAsk({ask}: {ask: AskEvent}) {
  const session = useSession();
  const lists = useStore(s => s.hello?.lists);
  const isString = ask.kind === 'string';
  const names = isString ? [ask.name ?? ''] : ask.names ?? [];
  const [values, setValues] = useState<string[]>(isString ? [ask.value ?? ''] : [...(ask.values ?? [])]);
  const submit = (e: Event) => {
    e.preventDefault();
    session.answer(ask, isString ? {ok: 1, value: values[0]} : {ok: 1, values});
  };
  /* Enter submits from any field, a select included (A3) */
  const onKeyDown = (e: KeyboardEvent) => {
    const tag = (e.target as HTMLElement).tagName;
    if (e.key === 'Enter' && (tag === 'SELECT' || tag === 'INPUT')) {
      e.preventDefault();
      (e.currentTarget as HTMLFormElement).requestSubmit();
    }
  };
  return (
    <form onSubmit={submit} onKeyDown={onKeyDown}>
      <div class="form-grid">
        {names.map((n, i) => {
          const spec = isString ? {label: n, list: null} : fieldSpec(n);
          const items = spec.list !== null ? lists?.[spec.list] : undefined;
          const change = (e: Event) => {
            const v = values.slice();
            v[i] = (e.target as HTMLInputElement | HTMLSelectElement).value;
            setValues(v);
          };
          if (items) {
            /* the X11 scroll list of variables, parameters, colours, markers or methods */
            const {options, selected} = selectOptions(items, values[i] ?? '');
            return (
              <label key={i}>
                <span>{spec.label}</span>
                <select value={selected} data-list={spec.list} data-autofocus={i === 0 ? '' : undefined} onChange={change}>
                  {options.map(o => <option key={o.value} value={o.value}>{o.label}</option>)}
                </select>
              </label>
            );
          }
          return (
            <label key={i}>
              <span>{spec.label}</span>
              <input value={values[i]} data-autofocus={i === 0 ? '' : undefined} onInput={change} />
            </label>
          );
        })}
      </div>
      <div class="dialog-actions">
        <button type="button" onClick={() => session.cancel(ask)}>{(ask.cancel as string) || 'Cancel'}</button>
        <button type="submit" class="primary">{(ask.ok as string) || 'OK'}</button>
      </div>
    </form>
  );
}

function ChecklistAsk({ask}: {ask: AskEvent}) {
  const session = useSession();
  const names = (ask.names as string[] | undefined) ?? [];
  const [flags, setFlags] = useState<number[]>(names.map((_, i) => ((ask.flags as number[] | undefined)?.[i] ? 1 : 0)));
  const all = (v: number) => setFlags(names.map(() => v));
  const submit = (e: Event) => {
    e.preventDefault();
    session.answer(ask, {ok: 1, flags});
  };
  return (
    <form onSubmit={submit}>
      <fieldset class="checklist">
        <legend class="visually-hidden">{ask.title || 'Choose'}</legend>
        {names.map((n, i) => (
          <label key={i}>
            <input type="checkbox" checked={!!flags[i]} data-autofocus={i === 0 ? '' : undefined}
              onChange={e => {
                const f = flags.slice();
                f[i] = (e.target as HTMLInputElement).checked ? 1 : 0;
                setFlags(f);
              }} />
            <span>{n}</span>
          </label>
        ))}
      </fieldset>
      <div class="dialog-actions">
        <button type="button" onClick={() => all(1)}>All</button>
        <button type="button" onClick={() => all(0)}>None</button>
        <button type="button" onClick={() => session.cancel(ask)}>Cancel</button>
        <button type="submit" class="primary">OK</button>
      </div>
    </form>
  );
}

/* what an ask this interface does not offer yet wants, in words (A13) */
const PENDING: Record<string, string> = {
  grab: 'a point of the AUTO diagram',
  mouse: 'a click in a window this interface does not show yet',
  rubber: 'a box in a window this interface does not show yet',
  drag: 'a drag in a window this interface does not show yet',
};

function PendingAsk({ask}: {ask: AskEvent}) {
  const session = useSession();
  return (
    <>
      <p>
        XPP asks for {PENDING[ask.kind] ?? <b>{ask.kind}</b>}, which this interface does not offer yet. Cancel
        it here.
      </p>
      <div class="dialog-actions">
        <button class="primary" onClick={() => session.cancel(ask)}>Cancel</button>
      </div>
    </>
  );
}

/* focus in on open (the first field, else the first control), trapped while
   open, back to where it was on close; Escape cancels */
function Modal({ask, children}: {ask: AskEvent; children: ComponentChildren}) {
  const session = useSession();
  const table = useStore(s => s.table);
  const core = useStore(s => s.core);
  const box = useRef<HTMLDivElement>(null);
  useEffect(() => {
    const before = document.activeElement as HTMLElement | null;
    const el = box.current!;
    const first = el.querySelector<HTMLElement>('[data-autofocus]') ?? el.querySelector<HTMLElement>(FOCUSABLE);
    first?.focus();
    if (first instanceof HTMLInputElement) first.select();
    return () => before?.focus?.();
  }, [ask.id]);
  const onKeyDown = (e: KeyboardEvent) => {
    if (e.key === 'Escape') {
      e.preventDefault();
      e.stopPropagation();
      session.cancel(ask);
    } else if (e.key === 'Tab') {
      const all = [...box.current!.querySelectorAll<HTMLElement>(FOCUSABLE)];
      if (!all.length) return;
      const i = all.indexOf(document.activeElement as HTMLElement);
      const next = e.shiftKey ? (i <= 0 ? all.length - 1 : i - 1) : (i === all.length - 1 ? 0 : i + 1);
      e.preventDefault();
      const to = all[next];
      to.focus();
      /* as a native Tab does: typing then replaces the field's text instead of adding to it */
      if (to instanceof HTMLInputElement && /^(text|search|number|url|tel|email|password)?$/.test(to.getAttribute('type') ?? ''))
        to.select();
      else if (to instanceof HTMLTextAreaElement) to.select();
    }
  };
  const title = ask.title || ask.name || 'XPP';
  return (
    <div class="dialog-backdrop">
      <div class="dialog" ref={box} role="dialog" aria-modal="true" aria-labelledby="ask-title" data-ask={ask.kind}
        onKeyDown={onKeyDown}>
        <div class="dialog-title-row">
          <h2 id="ask-title">{title}</h2>
          <HelpButton target={askHelp({table, core}, ask.kind)} label={title} />
        </div>
        {children}
      </div>
    </div>
  );
}

export function AskDialog() {
  const ask = useStore(s => s.ask);
  const pick = useStore(s => s.pick);
  const auto = useStore(s => s.diagram.open);
  if (!ask || ask.kind === 'pixels' || ask.kind === 'alert') return null;
  if (pick && pick.ask === ask.id) return null; /* a plot mode (PlotView.tsx, AutoView.tsx) */
  if (ask.kind === 'grab' && auto) return null; /* the AUTO view's grab (AutoView.tsx) */
  const body = ask.kind === 'menu' || ask.kind === 'choice' ? <MenuAsk ask={ask} />
    : ask.kind === 'string' || ask.kind === 'form' ? <FormAsk ask={ask} />
      : ask.kind === 'checklist' ? <ChecklistAsk ask={ask} />
        : ask.kind === 'file' ? <FileAsk ask={ask} />
          : <PendingAsk ask={ask} />;
  return <Modal key={ask.id} ask={ask}>{body}</Modal>;
}
