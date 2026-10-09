import {sourcePlace, type ErrorPlace} from '../protocol/errors';
import type {AboutLine} from '../protocol/types';
import {aboutVersionLine} from './about';

/* W13c: an on-demand GitHub check. No assets or release notes are read. */
export const RELEASE_API = 'https://api.github.com/repos/MuhammadMoustafa/xppautX/releases/latest';
/* every release so far is a pre-release, which `releases/latest` never names: a pre-release build asks the list.
   GitHub lists releases newest first by created_at (the date of the tagged commit), not by version, so a small
   page is read and the highest version by SemVer precedence is picked; a stable build keeps `latest`. */
export const RELEASES_API = 'https://api.github.com/repos/MuhammadMoustafa/xppautX/releases?per_page=10';
export const RELEASE_PREFIX = 'https://github.com/MuhammadMoustafa/xppautX/releases/';
const TAG_LIMIT = 64; // permits three safe integer components and a pre-release suffix without unbounded API text
const SUFFIX_LIMIT = 32; // a pre-release suffix such as beta.12 or rc.1 needs far less; bounds the identifiers compared
const URL_LIMIT = 256; // a release tag URL needs no long query or fragment
const CHECK_TIMEOUT_MS = 15000; // a network that never answers must give a shown error
const ANSWER_LIMIT = 1024 * 1024; // allows release asset metadata while bounding an untrusted response allocation

export class UpdateError extends Error {
  constructor(message: string, readonly place: ErrorPlace) { super(message); }
}

function located(error: unknown, file: string, text = '', offset?: number): UpdateError {
  const message = error instanceof Error ? error.message : String(error);
  return new UpdateError(message, sourcePlace(file, text, offset));
}

// JSON.parse has validated the text; walk its tokens only to locate a root field.
// Quoted strings stay whole, so nested keys and braces inside values cannot misplace it.
function fieldOffset(text: string, field: string): number | undefined {
  if (!text.trimStart().startsWith('{')) return undefined;
  let depth = 0, key = false, offset: number | undefined;
  for (const token of text.matchAll(/"(?:\\.|[^"\\])*"|[{}]|\[|\]|[:,]/g)) {
    const word = token[0];
    if (word === '{' || word === '[') {
      depth++;
      if (depth === 1) key = word === '{';
    } else if (word === '}' || word === ']') depth--;
    else if (depth === 1 && word === ',') key = true;
    else if (depth === 1 && key && word.startsWith('"')) {
      if (JSON.parse(word) === field) offset = token.index;
      key = false;
    }
  }
  return offset; // JSON.parse uses the last occurrence of a repeated root key.
}

function parseAnswer(text: string, file: string): unknown {
  try { return JSON.parse(text) as unknown; }
  catch (e) {
    // Engines report a position, line/column, end of input, or an unexpected token.
    const message = e instanceof Error ? e.message : String(e);
    const position = /at position (\d+)(?: \(line \d+ column \d+\))?$/.exec(message);
    const lc = /\(line (\d+) column (\d+)\)$/.exec(message);
    const token = /Unexpected token '([^']+)'/.exec(message)?.[1];
    // Without coordinates, only a token occurring exactly once locates the error.
    const index = token ? text.indexOf(token) : -1;
    const uniqueToken = index >= 0 && index === text.lastIndexOf(token!) ? index : undefined;
    const offset = position ? Number(position[1]) : lc
      ? text.split('\n').slice(0, Number(lc[1]) - 1).reduce((n, row) => n + row.length + 1, 0) + Number(lc[2]) - 1
      : /end of JSON/.test(message) ? text.length : uniqueToken;
    throw located(e, file, text, offset);
  }
}

async function releaseAnswer(response: Response, file: string): Promise<{answer: unknown; text: string}> {
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
    text += decoder.decode();
    return {answer: parseAnswer(text, file), text};
  } finally { reader.releaseLock(); }
}

type Version = {core: number[]; pre: string[]; text: string};

/* vX.Y.Z or vX.Y.Z-identifiers (SemVer: dot-separated [0-9A-Za-z-], numeric ones without a leading zero) */
function version(tag: unknown, preRelease = true): Version {
  const m = typeof tag === 'string' && tag.length <= TAG_LIMIT
    ? /^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z.-]+))?$/.exec(tag) : null;
  const pre = m?.[4] === undefined ? [] : m[4].split('.');
  if (!m || (m[4] !== undefined && (!preRelease || m[4].length > SUFFIX_LIMIT || pre.some(id => id === '' || /^0\d/.test(id)))))
    throw new Error('invalid release version (expected vX.Y.Z or vX.Y.Z-beta.N)');
  const core = m.slice(1, 4).map(Number);
  if (!core.every(Number.isSafeInteger)) throw new Error('release version is too large');
  return {core, pre, text: (tag as string).slice(1)};
}

/* SemVer precedence: the numbers first; equal, a pre-release is lower than none; two pre-releases by their
   identifiers (numeric ones numerically, a numeric lower than a word, words by ASCII, fewer parts lower) */
function compare(a: Version, b: Version): number {
  for (let i = 0; i < 3; i++) if (a.core[i] !== b.core[i]) return a.core[i] < b.core[i] ? -1 : 1;
  if (!a.pre.length || !b.pre.length) return b.pre.length - a.pre.length;
  const numeric = (id: string) => /^\d+$/.test(id);
  for (let i = 0; i < Math.min(a.pre.length, b.pre.length); i++) {
    const x = a.pre[i], y = b.pre[i];
    if (x === y) continue;
    if (numeric(x) && numeric(y)) return Number(x) < Number(y) ? -1 : 1;
    if (numeric(x) !== numeric(y)) return numeric(x) ? -1 : 1;
    return x < y ? -1 : 1;
  }
  return Math.sign(a.pre.length - b.pre.length);
}

export type UpdateResult = {text: string; url?: string};

export async function checkUpdates(about: AboutLine[], signal?: AbortSignal): Promise<UpdateResult> {
  // hello.about's first line is the version (core/xpp_about.cpp): a release tag (v0.1.0), a pre-release tag
  // (v0.1.0-beta.1) or a git-describe build (v0.1.0-12-gabcdef, or v0.1.0-beta.1-12-gabcdef).
  const first = aboutVersionLine(about);
  const found = /^xppautX (v\d+\.\d+\.\d+)(-[0-9A-Za-z.]+?)?(?:-\d+-g[0-9a-f]+)?$/.exec(first);
  /* a pre-release build is told of the newest release including pre-releases, a stable one only of stable ones */
  const preRelease = !!found?.[2];
  const api = preRelease ? RELEASES_API : RELEASE_API;
  let current: Version;
  try { current = version(found ? found[1] + (found[2] ?? '') : undefined); }
  catch (e) { throw located(e, 'hello.about', first, 0); }
  try {
    const timeout = AbortSignal.timeout(CHECK_TIMEOUT_MS);
    const response = await fetch(api, {signal: signal ? AbortSignal.any([signal, timeout]) : timeout, credentials: 'omit'});
    if (!response.ok) throw new Error(`GitHub returned HTTP ${response.status}`);
    const {answer, text} = await releaseAnswer(response, api);
    const entries = preRelease ? answer : [answer];
    if (preRelease && (!Array.isArray(answer) || answer.length === 0)) throw new Error('GitHub returned no releases');
    if (!preRelease && (!answer || typeof answer !== 'object')) throw new Error('GitHub returned no release');
    let latest: Version | undefined, latestUrl = '';
    for (const entry of entries as unknown[]) {
      if (!entry || typeof entry !== 'object') throw located(new Error('GitHub returned a bad release entry'), api, text);
      const {tag_name: tag, html_url: url} = entry as {tag_name?: unknown; html_url?: unknown};
      let v: Version;
      try { v = version(tag, preRelease); }
      catch (e) { throw located(e, api, text, preRelease ? undefined : fieldOffset(text, 'tag_name')); }
      /* the tag is checked before any URL is built from it; the page URL must be exactly its release page */
      if (typeof url !== 'string' || url.length > URL_LIMIT || url !== `${RELEASE_PREFIX}tag/${tag}`)
        throw located(new Error('GitHub returned an invalid release page URL'), api, text, preRelease ? undefined : fieldOffset(text, 'html_url'));
      if (!latest || compare(v, latest) > 0) { latest = v; latestUrl = url; }
    }
    return compare(latest!, current) > 0
      ? {text: `xppautX ${latest!.text} is available. Nothing is downloaded or installed by xppautX.`, url: latestUrl}
      : {text: `xppautX ${current.text} is the latest`};
  } catch (e) { throw e instanceof UpdateError ? e : located(e, api); }
}

export function requestUpdateCheck(): void {
  window.dispatchEvent(new Event('xpp-check-updates'));
}

export async function openRelease(url: string): Promise<void> {
  const bound = (window as unknown as {__xppOpenRelease?: (url: string) => Promise<unknown>}).__xppOpenRelease;
  if (bound) await bound(url);
  else window.open(url, '_blank', 'noopener,noreferrer');
}
