/* XPP's main menu (or File, or nUmerics, as the core switches it): each item
   sends its hotkey, exactly as clicking it in the X11 window does. A column
   beside the plot on wide screens, a drawer over it on narrow ones (the
   title bar's Menu button, Escape or a choice closes it). */
import {useEffect, useRef} from 'preact/hooks';
import {menuHelp} from '../help/links';
import {BUSY_TITLE, useSession, useStore} from './context';
import {HelpButton} from './HelpButton';

const WHICH = ['main', 'file', 'num'] as const;

export function MenuPanel() {
  const session = useSession();
  const menus = useStore(s => s.hello?.menus);
  const which = useStore(s => s.core?.menu ?? 0);
  const open = useStore(s => s.drawerOpen);
  /* the core runs one command at a time: its commands wait for the one running (T21) */
  const busy = useStore(s => s.busy);
  const nav = useRef<HTMLElement>(null);
  const close = () => session.store.dispatch({type: 'drawer', open: false});
  useEffect(() => {
    if (!open) {
      if (nav.current?.contains(document.activeElement)) document.querySelector<HTMLElement>('.menu-toggle')?.focus();
      return;
    }
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
  const name = WHICH[which] ?? 'main';
  const items = menus?.[name] ?? [], keys = menus?.[`${name}_keys`] ?? '', hints = menus?.[`${name}_hints`] ?? [];
  return (
    <>
      <nav id="command-menu" ref={nav} class={'menu-panel' + (open ? ' open' : '')} aria-label="Commands">
        <div class="menu-title-row">
          <h2 class="menu-title">{name === 'main' ? 'Commands' : name === 'file' ? 'File' : 'Numerics'}</h2>
          <HelpButton target={menuHelp(which)} label={name === 'main' ? 'the main commands' : name === 'file' ? 'the File menu' : 'Numerics'} />
        </div>
        <ul>
          {items.map((item, i) => (
            <li key={`${name}${i}`}>
              <button class="menu-item" title={busy ? BUSY_TITLE : hints[i]} aria-keyshortcuts={keys[i]} disabled={busy}
                onClick={() => { close(); session.key(keys[i]); }}>
                <kbd aria-hidden="true">{keys[i]?.toUpperCase()}</kbd>
                <span>{item}</span>
              </button>
            </li>
          ))}
        </ul>
      </nav>
      {open && <div class="drawer-scrim" onClick={close} aria-hidden="true" />}
    </>
  );
}
