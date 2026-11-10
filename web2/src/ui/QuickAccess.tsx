import {registryRows} from './navigation';
/* The pinned commands of the quick-access toolbar (docs/command-design.md "Quick access"), after the
   fixed run buttons: the user's own commands (keymap.pinned) in order, each with its label, dragged to
   reorder, with a context menu (Unpin, Move earlier, Move later: also the keyboard's way, Shift+F10 or
   the menu key). Those that do not fit the row go into a "More" menu, never away. Every change is the
   `keymap` command's `set`, so the page keeps no second copy of the pins. */
import {useLayoutEffect, useRef, useState} from 'preact/hooks';
import type {CommandRow} from '../protocol/kinds';
import {effectiveKeys, movePinned, pinnedRows, togglePinned, visibleCount, withPinned} from '../store/keymap';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {ContextMenu, type MenuItem} from './ContextMenu';

/** the gap between the buttons of the row (css/plot.css .quick-pinned) in px, which the fit counts with */
const GAP_PX = 4;
/** the "More" button's width before it has been drawn, in px */
const MORE_PX = 64;

export function QuickAccess() {
  const session = useSession();
  const may = useMay();
  const hello = useStore(s => s.hello);
  const keymap = useStore(s => s.keymap.info);
  const rows = pinnedRows(keymap, registryRows(hello));
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
  const run = (row: CommandRow) => session.registryAction(row.id);
  const dropOn = (target: string) => {
    const id = dragged.current;
    dragged.current = null;
    if (id && id !== target) save(movePinned(pinned, id, pinned.indexOf(target)));
  };
  const button = (row: CommandRow, hidden: boolean) => {
    const off = !may({cmd: row.id});
    const keys = effectiveKeys(keymap, row).join(' / ');
    return <button type="button" class={'quick-button' + (hidden ? ' quick-over' : '')} data-pinned={row.id} draggable
      aria-disabled={off} aria-haspopup="menu" aria-keyshortcuts={effectiveKeys(keymap, row)[0]?.replace('Ctrl', 'Control')}
      title={off ? BUSY_TITLE : keys ? `${row.description} (${keys}). Right-click for more.` : `${row.description}. Right-click for more.`}
      onClick={() => { if (!off) run(row); }}
      onContextMenu={e => { e.preventDefault(); setMenu({id: row.id, x: e.clientX, y: e.clientY}); }}
      onDragStart={() => { dragged.current = row.id; }} onDragOver={e => e.preventDefault()}
      onDrop={e => { e.preventDefault(); dropOn(row.id); }}>{row.name}</button>;
  };
  const overflow = rows.slice(shown);
  return <div class="quick-access" role="toolbar" aria-label="Pinned commands" ref={box}>
    <div class="quick-pinned">{rows.map((row, i) => button(row, i >= shown))}</div>
    {overflow.length > 0 && <details class="quick-more">
      <summary aria-label={`More pinned commands (${overflow.length})`}>More</summary>
      <ul class="quick-more-list">{overflow.map(row => <li key={row.id}>
        <button type="button" data-pinned-more={row.id} aria-disabled={!may({cmd: row.id})}
          onClick={e => {
            (e.currentTarget.closest('details') as HTMLDetailsElement).open = false;
            if (may({cmd: row.id})) run(row);
          }}
          onContextMenu={e => { e.preventDefault(); setMenu({id: row.id, x: e.clientX, y: e.clientY}); }}>{row.name}</button></li>)}</ul>
    </details>}
    {menu && <PinMenu id={menu.id} x={menu.x} y={menu.y} pinned={pinned} close={() => setMenu(null)} save={save} />}
  </div>;
}

/** the context menu of one pinned command: Unpin, and Move earlier / later (the keyboard's way to reorder) */
function PinMenu({id, x, y, pinned, close, save}: {id: string; x: number; y: number; pinned: string[]; close(): void; save(next: string[]): void}) {
  const at = pinned.indexOf(id);
  const items: MenuItem[] = [
    {label: 'Unpin', run: () => save(togglePinned(pinned, id))},
    {label: 'Move earlier', off: at <= 0, run: () => save(movePinned(pinned, id, at - 1))},
    {label: 'Move later', off: at >= pinned.length - 1, run: () => save(movePinned(pinned, id, at + 1))}];
  return <ContextMenu x={x} y={y} label="Pinned command" items={items} close={escaped => {
    close();
    if (escaped) document.querySelector<HTMLElement>(`[data-pinned="${id}"]`)?.focus();
  }} />;
}
