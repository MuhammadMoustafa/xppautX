import {test} from 'node:test';
import assert from 'node:assert/strict';
import {checkUpdates, RELEASE_PREFIX} from '../src/help/updates';

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
    answer('v01.0.0');
    await assert.rejects(checkUpdates('xppautX v0.1.0\n'), /invalid release version/);
    answer('v1.0.0', 'https://evil.example/');
    await assert.rejects(checkUpdates('xppautX v0.1.0\n'), /invalid release page/);
    answer('v1.0.0');
    await assert.rejects(checkUpdates('xppautX dev\n'), /invalid release version/);
    globalThis.fetch = async () => new Response('unavailable', {status: 503});
    await assert.rejects(checkUpdates('xppautX v0.1.0\n'), /HTTP 503/);
    globalThis.fetch = async () => new Response('{');
    await assert.rejects(checkUpdates('xppautX v0.1.0\n'), SyntaxError);
    globalThis.fetch = async () => new Response('x'.repeat(1024 * 1024 + 1));
    await assert.rejects(checkUpdates('xppautX v0.1.0\n'), /answer is too large/);
  } finally { globalThis.fetch = original; }
});
