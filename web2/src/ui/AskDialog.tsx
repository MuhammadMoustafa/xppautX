/* The core's prompts (docs/protocol.md "Asks") as a modal dialog: focus
   moves into it, stays in it (Tab cycles), Escape cancels, and focus goes
   back where it was when it closes. Menus, yes/no choices, string boxes,
   forms (a field that picks from `hello.lists` is a select, the others take
   what the ask's `kinds` say, through Field.tsx: OK waits until each
   does), checklists and
   files (FileDialog.tsx); alerts are notifications (session.ts); mouse,
   rubber and drag asks are plot modes (PlotView.tsx); other kinds say they
   are not offered yet and offer Cancel (A13, docs/ui-v2.md). */
import type {ComponentChildren} from 'preact';
import {useEffect, useLayoutEffect, useRef, useState} from 'preact/hooks';
import {answerFill, litAskKey} from '../store/player';
import {askHelp} from '../help/links';
import {fieldSpec, selectOptions} from '../protocol/lists';
import {fieldsValid, specOfKind, TEXT, type FieldSpec} from '../store/fieldKinds';
import type {AskEvent} from '../protocol/types';
import {Field} from './Field';
import {FileAsk} from './FileDialog';
import {nativeFileDialog} from '../pickers';
import {HelpButton} from './HelpButton';
import {MENU_ONE_COLUMN, menuRows} from './menuLayout';
import {useSession, useStore} from './context';
import {FOCUSABLE, useDialogFocus} from './dialogFocus';


function MenuAsk({ask}: {ask: AskEvent}) {
  const session = useSession();
  const keys = ask.keys ?? '';
  /* a layout effect, like the dialog's focus: a key typed the moment the menu is there finds it */
  useLayoutEffect(() => {
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
  /* what each typed field takes, as the core says (`kinds`; none: text); a list field picks, so takes its pick */
  const specs = names.map((n, i): FieldSpec =>
    (!isString && fieldSpec(n).list !== null ? TEXT : specOfKind(ask.kinds?.[i], lists)));
  const valid = fieldsValid(specs, values);
  const submit = (e: Event) => {
    e.preventDefault();
    if (!valid) return;
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
          const set = (text: string) => {
            const v = values.slice();
            v[i] = text;
            setValues(v);
          };
          const change = (e: Event) => set((e.target as HTMLSelectElement).value);
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
              <Field spec={specs[i]} value={values[i] ?? ''} data-autofocus={i === 0 ? '' : undefined} onInput={set} />
            </label>
          );
        })}
      </div>
      <div class="dialog-actions">
        <button type="button" onClick={() => session.cancel(ask)}>{(ask.cancel as string) || 'Cancel'}</button>
        <button type="submit" class="primary" disabled={!valid}>{(ask.ok as string) || 'OK'}</button>
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
  useDialogFocus(box, [ask.id]);
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

/** whether an ask is on screen as the modal dialog (not answered at once, not a plot mode, not the
    AUTO view's grab, not the desktop's own file dialog) */
export function askIsModal(ask: AskEvent | null, pick: {ask: number} | null, autoOpen: boolean): boolean {
  if (!ask || ask.kind === 'pixels' || ask.kind === 'alert') return false;
  if (pick && pick.ask === ask.id) return false;
  if (ask.kind === 'grab' && autoOpen) return false;
  return !(ask.kind === 'file' && nativeFileDialog());
}

/* the texts typed so far of `full`, over `ms` (the press's pace, W59b):
   all of it at 70%, then `done` (OK lights) */
function useTyping(full: string[] | null, ms: number, token: number): {typed: string[]; done: boolean} {
  const [t, setT] = useState(0);
  useEffect(() => {
    setT(0);
    if (!full) return;
    const start = performance.now(), span = Math.max(ms * 0.7, 1);
    const id = setInterval(() => {
      const f = (performance.now() - start) / span;
      setT(f);
      if (f >= 1.2) clearInterval(id);
    }, 40);
    return () => clearInterval(id);
  }, [token]);
  if (!full) return {typed: [], done: false};
  const total = full.reduce((n, s) => n + s.length, 0);
  let left = Math.round(Math.min(1, t) * total);
  const typed = full.map(s => {
    const k = Math.min(s.length, left);
    left -= k;
    return s.slice(0, k);
  });
  return {typed, done: t >= 1};
}

/* a question a recording's step asks (W59b): shown, never answered here;
   the player's answer is filled in as the core's press says, then OK lights */
function PlayedAsk({ask}: {ask: AskEvent}) {
  const key = useStore(s => litAskKey(s.player));
  const fill = useStore(s => answerFill(s.player));
  const press = useStore(s => s.player.press);
  const token = useStore(s => s.player.presses);
  const {typed, done} = useTyping(fill, press?.ms ?? 0, token);
  const title = ask.title || ask.name || 'XPP';
  const items = ask.kind === 'menu' ? ask.items ?? [] : ask.kind === 'choice' ? ask.choices ?? [] : [];
  const keys = ask.keys ?? '';
  const names = ask.kind === 'string' ? [ask.name ?? ''] : ask.kind === 'form' ? ask.names ?? [] : ask.kind === 'file' ? ['File'] : [];
  const shownValues = ask.kind === 'string' ? [ask.value ?? ''] : ask.kind === 'form' ? ask.values ?? [] : [''];
  return (
    <div class="dialog played-ask" role="dialog" aria-label={`${title} (answered by the recording)`} data-ask={ask.kind}>
      <div class="dialog-title-row"><h2>{title}</h2></div>
      {ask.question && <p>{ask.question}</p>}
      {items.length > 0 && (
        <ul class="menu-list" role="menu">
          {items.map((item, i) => (
            <li key={i} role="none">
              <span role="menuitem" class={'menu-item' + (key && keys[i]?.toLowerCase() === key.toLowerCase() ? ' lit' : '')}>
                <kbd aria-hidden="true">{keys[i]?.toUpperCase()}</kbd><span>{item}</span>
              </span>
            </li>
          ))}
        </ul>
      )}
      {names.length > 0 && (
        <div class="form-grid">
          {names.map((n, i) => (
            <label key={i}>
              <span>{n}</span>
              <span class={'played-value' + (fill && !done ? ' typing' : '')}>{fill ? typed[i] ?? '' : shownValues[i] ?? ''}</span>
            </label>
          ))}
        </div>
      )}
      {!items.length && !names.length && <p class="muted">Answered from the recording.</p>}
      <div class="dialog-actions">
        <span class={'played-ok' + (done ? ' lit' : '')}>{(ask.ok as string) || 'OK'}</span>
      </div>
    </div>
  );
}

export function AskDialog() {
  const ask = useStore(s => s.ask);
  const pick = useStore(s => s.pick);
  const auto = useStore(s => s.diagram.open);
  const played = useStore(s => s.player.running >= 0);
  if (ask && played && ask.kind !== 'pixels' && ask.kind !== 'alert') return <PlayedAsk key={ask.id} ask={ask} />;
  if (!ask || !askIsModal(ask, pick, auto)) return null;
  const body = ask.kind === 'menu' || ask.kind === 'choice' ? <MenuAsk ask={ask} />
    : ask.kind === 'string' || ask.kind === 'form' ? <FormAsk ask={ask} />
      : ask.kind === 'checklist' ? <ChecklistAsk ask={ask} />
        : ask.kind === 'file' ? <FileAsk ask={ask} />
          : <PendingAsk ask={ask} />;
  return <Modal key={ask.id} ask={ask}>{body}</Modal>;
}
