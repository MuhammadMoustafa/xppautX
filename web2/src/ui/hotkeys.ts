/* XPP's hotkeys: a key typed while the focus is on the page, the plot or a
   button (not in a text field, a select, a dialog or a menu) is sent as the
   protocol's key name (DOM KeyboardEvent.key), through session.typeKey,
   which lets a key typed before the menu of the one before it arrives
   answer that menu (typing I then G quickly integrates). A button keeps
   its own Enter, Space and arrows: only letters, digits, signs and Escape
   typed on it are hotkeys (docs/ui-v2.md T21: after a click the focus stays on the
   button, and F or G typed then must still act). Tab is never taken: it
   moves the focus. */
import {useEffect} from 'preact/hooks';
import type {Session} from '../session';

/** Visible work areas cycle without walking every field or command. */
function focusPane(backwards: boolean): void {
  const panes = ['#command-search', '.plot-host', '#values-panel', '#table-panel', '#text-panel']
    .map(selector => document.querySelector<HTMLElement>(selector))
    .filter((pane): pane is HTMLElement => !!pane && pane.getClientRects().length > 0 && getComputedStyle(pane).visibility !== 'hidden');
  if (!panes.length) return;
  const current = panes.findIndex(pane => pane === document.activeElement || pane.contains(document.activeElement));
  const next = current < 0 ? backwards ? panes.length - 1 : 0 : (current + (backwards ? -1 : 1) + panes.length) % panes.length;
  const pane = panes[next];
  if (!pane.matches('input, button, [tabindex]')) pane.tabIndex = -1;
  pane.focus();
}

const NAMED = new Set(['Escape', 'Enter', 'Backspace', 'Delete', 'Home', 'End', 'ArrowLeft', 'ArrowRight',
  'ArrowUp', 'ArrowDown', 'PageUp', 'PageDown']);
/** where typing is the element's own: text, lists, dialogs and menus */
const TYPING = 'input, textarea, select, [contenteditable], [role="dialog"], [role="menu"], [role="listbox"]';
/** controls that keep their own named keys (Enter, Space, the arrows) but not letters */
const BUTTONS = 'button, a, summary, [role="button"], [role="tab"], [role="slider"]';

export function hotkey(e: KeyboardEvent): string | null {
  if (e.ctrlKey || e.metaKey || e.altKey) return null;
  return e.key.length === 1 || NAMED.has(e.key) ? e.key : null;
}

/** whether a key typed on `target` is XPP's (a printable one on a button is; nothing typed in a field is) */
export function isHotkeyTarget(target: Element | null, key: string): boolean {
  if (!target?.closest) return true;
  if (target.closest(TYPING)) return false;
  if (target.closest(BUTTONS)) return (key.length === 1 && key !== ' ') || key === 'Escape';
  return true;
}

export function useHotkeys(session: Session): void {
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.defaultPrevented) return;
      if ((e.target as Element | null)?.closest('[role="dialog"], [aria-modal="true"]')) return;
      const modifier = e.ctrlKey || e.metaKey;
      if (modifier && !e.altKey && !e.shiftKey && ['o', 's', 'k'].includes(e.key.toLowerCase())) {
        const key = e.key.toLowerCase();
        e.preventDefault();
        if (key === 'k') {
          session.store.dispatch({type: 'drawer', open: true});
          document.querySelector<HTMLInputElement>('#command-search')?.focus();
        } else session.menuAction('file', key === 'o' ? 'openmodel' : 'savesession');
        return;
      }
      if (e.key === 'F6') {
        e.preventDefault();
        focusPane(e.shiftKey);
        return;
      }
      const k = hotkey(e);
      if (!k || !isHotkeyTarget(e.target as Element | null, k)) return;
      e.preventDefault();
      session.typeKey(k);
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [session]);
}
