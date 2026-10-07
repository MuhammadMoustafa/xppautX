import {useEffect, useRef, useState} from 'preact/hooks';
import {useFocusBackOnClose} from './focusBack';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {HelpButton} from './HelpButton';
import {menuHelp} from '../help/links';
import {menuCommand, menuName} from '../protocol/kinds';
import {contextualShortcut, layerLabel, navigationGroups, navigationProblems} from './navigation';

export function MenuPanel() {
  const session = useSession();
  const hello = useStore(s => s.hello);
  const which = useStore(s => s.core?.menu ?? 0);
  const open = useStore(s => s.drawerOpen);
  const asking = useStore(s => !!s.ask);
  const [query, setQuery] = useState('');
  const may = useMay();
  const nav = useRef<HTMLElement>(null);
  const close = () => session.store.dispatch({type: 'drawer', open: false});
  useFocusBackOnClose(open, nav, '.menu-toggle');
  useEffect(() => {
    if (open) nav.current?.querySelector<HTMLInputElement>('input')?.focus();
  }, [open]);
  /* the groups and the core's menus must agree: a difference is shown once per hello (Escape is
     hotkeys.ts's, the one handler) */
  useEffect(() => {
    const problems = hello ? navigationProblems(hello) : [];
    if (problems.length) session.store.dispatch({type: 'toast', kind: 'error', text: problems.join(' ')});
  }, [hello]);
  const name = menuName(hello, which);
  const groups = navigationGroups(hello, query).map(group => ({...group,
    items: !query.trim() && group.id === 'run'
      ? group.items.filter(item => item.id === 'initialconds' || name === 'num' && item.menu === 'num')
      : group.items}));
  const navigate = (e: KeyboardEvent) => {
    if (e.target instanceof HTMLInputElement && e.key !== 'ArrowDown') return;
    if (!['ArrowDown', 'ArrowUp', 'Home', 'End'].includes(e.key)) return;
    const items = [...(nav.current?.querySelectorAll<HTMLButtonElement>('.menu-item:not(:disabled)') ?? [])]
      .filter(item => item.getClientRects().length > 0);
    if (!items.length) return;
    const current = items.indexOf(e.target as HTMLButtonElement);
    const index = e.key === 'Home' ? 0 : e.key === 'End' ? items.length - 1
      : (current + (e.key === 'ArrowDown' ? 1 : -1) + items.length) % items.length;
    e.preventDefault();
    e.stopPropagation();
    items[index].focus();
  };
  return <>
    <nav id="command-menu" ref={nav} class={'menu-panel' + (open ? ' open' : '')} aria-label="Commands" onKeyDown={navigate}>
      <div class="menu-title-row"><h2 class="menu-title">Commands</h2><HelpButton target={menuHelp('main')} label="commands" /></div>
      <label class="visually-hidden" htmlFor="command-search">Search commands</label>
      <input id="command-search" type="search" placeholder="Search commands…" value={query}
        onInput={e => setQuery((e.target as HTMLInputElement).value)} aria-keyshortcuts="Control+k Meta+k" />
      {name && name !== 'main' && hello && <div class="shortcut-context">
        <strong>{layerLabel(hello, name)} shortcuts active</strong>
        <span>The next letter selects a command here.</span>
        <button class="small" disabled={asking} onClick={() => session.typeKey('Escape')}>Main commands <kbd>Esc</kbd></button>
      </div>}
      {groups.map(group => group.items.length > 0 && <details class="command-group" key={`${group.id}:${!!query}`} open={!!query || group.id === 'files' || group.id === 'run'}>
        <summary>{group.name}</summary>
        <ul>{group.items.map((item, i) => {
          const off = !may(menuCommand(item.menu, item.id));
          const shortcut = contextualShortcut(item.shortcut, item.menu, name);
          return <li key={`${item.menu}:${item.id}`}><button class="menu-item" data-menu={item.menu} data-item={item.id}
            data-shortcut-active={name === item.menu && name !== 'main' ? 'true' : undefined}
            tabIndex={i === 0 ? 0 : -1} disabled={off} title={off ? BUSY_TITLE : `${item.description} (${shortcut})`}
            aria-keyshortcuts={shortcut.includes(',') ? undefined : shortcut}
            onFocus={e => {
              e.currentTarget.closest('ul')?.querySelectorAll<HTMLButtonElement>('button').forEach(button => {
                button.tabIndex = button === e.currentTarget ? 0 : -1;
              });
            }} onClick={() => {close(); session.menuAction(item.menu, item.id);}}>
            <span>{item.label}</span><kbd aria-hidden="true">{shortcut}</kbd>
          </button></li>;
        })}</ul>
      </details>)}
      {!groups.some(group => group.items.length) && <p role="status">{hello ? 'No matching commands.' : 'Loading commands…'}</p>}
    </nav>
    {open && <div class="drawer-scrim" onClick={close} aria-hidden="true" />}
  </>;
}
