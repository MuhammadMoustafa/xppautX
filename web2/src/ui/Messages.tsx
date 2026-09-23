/* What XPP printed and the errors it reported, newest last (docs/ui-v2.md
   T16): errors (`message` `error`), the core's own log (stderr, `log`) and
   AUTO's console table and notes (also `log`, but classified apart by
   store/state.ts's classifyLogText -- the protocol does not tag them, so
   this is a best-effort text match, cheap and good enough to filter by) are
   shown with their own label and colour (A7: never colour alone) and can be
   filtered by kind and searched, both kept as local UI state since neither
   needs to be read by a test or survive a reload. */
import {useMemo, useState} from 'preact/hooks';
import type {LogEntry} from '../store/state';
import {useStore} from './context';

const KIND_LABEL: Record<LogEntry['kind'], string> = {error: 'Error', auto: 'AUTO', log: 'Log', info: 'Info'};
const FILTERS: (LogEntry['kind'] | 'all')[] = ['all', 'error', 'auto', 'log', 'info'];

export function Messages() {
  const log = useStore(s => s.log);
  const [filter, setFilter] = useState<LogEntry['kind'] | 'all'>('all');
  const [search, setSearch] = useState('');
  const counts = useMemo(() => {
    const c: Record<string, number> = {error: 0, auto: 0, log: 0, info: 0};
    for (const l of log) c[l.kind]++;
    return c;
  }, [log]);
  const needle = search.trim().toLowerCase();
  const shown = log.filter(l => (filter === 'all' || l.kind === filter) && (!needle || l.text.toLowerCase().includes(needle)));
  return (
    <details class="messages">
      <summary>
        Messages ({log.length}){counts.error > 0 && <span class="error-count"> · {counts.error} error{counts.error > 1 ? 's' : ''}</span>}
      </summary>
      <div class="messages-tools">
        <div class="messages-filters" role="group" aria-label="Filter messages by kind">
          {FILTERS.map(f => (
            <button key={f} class={'small' + (filter === f ? ' active' : '')} aria-pressed={filter === f} onClick={() => setFilter(f)}>
              {f === 'all' ? 'All' : KIND_LABEL[f]}{f !== 'all' && counts[f] > 0 ? ` (${counts[f]})` : ''}
            </button>
          ))}
        </div>
        <label class="messages-search">
          <span class="visually-hidden">Search messages</span>
          <input type="search" placeholder="Search…" value={search} onInput={e => setSearch((e.target as HTMLInputElement).value)} />
        </label>
      </div>
      {shown.length === 0 ? (
        <p class="text-empty">{log.length === 0 ? 'Nothing yet.' : 'No message matches.'}</p>
      ) : (
        <ul class="messages-list">
          {shown.map((l, i) => (
            <li key={i} class={'message-line message-' + l.kind}>
              <span class="message-kind">{KIND_LABEL[l.kind]}</span>
              <span class="message-text">{l.text}</span>
            </li>
          ))}
        </ul>
      )}
    </details>
  );
}
