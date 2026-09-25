/* Every input box of the page (T31): one component, its text checked by
   the validator of its kind (store/fieldKinds.ts) while typed. A text the
   box does not take is marked (aria-invalid, a short message under it,
   "A whole number") and is never committed: Enter keeps it and its
   message, and so does leaving the box, which holds the text, marked,
   until it is corrected or Escape drops it (never a silent revert).

   Two ways to use it:
   - in a form (`onInput`): the form holds the text and commits the whole
     form on OK, which it refuses while any box is marked (fieldsValid);
     Enter in a marked box does not reach the form;
   - on its own (`onCommit`): the box edits a draft from `editValue` (the
     full precision) while focused, shows `value` otherwise, and commits on
     Enter or when it loses the focus, only a text it takes and only when
     it differs from what the edit started from (so a value that moved
     under a focused, untouched box is not sent back); `commitAfter` ms
     after the last keystroke too (the slider's box). Escape drops the
     draft.

   Number boxes with a `step` step by it with ArrowUp/ArrowDown, as a
   spinner does; a name box suggests its names (a datalist). */
import type {InputHTMLAttributes} from 'preact';
import {useEffect, useId, useRef, useState} from 'preact/hooks';
import {fieldError, fieldInputMode, fieldMessage, type FieldSpec} from '../store/fieldKinds';

type InputAttrs = Omit<InputHTMLAttributes<HTMLInputElement>,
  'value' | 'onInput' | 'onChange' | 'type' | 'step' | 'inputMode' | 'list' | 'onBlur' | 'onFocus'>;

export interface FieldProps extends InputAttrs {
  spec: FieldSpec;
  /** the text shown (a form's text; a box on its own: the value shown while not edited) */
  value: string;
  /** a form's box: every change of its text */
  onInput?: (text: string) => void;
  /** a box on its own: a text it takes, changed, on Enter, on leaving it, or `commitAfter` */
  onCommit?: (text: string) => void;
  /** a box on its own: the text an edit starts from (default `value`) */
  editValue?: string;
  /** a box on its own: also commit this many ms after the last keystroke */
  commitAfter?: number;
  /** the full message for a text, when this box's rules say more than its kind's (null: takes it) */
  check?: (text: string) => string | null;
  /** a message from elsewhere (the core's refusal, a rule between boxes), after the box's own */
  error?: string | null;
  /** number boxes: what ArrowUp/ArrowDown add */
  step?: number | null;
  type?: 'text' | 'search';
  onFocus?: (e: FocusEvent) => void;
  onBlur?: (e: FocusEvent) => void;
}

/** `v` rounded to the decimals of `step`, so 0.1 + 0.2 shows as 0.3 */
function stepped(v: number, step: number): string {
  const decimals = Math.max(0, -Math.floor(Math.log10(Math.abs(step))) + 1);
  return String(Number(v.toFixed(Math.min(decimals, 15))));
}

export function Field(props: FieldProps) {
  const {
    spec, value, onInput, onCommit, editValue, commitAfter, check, error, step, type = 'text', id, onKeyDown,
    onFocus, onBlur, ...rest
  } = props;
  const own = useId();
  const baseId = id ?? `field-${own}`;
  const standalone = !!onCommit;
  /* a box on its own: its text while edited, and after a blur that did not commit (held) */
  const [draft, setDraft] = useState<string | null>(null);
  const started = useRef<string>('');
  const dropped = useRef(false);
  const timer = useRef<ReturnType<typeof setTimeout> | null>(null);
  useEffect(() => () => { if (timer.current) clearTimeout(timer.current); }, []);

  const text = standalone ? (draft ?? value) : value;
  const messageOf = (t: string): string | null => {
    if (check) return check(t);
    const phrase = fieldError(spec, t);
    return phrase ? fieldMessage(phrase) : null;
  };
  /* a box on its own checks only what is typed into it, not the value it is given */
  const typed = standalone && draft === null ? null : messageOf(text);
  const message = typed ?? error ?? null;
  const msgId = `${baseId}-msg`;
  const listId = spec.kind === 'name' ? `${baseId}-names` : undefined;

  const clearTimer = () => {
    if (timer.current) { clearTimeout(timer.current); timer.current = null; }
  };
  /* commit a text the box takes when it changed; false when it does not take it */
  const commit = (t: string): boolean => {
    clearTimer();
    const trimmed = t.trim();
    if (messageOf(trimmed) !== null) return false;
    if (trimmed !== started.current) {
      started.current = trimmed;
      onCommit!(trimmed);
    }
    return true;
  };
  const change = (t: string) => {
    if (!standalone) { onInput?.(t); return; }
    setDraft(t);
    clearTimer();
    if (commitAfter !== undefined && messageOf(t.trim()) === null)
      timer.current = setTimeout(() => { timer.current = null; commit(t); }, commitAfter);
  };
  const spin = (dir: 1 | -1, el: HTMLInputElement) => {
    if (!step || !(step > 0) || (spec.kind !== 'number' && spec.kind !== 'integer')) return false;
    const v = Number(text.trim());
    if (text.trim() === '' || !Number.isFinite(v)) return false;
    let next = v + dir * step;
    const lo = el.min !== '' ? Number(el.min) : NaN, hi = el.max !== '' ? Number(el.max) : NaN;
    if (Number.isFinite(lo)) next = Math.max(lo, next);
    if (Number.isFinite(hi)) next = Math.min(hi, next);
    change(stepped(next, step));
    return true;
  };

  return (
    <>
      <input {...rest} id={baseId} type={type} value={text} step={step ?? undefined} inputMode={fieldInputMode(spec)}
        list={listId} spellcheck={spec.kind === 'text' ? undefined : false}
        autocomplete={spec.kind === 'text' || spec.kind === 'file' ? undefined : 'off'}
        aria-invalid={message ? 'true' : undefined}
        aria-describedby={message ? msgId : undefined} data-kind={spec.kind}
        onFocus={e => {
          if (standalone && draft === null) {
            const from = editValue ?? value;
            started.current = from.trim();
            setDraft(from);
          }
          onFocus?.(e);
        }}
        onInput={e => change((e.target as HTMLInputElement).value)}
        onBlur={e => {
          if (standalone) {
            if (dropped.current) { dropped.current = false; clearTimer(); setDraft(null); }
            else if (draft !== null && commit(draft)) setDraft(null);
          }
          onBlur?.(e);
        }}
        onKeyDown={e => {
          const el = e.target as HTMLInputElement;
          if (e.key === 'Enter') {
            if (messageOf(el.value) !== null) {
              /* a text the box does not take is not committed: it stays, with its message */
              e.preventDefault();
              e.stopPropagation();
              return;
            }
            if (standalone) { e.preventDefault(); el.blur(); }
          } else if (e.key === 'Escape' && standalone) {
            e.stopPropagation();
            dropped.current = true;
            el.blur();
          } else if ((e.key === 'ArrowUp' || e.key === 'ArrowDown') && !e.altKey && !e.ctrlKey && !e.metaKey) {
            if (spin(e.key === 'ArrowUp' ? 1 : -1, el)) e.preventDefault();
          }
          onKeyDown?.(e as never);
        }}
      />
      {listId && spec.kind === 'name' && (
        <datalist id={listId}>{spec.names.map(n => <option key={n} value={n} />)}</datalist>
      )}
      {message && <p class="field-error" id={msgId} aria-live="polite">{message}</p>}
    </>
  );
}
