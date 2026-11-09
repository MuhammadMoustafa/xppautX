/* The core's prompts (docs/protocol.md "Asks") as a modal dialog: focus
   moves into it, stays in it (Tab cycles), Escape cancels, and focus goes
   back where it was when it closes. Menus, yes/no choices, string boxes,
   forms (a field that picks from `hello.lists` is a select, the others take
   what the ask's `kinds` say, through Field.tsx: OK waits until each
   does), checklists and
   files (FileDialog.tsx); mouse,
   rubber and drag asks are plot modes (PlotView.tsx); other kinds say they
   are not offered yet and offer Cancel (A13, docs/ui-v2.md). */
import type {ComponentChildren} from 'preact';
import {useEffect, useLayoutEffect, useState} from 'preact/hooks';
import {answerFill, litAskKey} from '../store/player';
import {LEAVE_ASK} from '../store/state';
import {askHelp} from '../help/links';
import {fieldSpec, selectOptions} from '../protocol/lists';
import {choiceLabel, ruleMessage, specOfKind, TEXT, type FieldSpec} from '../store/fieldKinds';
import type {AskEvent, HelloEvent, TextStyle} from '../protocol/types';
import {FileAsk} from './FileDialog';
import {nativeFileDialog} from '../pickers';
import {textPx} from '../plot/marks';
import {Dialog} from './Dialog';
import {Control, FormRow, focusFirstError, useFeedback} from './FormFields';
import {useSession, useStore} from './context';
import {useDark} from './theme';

/** the action's text of an ask: the core's own when it names what the action does, else the kind's ("Apply"; never OK) */
export function askAction(ask: AskEvent, fallback: string): string {
  const named = (ask.ok as string | undefined) ?? '';
  return named && named !== 'OK' ? named : fallback;
}

/** an ask's dialog: the one shell, titled by the ask, its ? the manual's section for it, its × and a click outside cancelling it (the core waits
    for the answer, so Escape is the ask's own cancel) */
export function AskShell({ask, action, onSubmit, extra, problems, children, width, onClose}: {
  ask: AskEvent; action?: {label: string; onClick?: () => void; attrs?: Record<string, string>}; onSubmit?: (e: Event) => void; extra?: ComponentChildren; problems?: number;
  children: ComponentChildren; width?: 'form' | 'list'; onClose?: () => void;
}) {
  const session = useSession();
  const table = useStore(s => s.table);
  const title = ask.title || ask.name || 'XPP';
  return <Dialog title={title} name={title} description={ask.question as string | undefined} help={askHelp({table}, ask.kind)} width={width} focusKey={ask.id}
    onClose={onClose ?? (() => session.cancel(ask))} action={action} extra={extra} onSubmit={onSubmit} problems={problems} attrs={{'data-ask': ask.kind}}>
    {children}
  </Dialog>;
}

function ChoiceAsk({ask}: {ask: AskEvent}) {
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
  const items = ask.choices ?? [];
  return (
    <AskShell ask={ask}>
      <ul class="menu-list" role="menu" aria-label={ask.title || ask.name || 'Choices'}>
        {items.map((item, i) => (
          <li key={i} role="none">
            <button role="menuitem" class="menu-item" title={ask.hints?.[i]}
              aria-keyshortcuts={keys[i]} onClick={() => session.answer(ask, keys[i] ? {key: keys[i]} : {index: i})}>
              <kbd aria-hidden="true">{keys[i]?.toUpperCase()}</kbd>
              <span>{item}</span>
            </button>
          </li>
        ))}
      </ul>
    </AskShell>
  );
}

/** why a typed ask field is wrong once it was left: every field of an ask is needed, so an empty one is "Required" */
function askMessage(spec: FieldSpec, text: string): string | null {
  if (spec.kind === 'boolean' || spec.kind === 'color') return ruleMessage(spec, text);
  return text.trim() === '' && spec.kind !== 'text' ? 'Required' : ruleMessage(spec, text);
}

function FormAsk({ask}: {ask: AskEvent}) {
  const session = useSession();
  const lists = useStore(s => s.hello?.lists);
  const dark = useDark(useStore(s => s.theme));
  const feedback = useFeedback();
  const isString = ask.kind === 'string';
  const names = isString ? [ask.name ?? ''] : ask.names ?? [];
  const [values, setValues] = useState<string[]>(isString ? [ask.value ?? ''] : [...(ask.values ?? [])]);
  /* what each typed field takes, as the core says (`kinds`, one per field); a list field picks, so takes its pick */
  const specs = names.map((n, i): FieldSpec =>
    (!isString && fieldSpec(n).list !== null ? TEXT
      : specOfKind(ask.kinds![i], lists, {options: ask.options?.[i], range: ask.ranges?.[i]})));
  const messages = specs.map((spec, i) => askMessage(spec, values[i] ?? ''));
  const shownCount = messages.filter((m, i) => m && feedback.shows(String(i))).length;
  const submit = (e: Event) => {
    e.preventDefault();
    if (messages.some(Boolean)) {
      feedback.attempt();
      queueMicrotask(() => focusFirstError(document.querySelector('[data-ask]')));
      return;
    }
    session.answer(ask, isString ? {ok: 1, value: values[0]} : {ok: 1, values});
  };
  /* a grid ask (`columns` and `rows`, the axes dialog): its first columns x rows fields are the table's
     cells, row by row; the others are plain fields under it */
  const columns = isString ? [] : ask.columns ?? [];
  const rowLabels = ask.rows ?? [];
  const gridCells = columns.length * rowLabels.length;
  const set = (i: number, text: string) => setValues(prev => prev.map((v, j) => (j === i ? text : v)));
  /* a table cell: named by its column and row */
  const cell = (i: number, cellLabel: string) => {
    const typed = specs[i];
    return <Control spec={typed} value={values[i] ?? ''} label={cellLabel} dark={dark} invalid={!!messages[i] && feedback.shows(String(i))}
      onBlur={() => feedback.touch(String(i))} onChange={t => set(i, t)} />;
  };
  /* the field i: a select for a list (the X11 scroll list of variables, parameters, colors, markers or methods), else the typed control */
  const field = (i: number) => {
    const n = names[i];
    const spec = isString ? {label: n, list: null} : fieldSpec(n);
    const items = spec.list !== null ? lists?.[spec.list] : undefined;
    const error = feedback.shows(String(i)) ? messages[i] : null;
    if (items) {
      const {options, selected} = selectOptions(items, values[i] ?? '');
      return <FormRow key={i} name={n} label={spec.label} error={error}>
        <select value={selected} data-list={spec.list} aria-label={spec.label} onChange={(e: Event) => set(i, (e.target as HTMLSelectElement).value)}>
          {options.map(o => <option key={o.value} value={o.value}>{o.label}</option>)}
        </select></FormRow>;
    }
    return <FormRow key={i} name={n} label={spec.label} error={error}>
      <Control spec={specs[i]} value={values[i] ?? ''} label={spec.label} dark={dark} invalid={!!error} verbatim
        onBlur={() => feedback.touch(String(i))} onChange={t => set(i, t)} />
    </FormRow>;
  };
  return (
    <AskShell ask={ask} onSubmit={submit} action={{label: askAction(ask, 'Apply')}} problems={shownCount} width={columns.length > 2 ? 'list' : 'form'}>
      {columns.length > 0 && (
        <table class="form-columns" data-form-columns>
          <thead>
            <tr><td />{columns.map(c => <th key={c} scope="col">{c}</th>)}</tr>
          </thead>
          <tbody>
            {rowLabels.map((r, row) => (
              <tr key={r}>
                <th scope="row">{r}</th>
                {columns.map((c, col) => <td key={c}>{cell(row * columns.length + col, `${c} ${r.toLowerCase()}`)}</td>)}
              </tr>
            ))}
          </tbody>
        </table>
      )}
      {names.map((_, i) => (i < gridCells ? null : field(i)))}
    </AskShell>
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
    <AskShell ask={ask} onSubmit={submit} action={{label: askAction(ask, 'Apply')}}
      extra={<><button type="button" onClick={() => all(1)}>All</button><button type="button" onClick={() => all(0)}>None</button></>}>
      <fieldset class="checklist">
        <legend class="visually-hidden">{ask.title || 'Choose'}</legend>
        {names.map((n, i) => (
          <label key={i} class="switch-row">
            <span class="switch"><input type="checkbox" role="switch" checked={!!flags[i]} data-autofocus={i === 0 ? '' : undefined}
              onChange={e => {
                const f = flags.slice();
                f[i] = (e.target as HTMLInputElement).checked ? 1 : 0;
                setFlags(f);
              }} /><i aria-hidden="true" /></span>
            <span>{n}</span>
          </label>
        ))}
      </fieldset>
    </AskShell>
  );
}

/* Text,etc/Text: the text, size, style and color of the text just placed; the color is any color with its alpha (ui/ColorField.tsx), which the
   core keeps and its PDF and SVG draw. A text already there is edited through annotation.edit's command form. */
function TextAsk({ask}: {ask: AskEvent}) {
  const session = useSession();
  const limits = useStore(s => s.hello?.text) as HelloEvent['text'];
  const styles = useStore(s => s.hello?.text_styles) as HelloEvent['text_styles'];
  const dark = useDark(useStore(s => s.theme));
  const feedback = useFeedback();
  const [text, setText] = useState(String(ask.value ?? ''));
  const [size, setSize] = useState(Number(ask.size)), [style, setStyle] = useState(ask.style as TextStyle), [color, setColor] = useState(String(ask.color));
  const messages = [text === '' ? 'Required' : null, ruleMessage({kind: 'color'}, color)];
  const submit = (e: Event) => {
    e.preventDefault();
    if (messages.some(Boolean)) {
      feedback.attempt();
      queueMicrotask(() => focusFirstError(document.querySelector('[data-ask]')));
      return;
    }
    session.answer(ask, {ok: 1, value: text, size, style, color});
  };
  return <AskShell ask={ask} onSubmit={submit} action={{label: askAction(ask, 'Place text')}} problems={messages.filter((m, i) => m && feedback.shows(String(i))).length}>
    <FormRow name="text" label="Text" required error={feedback.shows('0') ? messages[0] : null}>
      <span class="input-box"><input data-text-input data-autofocus value={text} maxLength={limits.max_length} aria-label="Text" onBlur={() => feedback.touch('0')}
        onInput={e => setText(e.currentTarget.value)} /></span>
    </FormRow>
    <FormRow name="size" label="Size">
      <select data-text-size value={size} aria-label="Size" onChange={e => setSize(Number(e.currentTarget.value))}>
        {Array.from({length: limits.size_max + 1}, (_, i) => <option key={i} value={i}>{i} ({textPx(i)} px)</option>)}
      </select>
    </FormRow>
    <FormRow name="style" label="Style">
      <select data-text-style value={style} aria-label="Style" onChange={e => setStyle(e.currentTarget.value as TextStyle)}>
        {styles.map(name => <option key={name} value={name}>{choiceLabel(name)}</option>)}
      </select>
    </FormRow>
    <FormRow name="color" label="Color" error={feedback.shows('1') ? messages[1] : null}>
      <Control spec={{kind: 'color'}} value={color} label="Text" dark={dark} onChange={setColor} onBlur={() => feedback.touch('1')} />
    </FormRow>
  </AskShell>;
}

/* what an ask this interface does not offer yet wants, in words (A13) */
const PENDING: Record<string, string> = {
  grab: 'a point of the autoX diagram',
  mouse: 'a click in a window this interface does not show yet',
  rubber: 'a box in a window this interface does not show yet',
  drag: 'a drag in a window this interface does not show yet',
};

function PendingAsk({ask}: {ask: AskEvent}) {
  const session = useSession();
  return (
    <AskShell ask={ask} action={{label: 'Dismiss', onClick: () => session.cancel(ask)}}>
      <p>XPP asks for {PENDING[ask.kind] ?? <b>{ask.kind}</b>}, which this interface does not offer yet. Dismiss it here.</p>
    </AskShell>
  );
}

/** whether an ask is on screen as the modal dialog (not answered at once, not a plot mode, not the
    autoX view's grab, not the desktop's own file dialog) */
export function askIsModal(ask: AskEvent | null, pick: {ask: number} | null, autoxOpen: boolean): boolean {
  if (!ask || ask.kind === 'pixels') return false;
  if (pick && pick.ask === ask.id) return false;
  if (ask.kind === 'grab' && autoxOpen) return false;
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
  const items = ask.kind === 'choice' ? ask.choices ?? [] : [];
  const keys = ask.keys ?? '';
  const names = ask.kind === 'string' ? [ask.name ?? ''] : ask.kind === 'form' ? ask.names ?? [] : ask.kind === 'file' ? ['File'] : [];
  const shownValues = ask.kind === 'string' ? [ask.value ?? ''] : ask.kind === 'form' ? ask.values ?? [] : [''];
  return (
    <Dialog title={title} name={`${title} (answered by the recording)`} description={ask.question as string | undefined} closable={false} focusKey={ask.id}
      onClose={() => undefined} className="played-ask" attrs={{'data-ask': ask.kind}}
      action={{label: askAction(ask, 'Apply'), attrs: {class: 'played-ok' + (done ? ' lit' : ''), disabled: 'true'}}}>
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
      {names.map((n, i) => (
        <FormRow key={i} name={n} label={n}>
          <span class={'played-value' + (fill && !done ? ' typing' : '')}>{fill ? typed[i] ?? '' : shownValues[i] ?? ''}</span>
        </FormRow>
      ))}
      {!items.length && !names.length && <p class="muted">Answered from the recording.</p>}
    </Dialog>
  );
}

export function AskDialog() {
  const ask = useStore(s => s.ask);
  const pick = useStore(s => s.pick);
  const auto = useStore(s => s.diagram.open);
  const played = useStore(s => s.player.running >= 0);
  /* the page's own leave question is the user's, even while a recording plays */
  if (ask && played && ask.id !== LEAVE_ASK && ask.kind !== 'pixels')
    return <PlayedAsk key={ask.id} ask={ask} />;
  if (!ask || !askIsModal(ask, pick, auto)) return null;
  return ask.kind === 'choice' ? <ChoiceAsk key={ask.id} ask={ask} />
    : ask.kind === 'string' || ask.kind === 'form' ? <FormAsk key={ask.id} ask={ask} />
      : ask.kind === 'checklist' ? <ChecklistAsk key={ask.id} ask={ask} />
        : ask.kind === 'file' ? <FileAsk key={ask.id} ask={ask} />
          : ask.kind === 'text' ? <TextAsk key={ask.id} ask={ask} />
            : <PendingAsk key={ask.id} ask={ask} />;
}
