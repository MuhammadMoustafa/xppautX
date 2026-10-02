/* Every input box of the page (T31): one component, its text checked by
   the validator of its kind (store/fieldKinds.ts) while typed. A keystroke,
   paste or drop that would leave a text the kind does not take, and is not
   an incomplete prefix of one it does, is refused outright (T35d,
   fieldAcceptsEdit): the box is left exactly as it was, and a brief
   aria-live hint names what was wrong (fieldCharMessage/fieldPasteMessage);
   the box itself is not marked invalid, since its actual text is still
   fine. A text the box does take but that is left half-typed when the box
   loses focus ("-", "1e-", a lone "%") is marked (aria-invalid, a message
   saying what is missing, fieldIncompleteReason) and never committed: it
   stays, marked, until it is corrected or Escape drops it (never a silent
   revert). A formula (`%...`) the core itself refuses (WF-001) is marked
   the same way, from the `error` prop, until it is corrected or Escape
   drops it back to the value the core has.

   Two ways to use it:
   - in a form (`onInput`): the form holds the text and commits the whole
     form on OK, which it refuses while any box is marked (fieldsValid);
     Enter in a marked box does not reach the form;
   - on its own (`onCommit`): the box edits a draft from `editValue` (the
     full precision) while focused, shows `value` otherwise, and commits on
     Enter or when it loses the focus, only a text it takes and only when
     it differs from what the edit started from (so a value that moved
     under a focused, untouched box is not sent back); `commitAfter` ms
     after the last keystroke too (the slider's box). The draft is kept,
     not cleared, until the commit is known to have settled (`settling`
     false) with no `error`: while the core is still judging a sent
     formula, or has refused it, the box goes on showing what was sent.
     Escape drops the draft, whether it is being typed, half-typed, or
     waiting on (or refused by) the core, and never reaches anything else
     (the page's hotkeys, a panel's own Escape) while it does: the box
     handles its own Escape before any of that sees the key.

   Number boxes with a `step` step by it with ArrowUp/ArrowDown, as a
   spinner does; a name box suggests its names (a datalist). */
import type {InputHTMLAttributes} from 'preact';
import {useEffect, useId, useRef, useState} from 'preact/hooks';
import {
  fieldAcceptsEdit, fieldCharMessage, fieldError, fieldIncomplete, fieldIncompleteReason, fieldInputMode, fieldMessage,
  fieldPasteMessage, fieldPasteOffender, type FieldSpec,
} from '../store/fieldKinds';

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
  /** a box on its own: a sent edit is still waiting for the core (the field's own draft is kept,
      not cleared, until this is false and `error` is not set: WF-001) */
  settling?: boolean;
  /** a box on its own: Escape dropped a draft that carried the core's `error` (WF-001) -- the
      owner's turn to forget that error too, since Field does not hold it */
  onDropError?: () => void;
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
    spec, value, onInput, onCommit, editValue, commitAfter, check, error, settling, onDropError, step, type = 'text', id,
    onKeyDown, onFocus, onBlur, ...rest
  } = props;
  const own = useId();
  const baseId = id ?? `field-${own}`;
  const standalone = !!onCommit;
  /* a box on its own: its text while edited, and after a blur that did not commit (held) */
  const [draft, setDraftState] = useState<string | null>(null);
  /* the draft as the last event left it: a handler reads this, never the `draft` of its render,
     because a keystroke and the Enter or blur after it can arrive before Preact has rendered
     (a fast typist, a paste then Enter, a slow machine): the render's draft is then the text
     before the keystroke, and the commit would send that and lose the edit */
  const latest = useRef<string | null>(null);
  const setDraft = (t: string | null) => { latest.current = t; setDraftState(t); };
  const started = useRef<string>('');
  const dropped = useRef(false);
  /* focused, and whether Enter was refused since the last keystroke: the start of a number is
     flagged only then, or once the box is left (fieldIncomplete) */
  const [focused, setFocused] = useState(false);
  const [refused, setRefused] = useState(false);
  /* a brief note on a keystroke, paste or drop fieldAcceptsEdit turned away (T35d): the box's
     text did not change, so this never marks the box invalid, and clears on the next accepted
     edit, a focus change, or Escape */
  const [hint, setHint] = useState<string | null>(null);
  const timer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const inputRef = useRef<HTMLInputElement | null>(null);
  useEffect(() => () => { if (timer.current) clearTimeout(timer.current); }, []);
  /* a sent edit the core refuses (WF-001) arrives after Enter already moved the focus on (a
     standalone box blurs once its own rules take the text): without this, the box would sit
     there marked but unfocused, and Escape -- which is the box's own key -- would have nothing
     to reach it through, so it would fall to the page's hotkeys instead (sent to the core) or a
     panel's own Escape. Taking the focus back when the refusal lands keeps Escape (and any
     correction) working on the box the core is actually complaining about. */
  useEffect(() => {
    if (standalone && error && !focused) inputRef.current?.focus();
  }, [standalone, error, focused]);

  const text = standalone ? (draft ?? value) : value;
  const messageOf = (t: string): string | null => {
    if (check) return check(t);
    const phrase = fieldError(spec, t);
    return phrase ? fieldMessage(phrase) : null;
  };
  /* the message shown for a text left as it is (typed, or held after Enter/blur): a half-typed
     text (fieldIncompleteReason) says what is missing, unless this box has its own rules beyond
     its kind's (`check`, e.g. AutoSettings' "Max points (NMX) must be..."), which say more and
     always have the last word; anything else falls back to the kind's ordinary message */
  const shownMessageOf = (t: string): string | null => (check ? null : fieldIncompleteReason(spec, t)) ?? messageOf(t);
  /* a box on its own checks only what is typed into it, not the value it is given */
  const typing = focused && !refused && fieldIncomplete(spec, text);
  const typed = (standalone && draft === null) || typing ? null : shownMessageOf(text);
  const message = typed ?? error ?? null;
  /* something Escape drops: a changed draft, or the core's refusal of one (a panel's own Escape
     waits for it: data-own-escape) */
  const edited = standalone && (!!error || (draft !== null && draft.trim() !== started.current));
  const msgId = `${baseId}-msg`;
  const listId = spec.kind === 'name' ? `${baseId}-names` : undefined;

  /* a draft the box committed (on leaving it): once the commit is no longer settling and the
     core sent back no error, the draft has done its job and the plain value (now the same text)
     takes over; while it is settling, or the core refused it, the draft (what was sent) stays on
     screen (WF-001). Only a committed draft: not one being typed, whatever the focus events say
     (a focus the browser never reported must not drop what is typed). */
  const [committed, setCommitted] = useState(false);
  useEffect(() => {
    if (!standalone || !committed || draft === null || settling || error) return;
    setCommitted(false);
    setDraft(null);
  }, [standalone, committed, draft, settling, error]);

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
    setRefused(false);
    setCommitted(false);
    setHint(null);
    if (!standalone) { onInput?.(t); return; }
    setDraft(t);
    clearTimer();
    if (commitAfter !== undefined && messageOf(t.trim()) === null)
      timer.current = setTimeout(() => { timer.current = null; commit(t); }, commitAfter);
  };
  /* the common prefix/suffix of `before` and `after`: `added` is what changed in the middle (a
     keystroke, a paste, a drop, an IME commit) so it can be checked and named on its own; empty
     when nothing was inserted (a plain deletion, always let through) */
  const diffAdded = (before: string, after: string): {prefix: string; added: string; suffix: string} => {
    let p = 0;
    while (p < before.length && p < after.length && before[p] === after[p]) p++;
    let s = 0;
    while (s < before.length - p && s < after.length - p
      && before[before.length - 1 - s] === after[after.length - 1 - s]) s++;
    return {prefix: before.slice(0, p), added: after.slice(p, after.length - s), suffix: before.slice(before.length - s)};
  };
  const spin = (dir: 1 | -1, el: HTMLInputElement) => {
    if (!step || !(step > 0) || (spec.kind !== 'number' && spec.kind !== 'integer')) return false;
    /* from the text last typed, like the commit: an arrow right after a keystroke steps that */
    const now = standalone ? (latest.current ?? value) : value;
    const v = Number(now.trim());
    if (now.trim() === '' || !Number.isFinite(v)) return false;
    let next = v + dir * step;
    const lo = el.min !== '' ? Number(el.min) : NaN, hi = el.max !== '' ? Number(el.max) : NaN;
    if (Number.isFinite(lo)) next = Math.max(lo, next);
    if (Number.isFinite(hi)) next = Math.min(hi, next);
    change(stepped(next, step));
    return true;
  };

  return (
    <>
      <input {...rest} ref={inputRef} id={baseId} type={type} value={text} step={step ?? undefined} inputMode={fieldInputMode(spec)}
        list={listId} spellcheck={spec.kind === 'text' ? undefined : false}
        autocomplete={spec.kind === 'text' || spec.kind === 'file' ? undefined : 'off'}
        aria-invalid={message ? 'true' : undefined}
        aria-describedby={message || hint ? msgId : undefined} data-kind={spec.kind}
        data-own-escape={edited ? '1' : undefined}
        onFocus={e => {
          setFocused(true);
          setCommitted(false);
          if (standalone && latest.current === null) {
            const from = editValue ?? value;
            started.current = from.trim();
            setDraft(from);
          }
          onFocus?.(e);
        }}
        onInput={e => {
          const el = e.target as HTMLInputElement;
          const attempted = el.value;
          const before = latest.current ?? text;
          const {prefix, added, suffix} = diffAdded(before, attempted);
          /* nothing inserted (a deletion): always let it through; an insertion the kind never
             takes, and is not on the way to something it does, is refused outright (T35d): the
             box's text does not change, only a brief hint says why */
          if (added && !fieldAcceptsEdit(spec, attempted)) {
            el.value = before;
            const at = Math.max(0, Math.min(prefix.length, before.length));
            try { el.setSelectionRange(at, at); } catch { /* not every input type supports it */ }
            if (added.length === 1) setHint(fieldCharMessage(spec, prefix, added));
            else {
              const offender = fieldPasteOffender(spec, prefix, added, suffix) ?? {ch: added[0], index: 1};
              setHint(fieldPasteMessage(spec, added, offender));
            }
            return;
          }
          change(attempted);
        }}
        onBlur={e => {
          setFocused(false);
          setHint(null);
          if (standalone) {
            if (dropped.current) { dropped.current = false; clearTimer(); setCommitted(false); setDraft(null); }
            /* the draft is kept, marked, when it is not what the box takes (half-typed) or the
               commit is still settling or the core refused it (WF-001); the effect above drops
               it once a commit is known to have settled cleanly */
            else if (latest.current !== null && commit(latest.current)) setCommitted(true);
          }
          onBlur?.(e);
        }}
        onKeyDown={e => {
          const el = e.target as HTMLInputElement;
          if (e.key === 'Enter') {
            if (messageOf(el.value) !== null) {
              /* a text the box does not take is not committed: it stays, with its message */
              setRefused(true);
              e.preventDefault();
              e.stopPropagation();
              return;
            }
            if (standalone) { e.preventDefault(); el.blur(); }
          } else if (e.key === 'Escape' && standalone) {
            /* the box's own key: it drops the draft (typed, half-typed, settling or refused by
               the core) and never reaches the page's hotkeys or a panel's own Escape (UX-001) */
            e.stopPropagation();
            setHint(null);
            dropped.current = true;
            if (error) onDropError?.();
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
      {/* a refused keystroke/paste/drop (hint) never marks the box invalid: its text did not
          change. It takes priority only while nothing else is shown (a marked box's own message
          says more). */}
      {(hint || message) && <p class="field-error" id={msgId} aria-live="polite">{message || hint}</p>}
    </>
  );
}
