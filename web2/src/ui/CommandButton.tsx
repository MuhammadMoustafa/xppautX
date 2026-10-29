import {useMay, useSession, useStore} from './context';
import {registryRows} from './navigation';

export function CommandButton({command}: {command: string}) {
  const may = useMay();
  const session = useSession(), hello = useStore(s => s.hello);
  const row = registryRows(hello).find(row => row.id === command);
  if (!row) return null;
  return <button class="small" data-command={command} disabled={!may({cmd: command})} title={row.description}
    onClick={() => session.registryAction(command)}>{row.label}</button>;
}
