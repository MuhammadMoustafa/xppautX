/* The split view of the main area (W720, docs/ui-v2.md R6): what is on screen of the core's split (the `windows` event's `split`), shared
   by the tab strips (ui/ViewTabs.tsx) and the plots (ui/Plots.tsx). The split is the core's: it is kept when the screen is too narrow
   for it, and drawn again when it is wide enough. */
import {useEffect, useState} from 'preact/hooks';
import type {SplitInfo} from '../protocol/types';
import {PHONE_QUERY} from '../store/splitView';
import type {AppState} from '../store/state';
import {useStore} from './context';

export function useNarrow(): boolean {
  const [narrow, setNarrow] = useState(() => window.matchMedia(PHONE_QUERY).matches);
  useEffect(() => {
    const query = window.matchMedia(PHONE_QUERY);
    const change = () => setNarrow(query.matches);
    query.addEventListener('change', change);
    change();
    return () => query.removeEventListener('change', change);
  }, []);
  return narrow;
}

export interface SplitView extends SplitInfo {
  direction: 'right' | 'down';
}

/** the split as it is drawn: null when there is none or the screen is a phone's (one strip with every tab, one plot) */
export function useSplitView(): SplitView | null {
  const split = useStore(s => s.split);
  const narrow = useNarrow();
  return split.direction === 'none' || narrow ? null : {...split, direction: split.direction};
}

/** the view a group's strip and plot show: the core's shown window of the group (a graph or an autoX view), else the group's first one that
    is not hidden */
export function shownInGroup(s: AppState, group: 'first' | 'second'): string {
  const named = s.split.shown[group];
  const own = s.windows.filter(w => (w.kind === 'graph' || w.kind === 'diagram') && w.group === group && (w.shown || w.name === s.split.selected));
  return own.some(w => w.name === named) ? named : own[0]?.name ?? named;
}

/** `share` (the first group's, 0 to 1) as the two grid tracks' sizes, in the page's custom properties for the strips and the plots to
    share while the splitter is dragged (ui/GroupSplitter.tsx sets them as well) */
export const SPLIT_FIRST = '--split-first', SPLIT_SECOND = '--split-second';
/** the tracks are proportions: whole numbers keep their sum a whole */
const SHARE_UNITS = 1000;
export function setSplitTracks(share: number): void {
  document.documentElement.style.setProperty(SPLIT_FIRST, `${Math.round(share * SHARE_UNITS)}fr`);
  document.documentElement.style.setProperty(SPLIT_SECOND, `${SHARE_UNITS - Math.round(share * SHARE_UNITS)}fr`);
}

/** the core's share kept in the tracks, once for the page */
export function useSplitTracks(): void {
  const share = useStore(s => s.split.share);
  useEffect(() => setSplitTracks(share), [share]);
}

/** the range the core accepts for the first group's share (hello's window.set_split_share), for the splitter's bounds */
export function useShareRange(): {min: number; max: number} {
  const bound = (key: 'min' | 'max', none: number) => useStore(s => {
    const arg = s.hello?.commands.find(c => c.cmd === 'window.set_split_share')?.arguments?.find(a => a.name === 'share');
    return typeof arg?.[key] === 'number' ? arg[key] : none;
  });
  return {min: bound('min', 0), max: bound('max', 1)};
}
