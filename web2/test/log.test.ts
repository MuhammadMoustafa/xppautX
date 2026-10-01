/* The page's log in chunks (store/log.ts): the entries and counts a whole array would give. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {appendLog, emptyLog, LOG_KEEP, logEntries, lastEntry, type LogEntry} from '../src/store/log';

const entry = (i: number): LogEntry => ({kind: i % 3 ? 'log' : 'auto', text: `line ${i}\n`});

test('the newest LOG_KEEP entries in order, their counts by kind, one event at a time', () => {
  let l = emptyLog;
  const all: LogEntry[] = [];
  for (let i = 0; i < LOG_KEEP + 700; i++) {
    const e = entry(i);
    all.push(e);
    l = appendLog(l, [e]);
  }
  const want = all.slice(-LOG_KEEP);
  assert.deepEqual(logEntries(l), want);
  assert.equal(l.length, LOG_KEEP);
  assert.deepEqual(l.counts, {log: want.filter(e => e.kind === 'log').length, auto: want.filter(e => e.kind === 'auto').length,
    error: 0, info: 0});
});

test('the newest entry replaced (a line that went on), then more; an older state keeps its own', () => {
  const a = appendLog(emptyLog, [entry(1), {kind: 'log', text: 'half'}]);
  const b = appendLog(a, [entry(4)], {kind: 'auto', text: 'half done\n'});
  assert.deepEqual(logEntries(b).map(e => e.text), ['line 1\n', 'half done\n', 'line 4\n']);
  assert.deepEqual([b.counts.log, b.counts.auto], [2, 1]);
  assert.equal(lastEntry(a)?.text, 'half', 'a is as it was');
  assert.equal(lastEntry(emptyLog), undefined);
});
