/* The bottom line: connection, work in progress (what runs and that Escape
   stops it), the core's last message, stored rows, and at the right end
   progress, Stop, then Stopping… until the run ends. */
import {busyText} from '../store/state';
import {useSession, useStore} from './context';
import {HelpButton} from './HelpButton';
import {menuName} from '../protocol/kinds';

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
  const running = useStore(s => s.running);
  const asking = useStore(s => s.ask !== null);
  const progress = useStore(s => s.progress);
  const bottom = useStore(s => s.bottom);
  const help = useStore(s => s.bottomHelp);
  const flash = useStore(s => s.flash);
  const rows = useStore(s => s.core?.rows ?? 0);
  const mode = useStore(s => menuName(s.hello, s.core?.menu ?? 0));
  const status = connectionText(connected, exited) ?? (stopping ? 'Stopping…' : busy ? busyText(running, asking) : 'Ready');
  const dot = exited !== null ? 'down' : !connected ? '' : busy ? 'busy' : 'up';
  return (
    <footer class="status-bar" data-flash={flash}>
      {/* a new warning: a second's flash, the key restarting the animation (theme.css .status-flash) */}
      {flash > 0 && <span key={flash} class="status-flash" aria-hidden="true" />}
      <span class={`status-dot ${dot}`} aria-hidden="true" />
      <span role="status" data-testid="status">{status}</span>
      {!busy && mode && mode !== 'main' && <span class="shortcut-status">{mode === 'file' ? 'File' : 'Numerics'} shortcuts · Esc returns</span>}
      <span class="status-message">{bottom} {help && <HelpButton target={help} label=".odex models" />}</span>
      <span class="muted rows">{rows} rows</span>
      {/* W83: a fixed-width slot the bar always keeps (theme.css .status-run), so
          the progress bar and Stop button toggling with a run never resize the
          bar or move the message/rows beside it; visibility, not mount/unmount,
          keeps the box. At the bar's right end, after the rows (W86). */}
      <span class="status-run">
        <progress class={progress ? 'shown' : ''} max={progress?.of ?? 1} value={progress?.n ?? 0}
          aria-hidden={progress ? undefined : 'true'} aria-label="Progress">
          {progress ? `${Math.round((100 * progress.n) / progress.of)}%` : ''}
        </progress>
        <button class={`small danger${busy ? ' shown' : ''}`} disabled={!busy || stopping}
          tabIndex={busy ? 0 : -1} onClick={() => session.abort()}
          title="Stop the running command (Escape does the same)">
          {stopping ? 'Stopping…' : 'Stop'}
        </button>
      </span>
    </footer>
  );
}
