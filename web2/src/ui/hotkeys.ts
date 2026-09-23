/* XPP's hotkeys: a key typed while the focus is on the page or the plot
   (not on a control, a field or a dialog) is sent as the protocol's key
   name (DOM KeyboardEvent.key). Tab is never taken: it moves the focus. */
import {useEffect} from 'preact/hooks';
import type {Session} from '../session';

const NAMED = new Set(['Escape', 'Enter', 'Backspace', 'Delete', 'Home', 'End', 'ArrowLeft', 'ArrowRight',
  'ArrowUp', 'ArrowDown', 'PageUp', 'PageDown']);
const CONTROLS = 'button, a, input, textarea, select, summary, [contenteditable], [role="dialog"], [role="menu"]';

export function hotkey(e: KeyboardEvent): string | null {
  if (e.ctrlKey || e.metaKey || e.altKey) return null;
  return e.key.length === 1 || NAMED.has(e.key) ? e.key : null;
}

export function useHotkeys(session: Session): void {
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.defaultPrevented || session.store.getState().ask) return;
      const t = e.target as HTMLElement | null;
      if (t?.closest?.(CONTROLS)) return;
      const k = hotkey(e);
      if (!k) return;
      e.preventDefault();
      session.key(k);
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [session]);
}
