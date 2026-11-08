import {useEffect, useRef} from 'preact/hooks';
import type {ComponentChildren} from 'preact';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {CommandLabel} from './CommandButton';
import {commandRow, menuTree, registryRows, type MenuNode} from './navigation';
import {effectiveKeys} from '../store/keymap';
import {closeMenuBar, openPanel} from './hotkeys';
import {useFocusBackOnClose} from './focusBack';

/** the title bar's Menu button: on a phone (below 48rem) the menu bar is a drawer this opens (css/layout.css) */
export function MenuButton() {
  const session = useSession(), open = useStore(s => s.menuOpen);
  return <button class="menu-toggle" aria-controls="app-menu" aria-expanded={open}
    onClick={() => session.store.dispatch({type: 'menu', open: !open})}>Menu</button>;
}

/** one entry of a level: a command's item, or a submenu (`menu`: its title and the level under it) */
interface Entry {menu: boolean; el: ComponentChildren}

/** a level's entries, its commands first and then its submenus (each kind in its own order), with a line between the
    two kinds. The kind is the tree's own (a node with a row is a command), never a field of the rows. */
function withRules(entries: Entry[]): ComponentChildren[] {
  const ordered = [...entries.filter(entry => !entry.menu), ...entries.filter(entry => entry.menu)];
  return ordered.flatMap((entry, i) => i > 0 && ordered[i - 1].menu !== entry.menu
    ? [<li key={`rule-${i}`} class="menu-rule" role="separator" aria-hidden="true" />, entry.el] : [entry.el]);
}

/** a menu entry's key, or nothing for a command no key runs (no empty box) */
const keyChip = (keys: string[]) => keys.length ? <kbd>{keys.join(' / ')}</kbd> : null;

export function MenuBar() {
  const session = useSession(), hello = useStore(s => s.hello), info = useStore(s => s.keymap.info);
  const drawer = useStore(s => s.menuOpen);
  const may = useMay();
  const windows = useStore(s => s.windows);
  const bar = useRef<HTMLElement>(null);
  const closeDrawer = () => session.store.dispatch({type: 'menu', open: false});
  const close = (focus = true) => {closeMenuBar(focus); closeDrawer();};
  useFocusBackOnClose(drawer, bar, '.menu-toggle');
  useEffect(() => {
    if (!drawer) return;
    bar.current?.querySelector<HTMLElement>('summary, button')?.focus();
    /* Escape closes the drawer through hotkeys.ts's one owner, after an open submenu */
    return openPanel(closeDrawer);
  }, [drawer]);
  useEffect(() => {
    const element = bar.current;
    if (!element) return;
    /* what panels sit below: the menu bar's bottom, or the title bar's when the bar is a drawer (fixed) */
    const place = () => document.documentElement.style.setProperty('--menu-bottom',
      `${(getComputedStyle(element).position === 'fixed' ? document.querySelector('.title-bar')! : element).getBoundingClientRect().bottom}px`);
    /* the bar's bottom moves with the title bar above it, as well as with its own height */
    const observer = new ResizeObserver(place);
    observer.observe(element);
    const titleBar = document.querySelector('.title-bar');
    if (titleBar) observer.observe(titleBar);
    place();
    return () => {observer.disconnect(); document.documentElement.style.removeProperty('--menu-bottom');};
  }, []);
  useEffect(() => {
    const element = bar.current;
    if (!element) return;
    /* one menu open at a time: opening one closes every open menu that is neither its ancestor nor its
       descendant (a window's submenu closes its sibling, not Windows). toggle does not bubble, so the
       capture listener on the bar sees each menu's. */
    const exclusive = (e: Event) => {
      const opened = e.target as HTMLDetailsElement;
      if (!opened.open) return;
      element.querySelectorAll<HTMLDetailsElement>('details[open]').forEach(other => {
        if (other !== opened && !other.contains(opened) && !opened.contains(other)) other.open = false;
      });
    };
    element.addEventListener('toggle', exclusive, true);
    return () => element.removeEventListener('toggle', exclusive, true);
  }, []);
  useEffect(() => {
    const outside = (e: PointerEvent) => {if (!bar.current?.contains(e.target as Node)) close(false);};
    document.addEventListener('pointerdown', outside);
    return () => document.removeEventListener('pointerdown', outside);
  }, []);
  /** a top menu's title and the entries under it, in reading order, the hidden ones left out */
  const menuItems = (top: HTMLElement) => [...top.querySelectorAll<HTMLElement>('summary, button:not(:disabled)')].filter(el => el.getClientRects().length);
  /** with one top menu open, pointing at another title opens that one (the toggle above closes the first).
      The click that follows the pointing keeps it open: the menu the pointer just opened is remembered
      until the pointer leaves it, and its title's click then does not close it again. */
  const pointed = useRef<HTMLDetailsElement | null>(null);
  const hover = (e: PointerEvent) => {
    const d = e.currentTarget as HTMLDetailsElement;
    if (!d.open && bar.current?.querySelector(':scope > details[open]')) { d.open = true; pointed.current = d; }
  };
  const unhover = (e: PointerEvent) => {if (pointed.current === e.currentTarget) pointed.current = null;};
  const title = (e: MouseEvent) => {
    const d = (e.currentTarget as HTMLElement).parentElement as HTMLDetailsElement;
    if (pointed.current === d && d.open) e.preventDefault();
    pointed.current = null;
  };
  const entry = (node: MenuNode): Entry => node.row ? {menu: false, el: <li key={node.row.id}><button role="menuitem" data-command={node.row.id} title={may({cmd: node.row.id}) ? node.row.description : BUSY_TITLE}
    disabled={!may({cmd: node.row.id})} onClick={() => {close(); session.registryAction(node.row!.id);}}>
    {node.name} {keyChip(effectiveKeys(info, node.row))}</button></li>}
    : {menu: true, el: <li key={node.name}><details><summary>{node.name}</summary><ul role="menu">{withRules(node.children.map(entry))}</ul></details></li>};
  const tree = menuTree(registryRows(hello));
  const order = ['File', 'Run', 'Analysis', 'Plot', 'Data', 'autoX', 'Windows', 'Help'];
  const windowNodes = tree.find(node => node.name === 'Windows')?.children ?? [];
  const newWindows = windowNodes.filter(node => node.row?.id === 'window.create' || node.row?.id === 'autox.new_view')
    .sort((a,b) => Number(b.row?.id === 'window.create') - Number(a.row?.id === 'window.create'));
  const windowTools = windowNodes.filter(node => !newWindows.includes(node));
  /* Windows: the new items, then each kind of window's list (its own submenus), then the tools */
  const windowLevel: Entry[] = [...newWindows.map(entry),
    ...['Graphs', 'autoX views', 'Others'].map((group, i) => ({menu: true, el: <li key={group}><strong>{group}</strong><ul>
      {windows.filter(w => i === 0 ? w.kind === 'graph' : i === 1 ? w.kind === 'diagram' : !['graph','diagram'].includes(w.kind)).map(w => <li key={w.name}>
        <details data-window-menu={w.name}><summary>{w.shown ? '✓ ' : ''}{w.name}</summary><ul>
          {['window.select', 'window.rename', w.shown ? 'window.hide' : 'window.show', 'window.destroy'].map(id => <li key={id}>
            <button data-window-action={id} disabled={id === 'window.rename' && !['graph','diagram'].includes(w.kind) || id === 'window.destroy' && w.kind === 'browser'}
              title={id === 'window.rename' && !['graph','diagram'].includes(w.kind) ? 'This tool window keeps its role name' : id === 'window.destroy' && w.kind === 'browser' ? 'The data browser stays open; Hide keeps it out of view' : undefined} onClick={() => {close(); if (id === 'window.rename') session.registryAction(id, {window: w.name});
              else {if (id === 'window.select') session.send({cmd: 'window.show', window: w.name}); session.send({cmd: id, window: w.name});}}}>{commandRow(hello, id)?.label}</button>
          </li>)}
        </ul></details>
      </li>)}
    </ul></li>}))]
    .concat(windowTools.map(entry));
  const levelOf = (name: string): Entry[] => name === 'Windows' ? windowLevel : (tree.find(node => node.name === name)?.children ?? []).map(entry);
  return <><div class={'menu-scrim' + (drawer ? ' open' : '')} onClick={closeDrawer} aria-hidden="true" />
  <nav class={'app-menu' + (drawer ? ' open' : '')} id="app-menu" aria-label="Menu bar" ref={bar} onKeyDown={e => {
    const active = document.activeElement as HTMLElement | null;
    const top = active?.closest<HTMLDetailsElement>('details[data-top-menu]');
    if (!top) return;
    const own = active?.closest<HTMLDetailsElement>('details');
    if (own && own !== top && (e.key === 'ArrowLeft' || e.key === 'ArrowRight')) {
      /* a submenu (beside its title from 48rem): Right opens it from its title and moves in, Left closes it and goes back to its title */
      e.preventDefault();
      const title = own.querySelector<HTMLElement>(':scope > summary');
      if (e.key === 'ArrowRight') {
        if (active === title) { own.open = true; own.querySelector<HTMLElement>(':scope > ul > li > :is(button, summary)')?.focus(); }
      } else {
        const parent = own.parentElement?.closest<HTMLDetailsElement>('details');
        own.open = false;
        (active === title && parent ? parent.querySelector<HTMLElement>(':scope > summary') : title)?.focus();
      }
      return;
    }
    if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
      /* from the title or an entry of this top menu, not from a submenu inside it */
      if (own !== top) return;
      const tops = [...e.currentTarget.querySelectorAll<HTMLDetailsElement>(':scope > details[data-top-menu]')];
      const next = tops[(tops.indexOf(top) + (e.key === 'ArrowRight' ? 1 : -1) + tops.length) % tops.length];
      const wasOpen = top.open;
      e.preventDefault();
      top.open = false; next.open = wasOpen;
      (wasOpen ? menuItems(next)[1] : next.querySelector<HTMLElement>(':scope > summary'))?.focus();
    } else if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      /* the open menu's entries (its title is item 0); a closed menu opens on its title's arrow */
      e.preventDefault();
      const down = e.key === 'ArrowDown';
      top.open = true;
      const entries = menuItems(top).slice(1);
      const at = entries.indexOf(active as HTMLElement);
      entries[at < 0 ? (down ? 0 : entries.length - 1) : (at + (down ? 1 : -1) + entries.length) % entries.length]?.focus();
    }
  }}><button class="menu-search" onClick={() => {close(false); session.store.dispatch({type: 'drawer', open: true});}}
    title="Search every command by name"><CommandLabel command="tool.command_palette" /></button>
  {order.map(name => <details key={name} data-top-menu={name} onPointerEnter={hover} onPointerLeave={unhover}><summary onClick={title}>{name}</summary><ul role="menu">
    {withRules(levelOf(name))}
  </ul></details>)}</nav></>;
}
