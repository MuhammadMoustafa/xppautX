/* What XPP printed and the errors it reported, newest last. */
import {useStore} from './context';

export function Messages() {
  const log = useStore(s => s.log);
  const errors = log.filter(l => l.kind === 'error').length;
  return (
    <details class="messages">
      <summary>
        Messages ({log.length}){errors > 0 && <span class="error-count"> · {errors} error{errors > 1 ? 's' : ''}</span>}
      </summary>
      <pre>{log.map(l => (l.kind === 'error' ? `error: ${l.text}\n` : l.text)).join('')}</pre>
    </details>
  );
}
