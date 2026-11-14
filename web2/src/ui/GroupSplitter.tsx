/* The bar between the two groups of the split view (W720): drag it, or use the arrow keys, Home and End, to set the first group's share of
   the main area. The tracks follow the pointer at once (ui/split.ts setSplitTracks); the share goes to the core on release
   (window.set_split_share), and a key sends at each press. The core's range is the bar's: neither group goes below it. */
import {useSession} from './context';
import {setSplitTracks, useShareRange, type SplitView} from './split';

/** how far an arrow key moves the bar, as a share of the area (VS Code's sash moves by a fixed step too) */
const SPLIT_KEY_STEP = 0.05;

export function GroupSplitter({split}: {split: SplitView}) {
  const session = useSession();
  const {min, max} = useShareRange();
  const horizontal = split.direction === 'down'; /* the groups are stacked: the bar is horizontal and moves up and down */
  const clamp = (share: number) => Math.min(max, Math.max(min, share));
  const onPointerDown = (e: PointerEvent) => {
    const bar = e.currentTarget as HTMLElement, area = bar.parentElement;
    if (!area || e.button !== 0) return;
    e.preventDefault();
    const box = area.getBoundingClientRect();
    let share = split.share;
    const move = (m: PointerEvent) => {
      share = clamp(horizontal ? (m.clientY - box.top) / box.height : (m.clientX - box.left) / box.width);
      setSplitTracks(share);
    };
    const stop = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', end);
      window.removeEventListener('pointercancel', stop);
    };
    const end = () => {
      stop();
      if (share !== split.share) session.setSplitShare(share);
    };
    /* on the window, so the drag follows the pointer wherever it goes */
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', end);
    window.addEventListener('pointercancel', stop);
  };
  const onKeyDown = (e: KeyboardEvent) => {
    const before = horizontal ? 'ArrowUp' : 'ArrowLeft', after = horizontal ? 'ArrowDown' : 'ArrowRight';
    const next = e.key === before ? split.share - SPLIT_KEY_STEP : e.key === after ? split.share + SPLIT_KEY_STEP
      : e.key === 'Home' ? min : e.key === 'End' ? max : null;
    if (next === null) return;
    e.preventDefault();
    e.stopPropagation(); /* not an XPP hotkey */
    session.setSplitShare(clamp(next));
  };
  return <div class="group-splitter" role="separator" tabIndex={0} aria-orientation={horizontal ? 'horizontal' : 'vertical'}
    aria-label="Size of the first group" aria-valuemin={Math.round(min * 100)} aria-valuemax={Math.round(max * 100)}
    aria-valuenow={Math.round(split.share * 100)} aria-valuetext={`${Math.round(split.share * 100)} percent`}
    onPointerDown={onPointerDown} onKeyDown={onKeyDown} />;
}
