import {useRef, useState} from 'preact/hooks';
import {useMay, useSession, useStore} from './context';
import {CommandLabel, useCommandLabel} from './CommandButton';
import {menuPlace, paletteRows, registryRows} from './navigation';
import {effectiveKeys, togglePinned, withPinned} from '../store/keymap';
import {cycleDialogFocus, useDialogFocus} from './dialogFocus';
import {Modal} from './Modal';

export function CommandPalette() {
  const open = useStore(s => s.drawerOpen);
  return open ? <Palette /> : null;
}
function Palette() {
  const may = useMay();
  const session = useSession(), hello = useStore(s => s.hello), info = useStore(s => s.keymap.info);
  const [query, setQuery] = useState('');
  const title = useCommandLabel('tool.command_palette');
  const box = useRef<HTMLDivElement>(null);
  useDialogFocus(box, []);
  const close = () => session.store.dispatch({type: 'drawer', open: false});
  const rows = paletteRows(registryRows(hello), query);
  return <Modal><div id="command-palette" class="dialog command-palette" role="dialog" aria-modal="true" aria-label={title} ref={box}
    onKeyDown={e => {cycleDialogFocus(e, box.current);
      if (e.key === 'Escape') {e.stopPropagation(); close();}
      if (['ArrowDown','ArrowUp','Home','End'].includes(e.key)) {
        const buttons = [...(box.current?.querySelectorAll<HTMLButtonElement>('.menu-item:not(:disabled)') ?? [])];
        const at = buttons.indexOf(document.activeElement as HTMLButtonElement);
        const index = e.key === 'Home' ? 0 : e.key === 'End' ? buttons.length - 1 : (at + (e.key === 'ArrowDown' ? 1 : -1) + buttons.length) % buttons.length;
        e.preventDefault(); buttons[index]?.focus();
      }
    }}>
    <h2><CommandLabel command="tool.command_palette" /></h2>
    <input id="command-search" type="search" data-autofocus value={query} aria-label={title} onInput={e => setQuery(e.currentTarget.value)} />
    <ul>{rows.map(row => <li key={row.id}>
      <button class="menu-item" data-command={row.id} disabled={!may({cmd: row.id})} title={row.description}
        onClick={() => {close(); session.registryAction(row.id);}}>{menuPlace(row)} <kbd>{effectiveKeys(info, row).join(' / ')}</kbd></button>
      {info && <button data-pin={row.id} aria-pressed={info.pinned.includes(row.id)} onClick={() => session.setKeymap(withPinned(info, togglePinned(info.pinned, row.id)))}>{info.pinned.includes(row.id) ? 'Unpin' : 'Pin'}</button>}
    </li>)}</ul><button onClick={close}>Close</button>
  </div></Modal>;
}
