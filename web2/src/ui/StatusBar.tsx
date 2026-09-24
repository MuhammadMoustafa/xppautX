/* The bottom line: connection, work in progress (progress, Stop, then
   Stopping… until the run ends), the core's last message, stored rows. */
import {useSession, useStore} from './context';

/* the connection, when it is not simply up (AutoStatus.tsx says it too) */
export function connectionText(connected: boolean, exited: number | null): string | null {
  return exited !== null ? 'XPP has stopped' : !connected ? 'Connecting…' : null;
}

export function StatusBar() {
  const session = useSession();
  const connected = useStore(s => s.connected);
  const exited = useStore(s => s.exited);
  const busy = useStore(s => s.busy);
  const stopping = useStore(s => s.stopping);
  const progress = useStore(s => s.progress);
  const bottom = useStore(s => s.bottom);
  const rows = useStore(s => s.core?.rows ?? 0);
  const status = connectionText(connected, exited) ?? (stopping ? 'Stopping…' : busy ? 'Working…' : 'Ready');
  const dot = exited !== null ? 'down' : !connected ? '' : busy ? 'busy' : 'up';
  return (
    <footer class="status-bar">
      <span class={`status-dot ${dot}`} aria-hidden="true" />
      <span role="status" data-testid="status">{status}</span>
      {progress && (
        <progress max={progress.of} value={progress.n} aria-label="Progress">
          {Math.round((100 * progress.n) / progress.of)}%
        </progress>
      )}
      {busy && (
        <button class="small danger" disabled={stopping} onClick={() => session.abort()}
          title="Stop the running command (Escape does the same)">
          {stopping ? 'Stopping…' : 'Stop'}
        </button>
      )}
      <span class="status-message">{bottom}</span>
      <span class="muted rows">{rows} rows</span>
    </footer>
  );
}
