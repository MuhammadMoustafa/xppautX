/** A button in a panel that runs a command of the core's table (hello.command_table) by its menu and id, the
    way the Commands list does (Session.menuAction): the commands whose place is a panel (W229) --
    the Data panel's Transpose and Lookup tables, the Values panel's Named sets and Copy set line.
    Disabled, with the reason, when the command may not run now (docs/protocol.md "Action kinds"). */
import {menuCommand} from '../protocol/kinds';
import type {MenuName} from '../protocol/types';
import {BUSY_TITLE, useMay, useSession} from './context';

export function CommandButton({menu, item, label, title}: {menu: MenuName; item: string; label: string; title: string}) {
  const session = useSession();
  const off = !useMay()(menuCommand(menu, item));
  return (
    <button class="small" data-menu={menu} data-item={item} disabled={off} title={off ? BUSY_TITLE : title}
      onClick={() => session.menuAction(menu, item)}>{label}</button>
  );
}
