/* A dialog's focus (W20): in on its first field when it opens, back to where
   it was when it closes. One place for what AskDialog, AutoxSettings and
   SliderDialog each did their own way. */
import {useLayoutEffect, useRef, type Inputs} from 'preact/hooks';

/** what the keyboard can reach in a panel or dialog */
export const FOCUSABLE = 'button:not([disabled]), input, select, textarea, [tabindex]:not([tabindex="-1"])';

/** the element a dialog starts on: its [data-autofocus] one, else its first control other than a
    help button (a menu with its "?" first took the focus there, and Enter opened Help) */
export function firstFocus(el: HTMLElement): HTMLElement | null {
  return el.querySelector<HTMLElement>('[data-autofocus]')
    ?? [...el.querySelectorAll<HTMLElement>(FOCUSABLE)].find(e => !e.closest('.help-link, .dialog-close')) ?? null;
}

/** focus the dialog's first field when it opens (and when `deps` change: another ask in the same
    dialog); on close, give the focus back to where it was, unless something else took it
    meanwhile: a dialog that replaces this one has already focused its own first field, and
    taking the focus back from it lost the keys typed into it (macOS's CI runner). A layout effect:
   the focus is in before the page is painted, not after it, when keys typed meanwhile went elsewhere */
export function useDialogFocus(box: {current: HTMLElement | null}, deps: Inputs): void {
  /* where the focus was before the dialog: read while rendering, since Modal's showModal() moves it into the
     dialog before any effect runs (the first run only: a later run is another ask in the same dialog) */
  const opener = useRef<HTMLElement | null | undefined>(undefined);
  if (opener.current === undefined) opener.current = document.activeElement as HTMLElement | null;
  useLayoutEffect(() => {
    const before = opener.current !== undefined ? opener.current : document.activeElement as HTMLElement | null;
    opener.current = undefined;
    const el = box.current;
    const first = el ? firstFocus(el) : null;
    first?.focus();
    if (first instanceof HTMLInputElement) first.select();
    /* a frame later, the focus again if something took it outside (the dialog this one replaces
       giving the focus back as it closes, in an order a slow machine can reverse) */
    const again = requestAnimationFrame(() => {
      const now = document.activeElement;
      if (el && el.isConnected && first && first.isConnected && !(now && el.contains(now))) first.focus();
    });
    return () => {
      cancelAnimationFrame(again);
      /* once the dialog has left the page (the same task, right after this commit): while a modal <dialog> is
         open the page behind it is inert, and a focus() there does nothing. Whatever took the focus meanwhile,
         a dialog that replaces this one, keeps it. */
      queueMicrotask(() => {
        if (el?.isConnected) return; /* only re-run (another ask in the same dialog): its new first field has the focus */
        const now = document.activeElement;
        if (now && now !== document.body) return;
        const target = before?.isConnected && before.getClientRects().length ? before
          : before?.closest<HTMLElement>('[data-top-menu]')?.querySelector<HTMLElement>('summary')
            ?? document.querySelector<HTMLElement>('.menu-toggle, .plot-host');
        target?.focus();
      });
    };
  }, deps);
}

/** Cycle a modal's fields like native Tab, selecting text on arrival. */
export function cycleDialogFocus(e: KeyboardEvent, box: HTMLElement | null): void {
  if (e.key !== 'Tab' || !box) return;
  const all = [...box.querySelectorAll<HTMLElement>(FOCUSABLE)].filter(el => el.getClientRects().length > 0);
  if (!all.length) return;
  const i = all.indexOf(document.activeElement as HTMLElement);
  const next = e.shiftKey ? (i <= 0 ? all.length - 1 : i - 1) : (i === all.length - 1 ? 0 : i + 1);
  e.preventDefault();
  const to = all[next];
  to.focus();
  if (to instanceof HTMLInputElement && /^(text|search|number|url|tel|email|password)?$/.test(to.getAttribute('type') ?? '')) to.select();
  else if (to instanceof HTMLTextAreaElement) to.select();
}
