/* The core's prompts (docs/protocol.md "Asks") as a modal dialog: focus
   moves into it, stays in it (Tab cycles), Escape cancels, and focus goes
   back where it was when it closes. This scaffold covers menus, yes/no
   choices, string boxes and forms; alerts are notifications (session.ts);
   other kinds offer Cancel until their components exist (docs/ui-v2.md). */
import type {ComponentChildren} from 'preact';
import {useEffect, useRef, useState} from 'preact/hooks';
import type {AskEvent} from '../protocol/types';
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
      <ul class="menu-list" role="menu" aria-label={ask.title || ask.name || 'Choices'}>
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
  const isString = ask.kind === 'string';
  const names = isString ? [ask.name ?? ''] : ask.names ?? [];
  const [values, setValues] = useState<string[]>(isString ? [ask.value ?? ''] : [...(ask.values ?? [])]);
  const submit = (e: Event) => {
    e.preventDefault();
    session.answer(ask, isString ? {ok: 1, value: values[0]} : {ok: 1, values});
  };
  return (
    <form onSubmit={submit}>
      <div class="form-grid">
        {names.map((n, i) => (
          <label key={i}>
            <span>{n.replace(/^\*\d/, '')}</span>
            <input
              value={values[i]}
              data-autofocus={i === 0 ? '' : undefined}
              onInput={e => {
                const v = values.slice();
                v[i] = (e.target as HTMLInputElement).value;
                setValues(v);
              }}
            />
          </label>
        ))}
      </div>
      <div class="dialog-actions">
        <button type="button" onClick={() => session.cancel(ask)}>{(ask.cancel as string) || 'Cancel'}</button>
        <button type="submit" class="primary">{(ask.ok as string) || 'OK'}</button>
      </div>
    </form>
  );
}

function PendingAsk({ask}: {ask: AskEvent}) {
  const session = useSession();
  return (
    <>
      <p>
        XPP asks for <b>{ask.kind}</b> input, which the new interface does not offer yet. Cancel it here, or use
        the classic interface for this command.
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
      all[next].focus();
    }
  };
  const title = ask.title || ask.name || 'XPP';
  return (
    <div class="dialog-backdrop">
      <div class="dialog" ref={box} role="dialog" aria-modal="true" aria-labelledby="ask-title" data-ask={ask.kind}
        onKeyDown={onKeyDown}>
        <h2 id="ask-title">{title}</h2>
        {children}
      </div>
    </div>
  );
}

export function AskDialog() {
  const ask = useStore(s => s.ask);
  if (!ask || ask.kind === 'pixels' || ask.kind === 'alert') return null;
  const body = ask.kind === 'menu' || ask.kind === 'choice' ? <MenuAsk ask={ask} />
    : ask.kind === 'string' || ask.kind === 'form' ? <FormAsk ask={ask} />
      : <PendingAsk ask={ask} />;
  return <Modal key={ask.id} ask={ask}>{body}</Modal>;
}
