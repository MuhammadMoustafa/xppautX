import {useMay, useSession, useStore} from './context';
import {commandRow} from './navigation';

/** a command's label, read from its registry row: every control that names a command shows this text */
export function useCommandLabel(command: string): string {
  return commandRow(useStore(s => s.hello), command)?.label ?? '';
}
export function CommandLabel({command}: {command: string}) {
  return <>{useCommandLabel(command)}</>;
}

export function CommandButton({command}: {command: string}) {
  const may = useMay();
  const session = useSession(), hello = useStore(s => s.hello);
  const row = commandRow(hello, command);
  if (!row) return null;
  return <button class="small" data-command={command} disabled={!may({cmd: command})} title={row.description}
    onClick={() => session.registryAction(command)}>{row.label}</button>;
}
