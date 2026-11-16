/* One item of a menu_choice row (navigation.ts choiceItems): a button that sends the command with the item's arguments through the session's one
   send (registryChoice). The menu bar's submenus (File > Export > Data as > csv) and the table's and the plot's Export menus are made of it. */
import {choiceLabel} from '../store/fieldKinds';
import type {CommandRow} from '../protocol/kinds';
import {BUSY_TITLE, useMay, useSession} from './context';

export function ChoiceItem({item, onPick}: {item: CommandRow; onPick?: () => void}) {
  const session = useSession();
  const may = useMay();
  const choice = item.choice!;
  const open = may({cmd: item.id});
  return (
    <li>
      <button role="menuitem" data-command={item.id} data-choice={choice.value} title={open ? item.description : BUSY_TITLE} disabled={!open}
        onClick={() => {onPick?.(); session.registryChoice(item.id, choice);}}>{choiceLabel(choice.value)}</button>
    </li>
  );
}
