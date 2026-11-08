/* The draggable inner edge of a side panel (W226): a separator that resizes the panel it controls by
   pointer or arrow keys, within store/panels.ts's limits. */
import {useEffect, useState} from 'preact/hooks';
import {PANEL_LIMITS, panelRange, SPLITTER_STEP, type PanelSize} from '../store/panels';
import {useSession, useStore} from './context';

interface Props {
  /** the panel's id */
  controls: string;
  size: PanelSize;
  /** the separator's own class, the stylesheet places it on the panel's inner edge */
  className: string;
  /** +1 when the pointer moving right grows the panel, -1 when it shrinks it */
  growth: 1 | -1;
}

export function Splitter({controls, size, className, growth}: Props) {
  const session = useSession();
  const stored = useStore(s => s.panels[size]);
  const [now, setNow] = useState(0);
  const panel = () => document.getElementById(controls);
  const extent = (el: Element) => Math.round(el.getBoundingClientRect().width);
  const range = () => panelRange(size, window.innerWidth);
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
  }, [controls, stored]);

  const onPointerDown = (e: PointerEvent) => {
    const el = panel();
    if (!el || e.button !== 0) return;
    e.preventDefault();
    const start = e.clientX, startSize = extent(el);
    const move = (m: PointerEvent) => set(startSize + growth * (m.clientX - start));
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
        let next: number | null = null;
    if (e.key === 'ArrowRight') next = now + growth * SPLITTER_STEP;
    else if (e.key === 'ArrowLeft') next = now - growth * SPLITTER_STEP;
    else if (e.key === 'Home') next = min;
    else if (e.key === 'End') next = max;
    if (next === null) return;
    e.preventDefault();
    set(next);
  };
  const {min, max} = range();
  return <div class={'splitter ' + className} role="separator" tabIndex={0} aria-orientation="vertical"
    aria-label={PANEL_LIMITS[size].label} aria-controls={controls}
    aria-valuemin={min} aria-valuemax={max} aria-valuenow={Math.min(max, Math.max(min, now))}
    onPointerDown={onPointerDown} onKeyDown={onKeyDown} />;
}
