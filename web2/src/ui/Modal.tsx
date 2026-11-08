/* The one owner of a modal dialog and of a popup menu (W632): the browser's top layer. A `<dialog>`
   opened with showModal() is drawn above everything on the page, whatever stacking context or
   container (a panel's z-index, the plot's `container-type`) it sits in, its backdrop dims and takes
   the clicks of the whole page, the menu bar included, and the rest of the page is inert while it is
   open. Escape stays the dialog's own handler (useDialogFocus / each dialog's onKeyDown), never the
   browser's close. The alternative, a portal into one overlay root, needs preact/compat, which
   rewrites every input's and textarea's events page-wide (the recording's note and the animation's
   slider changed behaviour), or a hand-made one that loses the context the hooks read. */
import type {ComponentChildren} from 'preact';
import {useLayoutEffect, useRef} from 'preact/hooks';

/** a modal dialog's backdrop over the whole page; a press on the backdrop itself (not on the dialog)
    calls `onBackdrop`. `clear`: a popup menu's, which dims nothing */
export function Modal({onBackdrop, clear, children}: {onBackdrop?(e: PointerEvent): void; clear?: boolean; children: ComponentChildren}) {
  const box = useRef<HTMLDialogElement>(null);
  /* a layout effect: open before the page is painted, and before the dialog's own focus effect, which runs after it */
  useLayoutEffect(() => { box.current?.showModal(); }, []);
  return <dialog class={'dialog-backdrop' + (clear ? ' clear' : '')} ref={box} onCancel={e => e.preventDefault()}
    onPointerDown={e => { if (onBackdrop && e.target === e.currentTarget) onBackdrop(e); }}>{children}</dialog>;
}
