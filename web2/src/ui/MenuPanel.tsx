/* XPP's main menu (or File, or nUmerics, as the core switches it): each item
   sends its hotkey, exactly as clicking it in the X11 window does. A column
   beside the plot on wide screens, a drawer over it on narrow ones (the
   title bar's Menu button, Escape or a choice closes it). */
import {useEffect, useRef} from 'preact/hooks';
import {useFocusBackOnClose} from './focusBack';
import {menuHelp} from '../help/links';
import {litMenuKey} from '../store/player';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {HelpButton} from './HelpButton';
import {menuName} from '../protocol/kinds';

export function MenuPanel() {
  const session = useSession();
  const hello = useStore(s => s.hello);
  /* before the first state: the main menu */
  const which = useStore(s => s.core?.menu ?? 0);
  const open = useStore(s => s.drawerOpen);
  /* a recording's step presses this key now (W59b) */
  const lit = useStore(s => litMenuKey(s.player));
  /* the menu opens during a run too; an item whose kind waits for it is disabled (W95) */
  const may = useMay();
  const nav = useRef<HTMLElement>(null);
  const close = () => session.store.dispatch({type: 'drawer', open: false});
  useFocusBackOnClose(open, nav, '.menu-toggle');
  useEffect(() => {
    if (!open) return;
    nav.current?.querySelector<HTMLElement>('button')?.focus();
    /* Escape closes the open drawer wherever the focus is */
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== 'Escape' || session.store.getState().ask) return;
      e.preventDefault();
      e.stopPropagation();
      close();
    };
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, [open]);
  const name = menuName(hello, which);
  const menus = hello?.menus;
  const items = menus && name ? menus[name] : [], keys = menus && name ? menus[`${name}_keys`] : '';
  const hints = menus && name ? menus[`${name}_hints`] : [];
  return (
    <>
      <nav id="command-menu" ref={nav} class={'menu-panel' + (open ? ' open' : '')} aria-label="Commands">
        <div class="menu-title-row">
          <h2 class="menu-title">{name === 'main' ? 'Commands' : name === 'file' ? 'File' : 'Numerics'}</h2>
          <HelpButton target={menuHelp(name)} label={name === 'main' ? 'the main commands' : name === 'file' ? 'the File menu' : 'Numerics'} />
        </div>
        <ul>
          {items.map((item, i) => {
            const off = !may({cmd: 'key', key: keys[i]});
            return (
              <li key={`${name}${i}`}>
                <button class={'menu-item' + (lit !== null && lit === keys[i] ? ' lit' : '')} title={off ? BUSY_TITLE : hints[i]} aria-keyshortcuts={keys[i]} disabled={off}
                  onClick={() => { close(); session.key(keys[i]); }}>
                  <kbd aria-hidden="true">{keys[i]?.toUpperCase()}</kbd>
                  <span>{item}</span>
                </button>
              </li>
            );
          })}
        </ul>
      </nav>
      {open && <div class="drawer-scrim" onClick={close} aria-hidden="true" />}
    </>
  );
}
