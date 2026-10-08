import {test} from 'node:test';
import assert from 'node:assert/strict';
import {checkUpdates, RELEASE_PREFIX, RELEASE_API, UpdateError} from '../src/help/updates';

test('updates compare numeric semver components and validate untrusted fields', async () => {
  const original = globalThis.fetch;
  const answer = (tag: string, url = `${RELEASE_PREFIX}tag/${tag}`) => {
    globalThis.fetch = async () => new Response(JSON.stringify({tag_name: tag, html_url: url, body: '<script>ignored</script>'}));
  };
  try {
    answer('v0.10.0');
    assert.ok((await checkUpdates('xppautX v0.9.0\n')).url);
    answer('v0.9.0');
    assert.equal((await checkUpdates('xppautX v0.10.0-12-gabcdef\n')).text, 'xppautX 0.10.0 is the latest');
    /* a beta is older than the release of its own numbers (releases/latest never names a pre-release) */
    answer('v0.1.0');
    assert.ok((await checkUpdates('xppautX v0.1.0-beta.1\n')).url);
    assert.ok((await checkUpdates('xppautX v0.1.0-beta.1-7-gabcdef\n')).url);
    assert.equal((await checkUpdates('xppautX v0.1.0\n')).text, 'xppautX 0.1.0 is the latest');
    answer('v0.0.9');
    assert.equal((await checkUpdates('xppautX v0.1.0-beta.1\n')).text, 'xppautX 0.1.0 is the latest');
    answer('v01.0.0');
    await assert.rejects(checkUpdates('xppautX v0.1.0\n'), /invalid release version/);
    answer('v1.0.0', 'https://evil.example/');
    await assert.rejects(checkUpdates('xppautX v0.1.0\n'), /invalid release page/);
    answer('v1.0.0');
    await assert.rejects(checkUpdates('xppautX dev\n'), /invalid release version/);
    globalThis.fetch = async () => new Response('unavailable', {status: 503});
    await assert.rejects(checkUpdates('xppautX v0.1.0\n'), /HTTP 503/);
    globalThis.fetch = async () => new Response('{');
    await assert.rejects(checkUpdates('xppautX v0.1.0\n'), UpdateError);
    globalThis.fetch = async () => new Response('x'.repeat(1024 * 1024 + 1));
    await assert.rejects(checkUpdates('xppautX v0.1.0\n'), /answer is too large/);
  } finally { globalThis.fetch = original; }
});


test('update errors preserve local and multiline answer places', async () => {
  const original = globalThis.fetch;
  let requests = 0;
  try {
    globalThis.fetch = async () => { requests++; return new Response('{\n"tag_name": "v1.0.0",\n"html_url": !\n}'); };
    await assert.rejects(checkUpdates('xppautX dev\n'), (e: UpdateError) => {
      assert.deepEqual(e.place, {file: 'hello.about', line: 1, col: 1, source: 'xppautX dev'});
      return true;
    });
    assert.equal(requests, 0);
    await assert.rejects(checkUpdates('xppautX v0.1.0\n'), (e: UpdateError) => {
      assert.equal(e.place.file, RELEASE_API);
      assert.equal(e.place.line, 3);
      assert.equal(e.place.source, '"html_url": !');
      return true;
    });
    globalThis.fetch = async () => { throw new Error('offline'); };
    await assert.rejects(checkUpdates('xppautX v0.1.0\n'), (e: UpdateError) => {
      assert.deepEqual(e.place, {file: RELEASE_API, line: 0, col: 0, source: ''});
      return true;
    });
  } finally { globalThis.fetch = original; }
});


test('update field places refer to the last root key, including escaped keys', async () => {
  const original = globalThis.fetch;
  try {
    globalThis.fetch = async () => new Response([
      '{',
      `"metadata": ${JSON.stringify({tag_name: 'dev', text: 'braces {} and "quotes"'})},`,
      '"tag_name": "v1.0.0",',
      '"tag\\u005fname": "dev",',
      '"html_url": "https://evil.example/"',
      '}',
    ].join('\n'));
    await assert.rejects(checkUpdates('xppautX v0.1.0\n'), (e: UpdateError) => {
      assert.equal(e.place.line, 4);
      assert.equal(e.place.source, '"tag\\u005fname": "dev",');
      return true;
    });
  } finally { globalThis.fetch = original; }
});

test('a caller can abort a pending update fetch', async () => {
  const original = globalThis.fetch;
  const controller = new AbortController();
  let signal: AbortSignal | undefined;
  try {
    globalThis.fetch = async (_url, options) => new Promise((_resolve, reject) => {
      signal = options!.signal!;
      signal.addEventListener('abort', () => reject(signal!.reason), {once: true});
    });
    const pending = checkUpdates('xppautX v0.1.0\n', controller.signal);
    controller.abort();
    assert.equal(signal?.aborted, true);
    await assert.rejects(pending, UpdateError);
  } finally { globalThis.fetch = original; }
});
