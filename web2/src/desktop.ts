/* What the desktop window's native menus call (core/xpp_window.cpp, W13a):
   Help > Manual and Help > Keyboard shortcuts run
   `window.__xppOpenHelp(chapter?, anchor?)` in the page. It opens the Help
   view as F1 does: with no chapter where it was left, else at that
   chapter (and anchor). In a browser nothing calls it. */
import type {Session} from './session';

export function installDesktopHooks(session: Session): void {
  (window as unknown as {__xppOpenHelp: unknown}).__xppOpenHelp = (chapter?: string, anchor?: string) =>
    session.store.dispatch({type: 'help', action: {type: 'open', target: chapter ? {chapter, anchor} : undefined}});
}
