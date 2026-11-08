/* The focus of a panel that opens over the page (a sheet, the drawer):
   back to the button that opened it when it closes, if the focus was
   inside it (for a button in a menu that has closed, the menu's summary:
   the Windows menu's title for Open animation and Open array plot).

   A layout effect, run in the same task as the render that drops the
   panel's `open` class, before the browser's next style update. A plain
   effect runs after that update (after the paint), and with
   `prefers-reduced-motion: reduce` (css/layout.css drops every transition; a
   Windows Server desktop, as CI's windows runner, has it on: W18) the
   update hides the panel at once, and the browser moves the focus inside it
   to <body>: the effect found it gone and left it there. Without reduced
   motion the panel's `visibility 0s 0.2s` transition hid that race. */
import type {RefObject} from 'preact';
import {useLayoutEffect} from 'preact/hooks';

export function useFocusBackOnClose(open: boolean, panel: RefObject<HTMLElement>, toggle: string): void {
  useLayoutEffect(() => {
    if (open || !panel.current?.contains(document.activeElement)) return;
    /* the one the user can see: a phone's title bar hides Data, Model and Help, and the menu bar has them; a closed
       drawer is hidden by its visibility, which keeps its layout (its items still have boxes) */
    const buttons = [...document.querySelectorAll<HTMLElement>(toggle)];
    const seen = (b: HTMLElement) => b.getClientRects().length > 0 && getComputedStyle(b).visibility !== 'hidden';
    const button = buttons.find(seen) ?? buttons[0];
    /* a button inside a menu is hidden once the menu closed on its choice: the focus goes to the menu's
       own title, the nearest thing the user can see (a hidden element takes none) */
    const menu = button?.closest('details');
    (menu && !menu.open ? menu.querySelector<HTMLElement>('summary') : button)?.focus();
  }, [open]);
}
