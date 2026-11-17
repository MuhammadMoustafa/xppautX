import {bindableRows} from './navigation';
/* The pinned commands of the quick-access toolbar (docs/command-design.md "Quick access"), after the
   fixed run buttons: the user's own commands (keymap.pinned) in order, each with its label, dragged to
   reorder, with a context menu (Unpin, Move earlier, Move later: also the keyboard's way, Shift+F10 or
   the menu key). Those that do not fit the row go into a "More" menu, never away. Every change is the
   `keymap` command's `set`, so the page keeps no second copy of the pins. */
import {useLayoutEffect, useRef, useState} from 'preact/hooks';
import type {Pin} from '../protocol/types';
import {movePinned, pinKeys, replacePin, visibleCount, withPinned} from '../store/keymap';
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
  const bindable = bindableRows(hello, keymap);
  const rows = keymap?.pinned ?? [];
  const box = useRef<HTMLDivElement>(null);
  const [shown, setShown] = useState(rows.length);
  const [menu, setMenu] = useState<{name: string; x: number; y: number} | null>(null);
  const dragged = useRef<string | null>(null);
  const ids = rows.map(r => r.name).join('\n');

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
  const save = (next: Pin[]) => session.setKeymap(withPinned(keymap, next));
  const run = (pin: Pin) => void session.runPin(pin.name);
  /* a pin may start when its first command may (the rest are checked as they run) */
  const mayRun = (pin: Pin) => may({cmd: pin.steps[0].split(' ')[0]});
  const dropOn = (target: string) => {
    const id = dragged.current;
    dragged.current = null;
    if (id && id !== target) save(movePinned(pinned, id, pinned.findIndex(p => p.name === target)));
  };
  const button = (pin: Pin, hidden: boolean) => {
    const off = !mayRun(pin);
    const keys = pinKeys(keymap, bindable, pin);
    const what = pin.steps.length === 1 ? pin.steps[0] : `${pin.steps.length} steps: ${pin.steps.join(', ')}`;
    return <button type="button" class={'quick-button' + (hidden ? ' quick-over' : '')} data-pinned={pin.name} draggable
      aria-disabled={off} aria-haspopup="menu" aria-keyshortcuts={keys[0]?.replace('Ctrl', 'Control')}
      title={off ? BUSY_TITLE : `${what}${keys.length ? ` (${keys.join(' / ')})` : ''}. Right-click for more.`}
      onClick={() => { if (!off) run(pin); }}
      onContextMenu={e => { e.preventDefault(); setMenu({name: pin.name, x: e.clientX, y: e.clientY}); }}
      onDragStart={() => { dragged.current = pin.name; }} onDragOver={e => e.preventDefault()}
      onDrop={e => { e.preventDefault(); dropOn(pin.name); }}>{pin.name}</button>;
  };
  const overflow = rows.slice(shown);
  return <div class="quick-access" role="toolbar" aria-label="Pinned commands" ref={box}>
    <div class="quick-pinned">{rows.map((row, i) => button(row, i >= shown))}</div>
    {overflow.length > 0 && <details class="quick-more">
      <summary aria-label={`More pinned commands (${overflow.length})`}>More</summary>
      <ul class="quick-more-list">{overflow.map(pin => <li key={pin.name}>
        <button type="button" data-pinned-more={pin.name} aria-disabled={!mayRun(pin)}
          onClick={e => {
            (e.currentTarget.closest('details') as HTMLDetailsElement).open = false;
            if (mayRun(pin)) run(pin);
          }}
          onContextMenu={e => { e.preventDefault(); setMenu({name: pin.name, x: e.clientX, y: e.clientY}); }}>{pin.name}</button></li>)}</ul>
    </details>}
    {menu && <PinMenu name={menu.name} x={menu.x} y={menu.y} pinned={pinned} close={() => setMenu(null)} save={save} />}
  </div>;
}

/** the context menu of one pin: Unpin, and Move earlier / later (the keyboard's way to reorder) */
function PinMenu({name, x, y, pinned, close, save}: {name: string; x: number; y: number; pinned: Pin[]; close(): void; save(next: Pin[]): void}) {
  const at = pinned.findIndex(p => p.name === name);
  const items: MenuItem[] = [
    {label: 'Unpin', run: () => save(replacePin(pinned, name, null))},
    {label: 'Move earlier', off: at <= 0, run: () => save(movePinned(pinned, name, at - 1))},
    {label: 'Move later', off: at >= pinned.length - 1, run: () => save(movePinned(pinned, name, at + 1))}];
  return <ContextMenu x={x} y={y} label="Pin" items={items} close={escaped => {
    close();
    if (escaped) document.querySelector<HTMLElement>(`[data-pinned="${CSS.escape(name)}"]`)?.focus();
  }} />;
}
