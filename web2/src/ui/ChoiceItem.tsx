/* One item of a menu_choice row (navigation.ts choiceItems): a button that sends the command with the item's arguments through the session's one
   send (registryChoice). The menu bar's submenus (File > Export > Data as > csv) and the table's and the plot's Export menus are made of it. */
import {choiceLabel} from '../store/fieldKinds';
import type {CommandRow} from '../protocol/kinds';
import {BUSY_TITLE, useMay, useSession} from './context';

/** `disabled` for what only the place knows (the kinescope's frames playing); the core's busy state is may's */
export function ChoiceItem({item, onPick, disabled = false}: {item: CommandRow; onPick?: () => void; disabled?: boolean}) {
  const session = useSession();
  const may = useMay();
  const choice = item.choice!;
  const open = may({cmd: item.id});
  return (
    <li>
      <button role="menuitem" data-command={item.id} data-choice={choice.value} title={open ? item.description : BUSY_TITLE} disabled={!open || disabled}
        onClick={() => {onPick?.(); session.registryChoice(item.id, choice.arguments);}}>{choiceLabel(choice.value)}</button>
    </li>
  );
}
