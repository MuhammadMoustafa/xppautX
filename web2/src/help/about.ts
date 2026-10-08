/* Help > About: hello.about (core/xpp_about.h) is lines of parts, a part plain
   text or a link (`url`); the view draws them and a click on a link opens it. */
import type {AboutLine} from '../protocol/types';

/** the first line's text, "xppautX <version>": what the update check compares */
export function aboutVersionLine(about: AboutLine[]): string {
  return about[0]?.map(p => p.text).join('') ?? '';
}

/** Open an About link in the system's browser. The desktop window binds
    `__xppOpenAboutLink` (core/xpp_window.cpp), which opens only the exact
    addresses the core's About lists; in a browser the anchor's own
    target=_blank does it and this is not called. False in a browser. */
export function openAboutLink(url: string): boolean {
  const bound = (window as unknown as {__xppOpenAboutLink?: (url: string) => Promise<unknown>}).__xppOpenAboutLink;
  if (typeof bound !== 'function') return false;
  void bound(url).catch(() => undefined);
  return true;
}
