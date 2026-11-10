import {useState} from 'preact/hooks';
import {useMay, useSession, useStore} from './context';
import {CommandLabel, useCommandLabel} from './CommandButton';
import {featureLack, paletteRows, registryRows} from './navigation';
import {effectiveKeys, togglePinned, withPinned} from '../store/keymap';
import {Dialog} from './Dialog';

export function CommandPalette() {
  const open = useStore(s => s.drawerOpen);
  return open ? <Palette /> : null;
}
function Palette() {
  const may = useMay();
  const session = useSession(), hello = useStore(s => s.hello), info = useStore(s => s.keymap.info);
  const [query, setQuery] = useState('');
  const title = useCommandLabel('command_palette.open');
  const close = () => session.store.dispatch({type: 'drawer', open: false});
  const rows = paletteRows(registryRows(hello), query);
  const arrows = (e: KeyboardEvent): boolean => {
    if (!['ArrowDown', 'ArrowUp', 'Home', 'End'].includes(e.key)) return false;
    const buttons = [...(document.querySelectorAll<HTMLButtonElement>('#command-palette .menu-item:not(:disabled)'))];
    const at = buttons.indexOf(document.activeElement as HTMLButtonElement);
    const index = e.key === 'Home' ? 0 : e.key === 'End' ? buttons.length - 1 : (at + (e.key === 'ArrowDown' ? 1 : -1) + buttons.length) % buttons.length;
    e.preventDefault(); buttons[index]?.focus();
    return true;
  };
  return <Dialog title={<CommandLabel command="command_palette.open" />} name={title} width="list" onClose={close} onKeyDown={arrows} className="command-palette"
    attrs={{id: 'command-palette'}}>
    <span class="input-box"><input id="command-search" type="search" data-autofocus value={query} aria-label={title} placeholder={title} onInput={e => setQuery(e.currentTarget.value)} /></span>
    <ul>{rows.map(row => <li key={row.id}>
      <button class="menu-item" data-command={row.id} disabled={!may({cmd: row.id}) || !!featureLack(hello, row.needs)} title={featureLack(hello, row.needs) ?? row.description}
        onClick={() => {close(); session.registryAction(row.id);}}>{row.name} {featureLack(hello, row.needs) ? <small>{featureLack(hello, row.needs)}</small> : <kbd>{effectiveKeys(info, row).join(' / ')}</kbd>}</button>
      {info && <button data-pin={row.id} aria-pressed={info.pinned.includes(row.id)} onClick={() => session.setKeymap(withPinned(info, togglePinned(info.pinned, row.id)))}>{info.pinned.includes(row.id) ? 'Unpin' : 'Pin'}</button>}
    </li>)}</ul>
  </Dialog>;
}
