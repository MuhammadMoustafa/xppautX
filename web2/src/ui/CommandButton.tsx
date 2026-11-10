import {useMay, useSession, useStore} from './context';
import {commandRow} from './navigation';

/** a command's label, read from its registry row: a control that is the command's own button shows this text, its menu entry's; a list or a message shows the row's name */
export function useCommandLabel(command: string): string {
  return commandRow(useStore(s => s.hello), command)?.label ?? '';
}
/** a command's full name, for a message or a list outside its menu */
export function useCommandName(command: string): string {
  return commandRow(useStore(s => s.hello), command)?.name ?? '';
}
export function CommandLabel({command}: {command: string}) {
  return <>{useCommandLabel(command)}</>;
}

export function CommandButton({command}: {command: string}) {
  const may = useMay();
  const session = useSession(), hello = useStore(s => s.hello);
  const row = commandRow(hello, command);
  if (!row) return null;
  return <button class="small" data-command={command} disabled={!may({cmd: command})} title={`${row.name}: ${row.description}`}
    onClick={() => session.registryAction(command)}>{row.label}</button>;
}
