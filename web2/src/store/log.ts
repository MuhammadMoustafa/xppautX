/* The page's log, what Messages and AUTO's Output show (docs/ui-v2.md
   T16, T27): its entries in chunks, so an event (an AUTO run sends a line
   a point) appends to the last chunk alone instead of copying the whole
   log, the oldest past LOG_KEEP dropped, and the count of each kind kept up
   (the panels' totals, without a scan of the log at every event). A chunk
   a state holds never changes. Pure: no DOM, no I/O. */

/** 'auto' is a best-effort guess (store/state.ts's classifyLogText): the
    protocol does not tag xpp_log_auto's lines apart from any other stderr
    text, so this is only ever as good as the patterns AUTO's own console
    table prints. */
export interface LogEntry {
  kind: 'log' | 'error' | 'info' | 'auto';
  text: string;
}

export interface Log {
  /** the entries, oldest first, in chunks of at most LOG_CHUNK */
  chunks: readonly (readonly LogEntry[])[];
  length: number;
  counts: Readonly<Record<LogEntry['kind'], number>>;
}

/* lines kept (T27: one entry per line, so a long AUTO run's table in Output) */
export const LOG_KEEP = 5000;
/* entries per chunk: what an event copies (and the chunk list, LOG_KEEP / LOG_CHUNK long) */
const LOG_CHUNK = 256;

export const emptyLog: Log = {chunks: [], length: 0, counts: {log: 0, error: 0, info: 0, auto: 0}};

/** the entries, oldest first */
export function logEntries(l: Log): LogEntry[] {
  return l.chunks.flat();
}

/** the newest entry */
export function lastEntry(l: Log): LogEntry | undefined {
  const c = l.chunks[l.chunks.length - 1];
  return c?.[c.length - 1];
}

/** `l` with its newest entry replaced by `replaceLast` (when given), then `entries` after it */
export function appendLog(l: Log, entries: readonly LogEntry[], replaceLast?: LogEntry): Log {
  const chunks = l.chunks.slice(), counts = {...l.counts};
  let length = l.length;
  let tail = chunks.length ? [...chunks.pop()!] : [];
  if (replaceLast && tail.length) {
    counts[tail[tail.length - 1].kind]--;
    counts[replaceLast.kind]++;
    tail[tail.length - 1] = replaceLast;
  }
  for (const e of entries) {
    if (tail.length === LOG_CHUNK) {
      chunks.push(tail);
      tail = [];
    }
    tail.push(e);
    counts[e.kind]++;
    length++;
  }
  if (tail.length) chunks.push(tail);
  while (length > LOG_KEEP) { /* the oldest go: whole chunks, then part of one */
    const first = chunks[0], over = Math.min(first.length, length - LOG_KEEP);
    for (let i = 0; i < over; i++) counts[first[i].kind]--;
    if (over === first.length) chunks.shift();
    else chunks[0] = first.slice(over);
    length -= over;
  }
  return {chunks, length, counts};
}
