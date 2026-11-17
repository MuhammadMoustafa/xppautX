/* The bottom line: connection, work in progress (what runs and that Escape
   stops it), the core's last message, Messages (a pop-up above the bar), stored rows, and at the right end
   progress, Stop, then Stopping… until the run ends. */
import {busyText} from '../store/state';
import {shortTitle} from '../store/batch';
import {useSession, useStore} from './context';
import {CommandLabel} from './CommandButton';
import {offerText, requestUpdateCheck} from '../help/updates';
import {HelpButton} from './HelpButton';
import {Messages} from './Messages';

/* the connection, when it is not simply up (AutoxStatus.tsx says it too) */
export function connectionText(connected: boolean, exited: number | null): string | null {
  return exited !== null ? 'xppautX has stopped' : !connected ? 'Connecting…' : null;
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
  const update = useStore(s => s.update);
  const batch = useStore(s => s.batch);
  const rows = useStore(s => s.core?.rows ?? 0);
  const status = connectionText(connected, exited) ?? (stopping ? 'Stopping…' : batch.phase === 'running' ? `${batch.title}: step ${batch.index + 1} of ${batch.steps.length}` : busy ? busyText(running, asking) : 'Ready');
  const dot = exited !== null ? 'down' : !connected ? '' : busy ? 'busy' : 'up';
  return (
    <footer class="status-bar" data-flash={flash}>
      {/* a new warning: a second's flash, the key restarting the animation (css/layout.css .status-flash) */}
      {flash > 0 && <span key={flash} class="status-flash" aria-hidden="true" />}
      <span class={`status-dot ${dot}`} aria-hidden="true" />
      <span role="status" data-testid="status" class={'status-text' + (batch.phase === 'running' ? ' status-long' : '')}>{status}</span>
      {/* a phone has room for the pin's name cut short and its step count only (css/layout.css .status-long) */}
      {batch.phase === 'running' && <span class="status-short" aria-hidden="true"><span class="status-name">{shortTitle(batch.title)}</span> {batch.index + 1}/{batch.steps.length}</span>}
      {batch.phase === 'running' && <code class="status-step" data-batch-step>{batch.steps[batch.index]}</code>}
      <span class="status-message">{bottom} {help && <HelpButton target={help} label=".odex models" />}</span>
      {/* W622: the opening's check found a newer release: a quiet notice, never a dialog; the button shows it in the update dialog */}
      {update && <button class="small update-notice" data-update-notice onClick={requestUpdateCheck}
        title="A newer release is out: show it">{offerText(update)}</button>}
      <Messages />
      <span class="muted rows">{rows} rows</span>
      {/* W83: a fixed-width slot the bar always keeps (css/layout.css .status-run), so
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
          {stopping ? 'Stopping…' : <CommandLabel command="run.stop" />}
        </button>
      </span>
    </footer>
  );
}
