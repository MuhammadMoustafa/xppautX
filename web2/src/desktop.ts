/* What the desktop window's native menus call (core/xpp_window.cpp, W13a):
   Help > Manual and Help > Keyboard shortcuts run
   `window.__xppOpenHelp(chapter?, anchor?)` in the page. It opens the Help
   view as F1 does: with no chapter where it was left, else at that
   chapter (and anchor). Its close box and File > Quit (Cmd+Q on macOS) run
   `window.__xppQuit()` (W110): File > Quit's question (session.ts
   quitAsked). In a browser nothing calls them. */
import {requestUpdateCheck} from './help/updates';
import type {Session} from './session';
import {nativeFileDialog} from './pickers';

export function installDesktopHooks(session: Session): void {
  const w = window as unknown as {__xppOpenHelp: unknown; __xppQuit: unknown; __xppCheckUpdates: unknown};
  w.__xppOpenHelp = (chapter?: string, anchor?: string) =>
    session.store.dispatch({type: 'help', action: {type: 'open', target: chapter ? {chapter, anchor} : undefined}});
  w.__xppCheckUpdates = requestUpdateCheck;
  w.__xppQuit = () => session.quitAsked();
}

/** The desktop window closes (its `__xppCloseWindow`, bound by
    core/xpp_window.cpp, W110): the leave question's Don't save there. The
    window then sends the core the plain quit, and ends the process even
    when a computation never reaches a checkpoint. False in a browser, which
    has no such window. */
export function closeDesktopWindow(): boolean {
  const close = (window as unknown as {__xppCloseWindow?: () => Promise<unknown>}).__xppCloseWindow;
  if (typeof close !== 'function') return false;
  void close().catch(() => undefined);
  return true;
}

/** Browser mode only (the desktop window binds `__xppFileDialog` and closes
    through its own path, xpp_window.cpp). Closing the tab loses the session,
    so the browser's own "Leave site?" comes first; and when the page goes
    away it tells the core with a beacon (POST /leave, docs/protocol.md), which
    then waits about 2 s for a page to reconnect (a reload) before it exits,
    instead of the watchdog's 10 s. */
export function installBrowserLeave(token: string = location.search, base = '/'): void {
  if (nativeFileDialog()) return;
  window.addEventListener('beforeunload', e => {
    e.preventDefault();
    e.returnValue = '';
  });
  window.addEventListener('pagehide', () => {
    try {
      navigator.sendBeacon(`${base}leave${token}`);
    } catch {
      /* no beacon: the watchdog's heartbeat finds the closed page */
    }
  });
}
