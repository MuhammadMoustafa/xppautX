/* The one display of text that may be cut (W640): a read-only number cut to six digits (store/values.ts
   shownNumber) and a name too long for its place. Cut text says so: a muted … (for a number, before the
   exponent) and a dotted underline. Its full value or name is in a one-line tip, with a clipboard icon, on
   hover or focus; a click or tap, or Enter, copies it, the confirmation being a green tint and the icon
   turning into a green tick, no word. The tip is a `popover` (non-modal), drawn in the browser's top layer like
   ui/Modal.tsx's dialogs, so no panel clips it. Text that is not cut is plain: nothing to say, nothing to copy.
   Input boxes are never shortened; they are Field.tsx's. */
import type {ComponentChildren} from 'preact';
import {useEffect, useLayoutEffect, useRef, useState} from 'preact/hooks';
import {shownNumber} from '../store/values';
import {useSession} from './context';

/** how long the green tick stays, ms (long enough to be seen, short enough not to hide the next click's) */
const COPIED_MS = 1500;
/** the gap between the text and its tip, and between the tip and the window's edge, px */
const TIP_GAP = 6;

const ICON = {width: 16, height: 16, viewBox: '0 0 24 24', fill: 'none', stroke: 'currentColor', 'stroke-width': 2,
  'stroke-linecap': 'round' as const, 'stroke-linejoin': 'round' as const, 'aria-hidden': 'true' as const};

function Tip({host, full, copied}: {host: HTMLElement; full: string; copied: boolean}) {
  const tip = useRef<HTMLSpanElement>(null);
  /* placed under the text (over it when there is no room), kept inside the window, before it is painted */
  useLayoutEffect(() => {
    const el = tip.current;
    if (!el) return;
    el.showPopover();
    const at = host.getBoundingClientRect(), box = el.getBoundingClientRect();
    const below = at.bottom + TIP_GAP + box.height <= innerHeight || at.top - TIP_GAP - box.height < 0;
    el.style.top = `${below ? at.bottom + TIP_GAP : at.top - TIP_GAP - box.height}px`;
    el.style.left = `${Math.max(TIP_GAP, Math.min(at.left, innerWidth - box.width - TIP_GAP))}px`;
  }, [host, full]);
  return (
    <span class={'cut-tip' + (copied ? ' copied' : '')} popover="manual" role="tooltip" ref={tip}>
      <span class="cut-tip-text">{full}</span>
      {copied
        ? <svg {...ICON} class="cut-tick"><path d="M20 6 9 17l-5-5" /></svg>
        : <svg {...ICON} class="cut-icon"><rect x="9" y="9" width="12" height="12" rx="2" /><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1" /></svg>}
    </span>
  );
}

interface Props {
  /** the whole value or name, what the tip shows and a click copies */
  full: string;
  /** whether it is cut; undefined: measured (a name is cut where its box ends) */
  cut?: boolean;
  /** false where the click has another job (a legend entry toggles its curve): the tip only */
  copy?: boolean;
  /** the tip's open state when its owner tracks the pointer and focus itself (the axis title's button, W646):
      the text's own pointer and focus then do not open it */
  tipped?: boolean;
  class?: string;
  children: ComponentChildren;
}

export function CutText({full, cut, copy = true, tipped: owned, class: className, children}: Props) {
  const session = useSession();
  const host = useRef<HTMLSpanElement>(null);
  const [measured, setMeasured] = useState(false);
  const [ownTip, setTip] = useState(false);
  const tipped = owned ?? ownTip;
  const [copied, setCopied] = useState(false);
  const timer = useRef<ReturnType<typeof setTimeout> | null>(null);
  useEffect(() => () => { if (timer.current) clearTimeout(timer.current); }, []);
  useEffect(() => {
    const el = host.current;
    if (cut !== undefined || !el) return;
    /* a vertical title (writing-mode, the y axis) is cut along its block's height */
    const vertical = getComputedStyle(el).writingMode.startsWith('vertical');
    const look = () => setMeasured(vertical ? el.scrollHeight > el.clientHeight : el.scrollWidth > el.clientWidth);
    const watch = new ResizeObserver(look);
    watch.observe(el);
    look();
    return () => watch.disconnect();
  }, [cut, full]);
  const isCut = cut ?? measured;
  const doCopy = async (e: Event) => {
    e.preventDefault(); /* a name inside a label does not move the focus to its field */
    try {
      await navigator.clipboard.writeText(full);
    } catch (err) {
      session.store.dispatch({type: 'bottom', text: `Could not copy "${full}": ${err instanceof Error ? err.message : String(err)}`});
      return;
    }
    setCopied(true);
    if (timer.current) clearTimeout(timer.current);
    timer.current = setTimeout(() => { timer.current = null; setCopied(false); }, COPIED_MS);
  };
  const base = 'cut-text' + (className ? ' ' + className : '');
  if (!isCut) return <span class={base} ref={host}>{children}</span>;
  return (
    <span class={base + ' cut' + (copied ? ' copied' : '')} ref={host} tabIndex={copy ? 0 : undefined} role={copy ? 'button' : undefined}
      aria-label={copy ? `${full}, click to copy` : full}
      onPointerEnter={() => setTip(true)} onPointerLeave={() => setTip(false)}
      onFocus={() => setTip(true)} onBlur={() => setTip(false)}
      onClick={copy ? doCopy : undefined}
      onKeyDown={copy ? e => { if (e.key === 'Enter' || e.key === ' ') void doCopy(e); } : undefined}>
      {children}
      {tipped && host.current && <Tip host={host.current} full={full} copied={copied} />}
      {copied && <span class="visually-hidden" role="status">Copied</span>}
    </span>
  );
}

/** a read-only number: six significant digits, the … before the exponent when digits were dropped */
export function ShownNumber({value, class: className}: {value: number; class?: string}) {
  const {head, tail, cut, full} = shownNumber(value);
  return (
    <CutText full={full} cut={cut} class={className}>
      {head}{cut && <span class="cut-more" aria-hidden="true">…</span>}{tail}
    </CutText>
  );
}

/** a name that never wraps: cut with … where its box ends, the whole name in the tip */
export function ShownName({name, copy, tipped, class: className}: {name: string; copy?: boolean; tipped?: boolean; class?: string}) {
  return <CutText full={name} copy={copy} tipped={tipped} class={'cut-name' + (className ? ' ' + className : '')}>{name}</CutText>;
}
