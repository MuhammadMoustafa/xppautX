/* A context menu at a point (the pointer's, or the keyboard's menu key): a modal of role=menu over the page, its
   enabled items one button each. A press outside it or Escape closes it, the arrow keys move among its enabled
   items. The caller says what closing does (`close(escaped)`: Escape tells it so, for the focus to go back). Used
   by the pinned commands (QuickAccess) and the view tabs (ViewTabs), so one menu behaves the same in both. */
import {useLayoutEffect, useRef} from 'preact/hooks';
import {Modal} from './Modal';

export interface MenuItem {label: string; off?: boolean; run(): void}

export function ContextMenu({x, y, label, items, close}: {x: number; y: number; label: string; items: MenuItem[]; close(escaped: boolean): void}) {
  const box = useRef<HTMLDivElement>(null);
  useLayoutEffect(() => {
    box.current?.querySelector<HTMLElement>('button')?.focus();
    const away = (e: PointerEvent) => { if (!box.current?.contains(e.target as Node)) close(false); };
    document.addEventListener('pointerdown', away, true);
    return () => document.removeEventListener('pointerdown', away, true);
  }, []);
  return <Modal clear><div class="quick-menu" role="menu" aria-label={label} ref={box} style={{left: `${x}px`, top: `${y}px`}}
    onKeyDown={e => {
      const buttons = [...e.currentTarget.querySelectorAll<HTMLButtonElement>('button:not([aria-disabled="true"])')];
      const i = buttons.indexOf(document.activeElement as HTMLButtonElement);
      if (e.key === 'Escape') { e.preventDefault(); e.stopPropagation(); close(true); }
      else if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
        e.preventDefault();
        buttons[(i + (e.key === 'ArrowDown' ? 1 : -1) + buttons.length) % buttons.length]?.focus();
      }
    }}>
    {items.map(item => <button type="button" role="menuitem" key={item.label} aria-disabled={!!item.off}
      onClick={() => { if (!item.off) { close(false); item.run(); } }}>{item.label}</button>)}
  </div></Modal>;
}
