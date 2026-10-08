/* The draggable inner edge of a side panel (W226): a separator that resizes the panel it controls by
   pointer or arrow keys, within store/panels.ts's limits. */
import {useEffect, useState} from 'preact/hooks';
import {PANEL_LIMITS, panelRange, SPLITTER_STEP, type PanelSize} from '../store/panels';
import {useSession, useStore} from './context';

/** whether the window is at least `rem` wide (follows a resize) */
export function useMinWidth(rem: number): boolean {
  const query = `(min-width: ${rem}rem)`;
  const [on, setOn] = useState(() => window.matchMedia(query).matches);
  useEffect(() => {
    const m = window.matchMedia(query), set = () => setOn(m.matches);
    set();
    m.addEventListener('change', set);
    return () => m.removeEventListener('change', set);
  }, [query]);
  return on;
}

interface Props {
  /** the panel's id */
  controls: string;
  size: PanelSize;
  /** the separator's own class, the stylesheet places it on the panel's inner edge */
  className: string;
  /** a vertical separator resizes the width, a horizontal one the height */
  orientation: 'vertical' | 'horizontal';
  /** +1 when the pointer moving right (vertical) or down (horizontal) grows the panel, -1 when it shrinks it */
  growth: 1 | -1;
}

export function Splitter({controls, size, className, orientation, growth}: Props) {
  const session = useSession();
  const stored = useStore(s => s.panels[size]);
  const [now, setNow] = useState(0);
  const vertical = orientation === 'vertical';
  const panel = () => document.getElementById(controls);
  const extent = (el: Element) => Math.round(vertical ? el.getBoundingClientRect().width : el.getBoundingClientRect().height);
  const range = () => panelRange(size, vertical ? window.innerWidth : window.innerHeight);
  const set = (px: number) => {
    const {min, max} = range();
    session.store.dispatch({type: 'panels', panels: {[size]: Math.min(max, Math.max(min, Math.round(px)))}});
  };
  /* the size on screen, whatever set it: the stored one, the stylesheet's default or the window's limit */
  useEffect(() => {
    const el = panel();
    if (!el) return;
    const ro = new ResizeObserver(() => setNow(extent(el)));
    ro.observe(el);
    setNow(extent(el));
    return () => ro.disconnect();
  }, [controls, vertical, stored]);

  const onPointerDown = (e: PointerEvent) => {
    const el = panel();
    if (!el || e.button !== 0) return;
    e.preventDefault();
    const start = vertical ? e.clientX : e.clientY, startSize = extent(el);
    const move = (m: PointerEvent) => set(startSize + growth * ((vertical ? m.clientX : m.clientY) - start));
    const stop = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', stop);
      window.removeEventListener('pointercancel', stop);
    };
    /* on the window, so the drag follows the pointer wherever it goes */
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', stop);
    window.addEventListener('pointercancel', stop);
  };
  const onKeyDown = (e: KeyboardEvent) => {
    const {min, max} = range();
    const [shrink, grow] = vertical ? ['ArrowLeft', 'ArrowRight'] : ['ArrowUp', 'ArrowDown'];
    const sign = growth;
    let next: number | null = null;
    if (e.key === grow) next = now + sign * SPLITTER_STEP;
    else if (e.key === shrink) next = now - sign * SPLITTER_STEP;
    else if (e.key === 'Home') next = min;
    else if (e.key === 'End') next = max;
    if (next === null) return;
    e.preventDefault();
    set(next);
  };
  const {min, max} = range();
  return <div class={'splitter ' + className} role="separator" tabIndex={0} aria-orientation={orientation}
    aria-label={PANEL_LIMITS[size].label} aria-controls={controls}
    aria-valuemin={min} aria-valuemax={max} aria-valuenow={Math.min(max, Math.max(min, now))}
    onPointerDown={onPointerDown} onKeyDown={onKeyDown} />;
}
