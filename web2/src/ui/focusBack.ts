/* The focus of a panel that opens over the page (a sheet, the drawer):
   back to the button that opened it when it closes, if the focus was
   inside it.

   A layout effect, run in the same task as the render that drops the
   panel's `open` class, before the browser's next style update. A plain
   effect runs after that update (after the paint), and with
   `prefers-reduced-motion: reduce` (theme.css drops every transition; a
   Windows Server desktop, as CI's windows runner, has it on: W18) the
   update hides the panel at once, and the browser moves the focus inside it
   to <body>: the effect found it gone and left it there. Without reduced
   motion the panel's `visibility 0s 0.2s` transition hid that race. */
import type {RefObject} from 'preact';
import {useLayoutEffect} from 'preact/hooks';

export function useFocusBackOnClose(open: boolean, panel: RefObject<HTMLElement>, toggle: string): void {
  useLayoutEffect(() => {
    if (!open && panel.current?.contains(document.activeElement)) document.querySelector<HTMLElement>(toggle)?.focus();
  }, [open]);
}
