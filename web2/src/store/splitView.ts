/* Whether the core's split view is on screen (W720): the split is the core's and is kept when the screen is too narrow for it; below the
   phone layout's width (docs/ui-v2.md R3) the main area is one strip and one plot. */
import type {AppState} from './state';

/** the phone layout's width: below it no split is drawn */
export const PHONE_QUERY = '(max-width: 47.99rem)';

/** the split is drawn: there is one and the screen is not a phone's */
export function splitDrawn(s: Pick<AppState, 'split'>): boolean {
  return s.split.direction !== 'none' && !window.matchMedia(PHONE_QUERY).matches;
}
