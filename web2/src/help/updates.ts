/* W13c: an on-demand GitHub check. No assets or release notes are read. */
export const RELEASE_API = 'https://api.github.com/repos/MuhammadMoustafa/xppautX/releases/latest';
export const RELEASE_PREFIX = 'https://github.com/MuhammadMoustafa/xppautX/releases/';
const TAG_LIMIT = 64; // permits three safe integer components without unbounded API text
const URL_LIMIT = 256; // a release tag URL needs no long query or fragment
const CHECK_TIMEOUT_MS = 15000; // a network that never answers must give a shown error
const ANSWER_LIMIT = 1024 * 1024; // allows release asset metadata while bounding an untrusted response allocation

async function releaseAnswer(response: Response): Promise<unknown> {
  if (!response.body) throw new Error('GitHub returned no answer');
  const reader = response.body.getReader();
  const decoder = new TextDecoder('utf-8', {fatal: true});
  let bytes = 0, text = '';
  try {
    for (;;) {
      const chunk = await reader.read();
      if (chunk.done) break;
      bytes += chunk.value.byteLength;
      if (bytes > ANSWER_LIMIT) {
        await reader.cancel();
        throw new Error('GitHub release answer is too large');
      }
      text += decoder.decode(chunk.value, {stream: true});
    }
    return JSON.parse(text + decoder.decode()) as unknown;
  } finally { reader.releaseLock(); }
}

function version(tag: unknown): number[] {
  if (typeof tag !== 'string' || tag.length > TAG_LIMIT || !/^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.test(tag))
    throw new Error('invalid release version (expected vX.Y.Z)');
  const parts = tag.slice(1).split('.').map(Number);
  if (!parts.every(Number.isSafeInteger)) throw new Error('release version is too large');
  return parts;
}

export type UpdateResult = {text: string; url?: string};

export async function checkUpdates(about: string): Promise<UpdateResult> {
  // hello.about's first line is xpp_about_text's version; git-describe builds use its base tag.
  const local = /^xppautX (v\d+\.\d+\.\d+)(?:-\d+-g[0-9a-f]+)?\n/.exec(about)?.[1];
  const current = version(local);
  const response = await fetch(RELEASE_API, {signal: AbortSignal.timeout(CHECK_TIMEOUT_MS), credentials: 'omit'});
  if (!response.ok) throw new Error(`GitHub returned HTTP ${response.status}`);
  const answer = await releaseAnswer(response);
  if (!answer || typeof answer !== 'object') throw new Error('GitHub returned no release');
  const {tag_name: tag, html_url: url} = answer as {tag_name?: unknown; html_url?: unknown};
  const latest = version(tag);
  if (typeof url !== 'string' || url.length > URL_LIMIT || url !== `${RELEASE_PREFIX}tag/${tag}`)
    throw new Error('GitHub returned an invalid release page URL');
  const differing = latest.findIndex((n, i) => n !== current[i]);
  return differing >= 0 && latest[differing] > current[differing]
    ? {text: `xppautX ${latest.join('.')} is available. Nothing is downloaded or installed by xppautX.`, url}
    : {text: `xppautX ${current.join('.')} is the latest`};
}

export function requestUpdateCheck(): void {
  window.dispatchEvent(new Event('xpp-check-updates'));
}

export async function openRelease(url: string): Promise<void> {
  const bound = (window as unknown as {__xppOpenRelease?: (url: string) => Promise<unknown>}).__xppOpenRelease;
  if (bound) await bound(url);
  else window.open(url, '_blank', 'noopener,noreferrer');
}
