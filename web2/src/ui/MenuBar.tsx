import {useEffect, useRef} from 'preact/hooks';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {menuTree, registryRows, type MenuNode} from './navigation';
import {effectiveKeys} from '../store/keymap';

export function MenuBar() {
  const session = useSession(), hello = useStore(s => s.hello), info = useStore(s => s.keymap.info);
  const may = useMay();
  const windows = useStore(s => s.windows);
  const bar = useRef<HTMLElement>(null);
  const close = (focus = true) => {
    const summary = document.activeElement?.closest('[data-top-menu]')?.querySelector<HTMLElement>(':scope > summary');
    bar.current?.querySelectorAll('details[open]').forEach(d => d.removeAttribute('open'));
    if (focus) summary?.focus();
  };
  useEffect(() => {
    const element = bar.current;
    if (!element) return;
    const place = () => document.documentElement.style.setProperty('--menu-bottom', `${element.getBoundingClientRect().bottom}px`);
    /* the bar's bottom moves with the title bar above it, as well as with its own height */
    const observer = new ResizeObserver(place);
    observer.observe(element);
    if (element.previousElementSibling) observer.observe(element.previousElementSibling);
    place();
    return () => {observer.disconnect(); document.documentElement.style.removeProperty('--menu-bottom');};
  }, []);
  useEffect(() => {
    const outside = (e: PointerEvent) => {if (!bar.current?.contains(e.target as Node)) close(false);};
    document.addEventListener('pointerdown', outside);
    return () => document.removeEventListener('pointerdown', outside);
  }, []);
  const item = (node: MenuNode) => node.row ? <li key={node.row.id}><button role="menuitem" data-command={node.row.id} title={may({cmd: node.row.id}) ? node.row.description : BUSY_TITLE}
    disabled={!may({cmd: node.row.id})} onClick={() => {close(); session.registryAction(node.row!.id);}}>
    {node.name} <kbd>{effectiveKeys(info, node.row).join(' / ')}</kbd></button></li>
    : <li key={node.name}><details><summary>{node.name}</summary><ul role="menu">{node.children.map(item)}</ul></details></li>;
  const tree = menuTree(registryRows(hello));
  const order = ['File', 'Run', 'Analysis', 'Plot', 'Data', 'AUTO', 'Windows', 'Help'];
  const windowNodes = tree.find(node => node.name === 'Windows')?.children ?? [];
  const newWindows = windowNodes.filter(node => node.row?.id === 'window.create' || node.row?.id === 'auto.new_view')
    .sort((a,b) => Number(b.row?.id === 'window.create') - Number(a.row?.id === 'window.create'));
  const windowTools = windowNodes.filter(node => !newWindows.includes(node));
  return <nav class="app-menu" aria-label="Menu bar" ref={bar} onKeyDown={e => {
    if (e.key === 'Escape') {e.preventDefault(); e.stopPropagation(); close();}
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      const buttons = [...e.currentTarget.querySelectorAll<HTMLElement>('summary, button:not(:disabled)')].filter(b => b.getClientRects().length);
      const at = buttons.indexOf(document.activeElement as HTMLElement);
      e.preventDefault(); buttons[(at + (e.key === 'ArrowDown' ? 1 : -1) + buttons.length) % buttons.length]?.focus();
    }
  }}>{order.map(name => <details key={name} data-top-menu={name}><summary>{name}</summary><ul role="menu">
    {(name === 'Windows' ? newWindows : tree.find(node => node.name === name)?.children ?? []).map(item)}
    {name === 'Windows' && ['Graphs', 'AUTO views', 'Others'].map((group, i) => <li key={group}><strong>{group}</strong><ul>
      {windows.filter(w => i === 0 ? w.kind === 'graph' : i === 1 ? w.kind === 'diagram' : !['graph','diagram'].includes(w.kind)).map(w => <li key={w.name}>
        <details data-window-menu={w.name}><summary>{w.shown ? '✓ ' : ''}{w.name}</summary><ul>
          {['window.select', 'window.rename', w.shown ? 'window.hide' : 'window.show', 'window.destroy'].map(id => <li key={id}>
            <button data-window-action={id} disabled={id === 'window.rename' && !['graph','diagram'].includes(w.kind) || id === 'window.destroy' && w.kind === 'browser'}
              title={id === 'window.rename' && !['graph','diagram'].includes(w.kind) ? 'This tool window keeps its role name' : id === 'window.destroy' && w.kind === 'browser' ? 'The data browser stays open; Hide keeps it out of view' : undefined} onClick={() => {close(); if (id === 'window.rename') session.registryAction(id, {window: w.name});
              else {if (id === 'window.select') session.send({cmd: 'window.show', window: w.name}); session.send({cmd: id, window: w.name});}}}>{id === 'window.select' ? 'Bring to front' : id === 'window.rename' ? 'Rename' : id === 'window.destroy' ? 'Destroy' : w.shown ? 'Hide' : 'Show'}</button>
          </li>)}
        </ul></details>
      </li>)}
    </ul></li>)}
    {name === 'Windows' && windowTools.map(item)}
  </ul></details>)}</nav>;
}
