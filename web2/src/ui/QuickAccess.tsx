/* The pinned commands of the quick-access toolbar (docs/command-design.md "Quick access"), after the
   fixed run buttons: the user's own commands (keymap.pinned) in order, each with its label, dragged to
   reorder, with a context menu (Unpin, Move earlier, Move later: also the keyboard's way, Shift+F10 or
   the menu key). Those that do not fit the row go into a "More" menu, never away. Every change is the
   `keymap` command's `set`, so the page keeps no second copy of the pins. */
import {useLayoutEffect, useRef, useState} from 'preact/hooks';
import {menuCommand} from '../protocol/kinds';
import type {CommandRow} from '../protocol/kinds';
import {effectiveKeys, movePinned, pinnedRows, togglePinned, visibleCount, withPinned} from '../store/keymap';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';

/** the gap between the buttons of the row (theme.css .quick-pinned) in px, which the fit counts with */
const GAP_PX = 4;
/** the "More" button's width before it has been drawn, in px */
const MORE_PX = 64;

export function QuickAccess() {
  const session = useSession();
  const may = useMay();
  const hello = useStore(s => s.hello);
  const keymap = useStore(s => s.keymap.info);
  const rows = pinnedRows(keymap, hello?.command_table ?? []);
  const box = useRef<HTMLDivElement>(null);
  const [shown, setShown] = useState(rows.length);
  const [menu, setMenu] = useState<{id: string; x: number; y: number} | null>(null);
  const dragged = useRef<string | null>(null);
  const ids = rows.map(r => r.id).join(' ');

  /* how many fit: the buttons that do not are drawn invisible (still measurable) and listed in More */
  useLayoutEffect(() => {
    const el = box.current;
    if (!el) return;
    const fit = () => {
      const widths = [...el.querySelectorAll<HTMLElement>('[data-pinned]')].map(b => b.offsetWidth);
      const more = el.querySelector<HTMLElement>('.quick-more')?.offsetWidth || MORE_PX;
      setShown(visibleCount(widths, el.clientWidth, more, GAP_PX));
    };
    fit();
    const watch = new ResizeObserver(fit);
    watch.observe(el);
    return () => watch.disconnect();
  }, [ids]);

  if (!rows.length || !keymap) return null;
  const pinned = keymap.pinned;
  const save = (next: string[]) => session.setKeymap(withPinned(keymap, next));
  const run = (row: CommandRow) => session.menuAction(row.menu, row.id);
  const dropOn = (target: string) => {
    const id = dragged.current;
    dragged.current = null;
    if (id && id !== target) save(movePinned(pinned, id, pinned.indexOf(target)));
  };
  const button = (row: CommandRow, hidden: boolean) => {
    const off = !may(menuCommand(row.menu, row.id));
    const keys = effectiveKeys(keymap, row).join(' / ');
    return <button type="button" class={'quick-button' + (hidden ? ' quick-over' : '')} data-pinned={row.id} draggable
      aria-disabled={off} aria-haspopup="menu" aria-keyshortcuts={effectiveKeys(keymap, row)[0]?.replace('Ctrl', 'Control')}
      title={off ? BUSY_TITLE : keys ? `${row.description} (${keys}). Right-click for more.` : `${row.description}. Right-click for more.`}
      onClick={() => { if (!off) run(row); }}
      onContextMenu={e => { e.preventDefault(); setMenu({id: row.id, x: e.clientX, y: e.clientY}); }}
      onDragStart={() => { dragged.current = row.id; }} onDragOver={e => e.preventDefault()}
      onDrop={e => { e.preventDefault(); dropOn(row.id); }}>{row.label}</button>;
  };
  const overflow = rows.slice(shown);
  return <div class="quick-access" role="toolbar" aria-label="Pinned commands" ref={box}>
    <div class="quick-pinned">{rows.map((row, i) => button(row, i >= shown))}</div>
    {overflow.length > 0 && <details class="quick-more">
      <summary aria-label={`More pinned commands (${overflow.length})`}>More</summary>
      <ul class="quick-more-list">{overflow.map(row => <li key={row.id}>
        <button type="button" data-pinned-more={row.id} aria-disabled={!may(menuCommand(row.menu, row.id))}
          onClick={e => {
            (e.currentTarget.closest('details') as HTMLDetailsElement).open = false;
            if (may(menuCommand(row.menu, row.id))) run(row);
          }}
          onContextMenu={e => { e.preventDefault(); setMenu({id: row.id, x: e.clientX, y: e.clientY}); }}>{row.label}</button></li>)}</ul>
    </details>}
    {menu && <PinMenu id={menu.id} x={menu.x} y={menu.y} pinned={pinned} close={() => setMenu(null)} save={save} />}
  </div>;
}

/** the context menu of one pinned command: Unpin, and Move earlier / later (the keyboard's way to reorder) */
function PinMenu({id, x, y, pinned, close, save}: {id: string; x: number; y: number; pinned: string[]; close(): void; save(next: string[]): void}) {
  const box = useRef<HTMLDivElement>(null);
  const at = pinned.indexOf(id);
  useLayoutEffect(() => {
    box.current?.querySelector<HTMLElement>('button')?.focus();
    const away = (e: PointerEvent) => { if (!box.current?.contains(e.target as Node)) close(); };
    document.addEventListener('pointerdown', away, true);
    return () => document.removeEventListener('pointerdown', away, true);
  }, []);
  const choose = (next: string[]) => { close(); save(next); };
  const items: {label: string; off?: boolean; run(): void}[] = [
    {label: 'Unpin', run: () => choose(togglePinned(pinned, id))},
    {label: 'Move earlier', off: at <= 0, run: () => choose(movePinned(pinned, id, at - 1))},
    {label: 'Move later', off: at >= pinned.length - 1, run: () => choose(movePinned(pinned, id, at + 1))}];
  return <div class="quick-menu" role="menu" aria-label="Pinned command" ref={box} style={{left: `${x}px`, top: `${y}px`}}
    onKeyDown={e => {
      const buttons = [...e.currentTarget.querySelectorAll<HTMLButtonElement>('button:not([aria-disabled="true"])')];
      const i = buttons.indexOf(document.activeElement as HTMLButtonElement);
      if (e.key === 'Escape') { e.preventDefault(); e.stopPropagation(); close(); document.querySelector<HTMLElement>(`[data-pinned="${id}"]`)?.focus(); }
      else if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
        e.preventDefault();
        buttons[(i + (e.key === 'ArrowDown' ? 1 : -1) + buttons.length) % buttons.length]?.focus();
      }
    }}>
    {items.map(item => <button type="button" role="menuitem" key={item.label} aria-disabled={!!item.off}
      onClick={() => { if (!item.off) item.run(); }}>{item.label}</button>)}
  </div>;
}
