import {test} from 'node:test';
import assert from 'node:assert/strict';
import {checkUpdates, RELEASE_PREFIX, RELEASE_API, RELEASES_API, UpdateError} from '../src/help/updates';
const about = (version: string) => [[{text: version}]];

test('updates compare numeric semver components and validate untrusted fields', async () => {
  const original = globalThis.fetch;
  const answer = (tag: string, url = `${RELEASE_PREFIX}tag/${tag}`) => {
    globalThis.fetch = async () => new Response(JSON.stringify({tag_name: tag, html_url: url, body: '<script>ignored</script>'}));
  };
  try {
    answer('v0.10.0');
    assert.ok((await checkUpdates(about('xppautX v0.9.0'))).url);
    answer('v0.9.0');
    assert.equal((await checkUpdates(about('xppautX v0.10.0-12-gabcdef'))).text, 'xppautX 0.10.0 is the latest');
    /* a pre-release build asks the list (see the next test); a release build is not offered a beta */
    answer('v0.1.0');
    assert.equal((await checkUpdates(about('xppautX v0.1.0'))).text, 'xppautX 0.1.0 is the latest');
    answer('v01.0.0');
    await assert.rejects(checkUpdates(about('xppautX v0.1.0')), /invalid release version/);
    answer('v1.0.0', 'https://evil.example/');
    await assert.rejects(checkUpdates(about('xppautX v0.1.0')), /invalid release page/);
    answer('v1.0.0');
    await assert.rejects(checkUpdates(about('xppautX dev')), /invalid release version/);
    globalThis.fetch = async () => new Response('unavailable', {status: 503});
    await assert.rejects(checkUpdates(about('xppautX v0.1.0')), /HTTP 503/);
    globalThis.fetch = async () => new Response('{');
    await assert.rejects(checkUpdates(about('xppautX v0.1.0')), UpdateError);
    globalThis.fetch = async () => new Response('x'.repeat(1024 * 1024 + 1));
    await assert.rejects(checkUpdates(about('xppautX v0.1.0')), /answer is too large/);
  } finally { globalThis.fetch = original; }
});

test('a pre-release build asks the list and takes the highest version by SemVer precedence', async () => {
  const original = globalThis.fetch;
  let asked = '';
  const list = (...tags: unknown[]) => {
    globalThis.fetch = async (url) => {
      asked = String(url);
      return new Response(JSON.stringify(tags.map(t => typeof t === 'string' ? {tag_name: t, html_url: `${RELEASE_PREFIX}tag/${t}`} : t)));
    };
  };
  const check = async (local: string, ...tags: unknown[]) => { list(...tags); return checkUpdates(about(`xppautX ${local}`)); };
  try {
    let r = await check('v0.1.0-beta.1', 'v0.1.0-beta.1', 'v0.1.0-beta.2');
    assert.equal(asked, RELEASES_API);
    assert.equal(r.text.startsWith('xppautX 0.1.0-beta.2 is available'), true);
    assert.equal(r.url, `${RELEASE_PREFIX}tag/v0.1.0-beta.2`);
    r = await check('v0.1.0-beta.2-12-gabcdef', 'v0.1.0-beta.1', 'v0.1.0-beta.2');
    assert.equal(r.text, 'xppautX 0.1.0-beta.2 is the latest');
    assert.equal((await check('v0.1.0-beta.2', 'v0.1.0-beta.1')).text, 'xppautX 0.1.0-beta.2 is the latest');
    /* precedence, whatever the list order: beta.11 > beta.2 (numeric), rc > beta (ASCII), release > any beta, longer > shorter */
    assert.match((await check('v0.1.0-beta.1', 'v0.1.0-beta.2', 'v0.1.0-beta.11', 'v0.0.9')).text, /0\.1\.0-beta\.11 is available/);
    assert.match((await check('v0.1.0-beta.1', 'v0.1.0-rc.1', 'v0.1.0-beta.11')).text, /0\.1\.0-rc\.1 is available/);
    assert.match((await check('v0.1.0-rc.1', 'v0.1.0', 'v0.1.0-rc.1')).text, /0\.1\.0 is available/);
    assert.match((await check('v0.1.0-beta', 'v0.1.0-beta.1')).text, /0\.1\.0-beta\.1 is available/);
    assert.match((await check('v0.1.0-beta.1', 'v0.1.0-beta.x', 'v0.1.0-beta.9')).text, /beta\.x is available/);
    assert.match((await check('v0.1.0-beta.1', 'v0.2.0-alpha')).text, /0\.2\.0-alpha is available/);
    /* shown errors */
    for (const bad of [[], 'x', {}, null]) {
      globalThis.fetch = async () => new Response(JSON.stringify(bad));
      await assert.rejects(checkUpdates(about('xppautX v0.1.0-beta.1')), UpdateError);
    }
    await assert.rejects(check('v0.1.0-beta.1', 'v0.1.0-beta.2', 5), /bad release entry/);
    await assert.rejects(check('v0.1.0-beta.1', 'v0.1.0-beta.01'), /invalid release version/);
    await assert.rejects(check('v0.1.0-beta.1', 'v0.1.0-beta..1'), /invalid release version/);
    await assert.rejects(check('v0.1.0-beta.1', 'v0.1.0-' + 'b'.repeat(40)), /invalid release version/);
    await assert.rejects(check('v0.1.0-beta.1', 'v0.1.0-beta.1/../../x'), /invalid release version/);
    await assert.rejects(check('v0.1.0-beta.1', 'v0.1.0-beta.2?x#y'), /invalid release version/);
    await assert.rejects(check('v0.1.0-beta.1', {tag_name: 'v0.1.0-beta.2', html_url: 'https://evil.example/'}), /invalid release page/);
    await assert.rejects(check('v0.1.0-beta.1', {tag_name: 'v0.1.0-beta.2', html_url: `${RELEASE_PREFIX}tag/v0.1.0-beta.3`}), /invalid release page/);
    /* a stable build asks `latest` and is never offered a beta, even from a wrong answer */
    globalThis.fetch = async (url) => {
      asked = String(url);
      return new Response(JSON.stringify({tag_name: 'v0.2.0-beta.1', html_url: `${RELEASE_PREFIX}tag/v0.2.0-beta.1`}));
    };
    await assert.rejects(checkUpdates(about('xppautX v0.1.0')), /invalid release version/);
    assert.equal(asked, RELEASE_API);
  } finally { globalThis.fetch = original; }
});

test('update errors preserve local and multiline answer places', async () => {
  const original = globalThis.fetch;
  let requests = 0;
  try {
    globalThis.fetch = async () => { requests++; return new Response('{\n"tag_name": "v1.0.0",\n"html_url": !\n}'); };
    await assert.rejects(checkUpdates(about('xppautX dev')), (e: UpdateError) => {
      assert.deepEqual(e.place, {file: 'hello.about', line: 1, col: 1, source: 'xppautX dev'});
      return true;
    });
    assert.equal(requests, 0);
    await assert.rejects(checkUpdates(about('xppautX v0.1.0')), (e: UpdateError) => {
      assert.equal(e.place.file, RELEASE_API);
      assert.equal(e.place.line, 3);
      assert.equal(e.place.source, '"html_url": !');
      return true;
    });
    globalThis.fetch = async () => { throw new Error('offline'); };
    await assert.rejects(checkUpdates(about('xppautX v0.1.0')), (e: UpdateError) => {
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
    await assert.rejects(checkUpdates(about('xppautX v0.1.0')), (e: UpdateError) => {
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
    const pending = checkUpdates(about('xppautX v0.1.0'), controller.signal);
    controller.abort();
    assert.equal(signal?.aborted, true);
    await assert.rejects(pending, UpdateError);
  } finally { globalThis.fetch = original; }
});
