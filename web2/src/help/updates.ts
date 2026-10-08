import {sourcePlace, type ErrorPlace} from '../protocol/errors';
import type {AboutLine} from '../protocol/types';
import {aboutVersionLine} from './about';

/* W13c: an on-demand GitHub check. No assets or release notes are read. */
export const RELEASE_API = 'https://api.github.com/repos/MuhammadMoustafa/xppautX/releases/latest';
export const RELEASE_PREFIX = 'https://github.com/MuhammadMoustafa/xppautX/releases/';
const TAG_LIMIT = 64; // permits three safe integer components without unbounded API text
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

function parseAnswer(text: string): unknown {
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
    throw located(e, RELEASE_API, text, offset);
  }
}

async function releaseAnswer(response: Response): Promise<{answer: unknown; text: string}> {
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
    return {answer: parseAnswer(text), text};
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

export async function checkUpdates(about: AboutLine[], signal?: AbortSignal): Promise<UpdateResult> {
  // hello.about's first line is the version (core/xpp_about.cpp): a release tag (v0.1.0), a pre-release tag
  // (v0.1.0-beta.1) or a git-describe build (v0.1.0-12-gabcdef, or v0.1.0-beta.1-12-gabcdef).
  const first = aboutVersionLine(about);
  const found = /^xppautX (v\d+\.\d+\.\d+)(-[0-9A-Za-z.]+?)?(?:-\d+-g[0-9a-f]+)?$/.exec(first);
  const local = found?.[1];
  /* a pre-release is older than the release of its own numbers, which `releases/latest` can name */
  const preRelease = !!found?.[2];
  let current: number[];
  try { current = version(local); }
  catch (e) { throw located(e, 'hello.about', first, 0); }
  try {
    const timeout = AbortSignal.timeout(CHECK_TIMEOUT_MS);
    const response = await fetch(RELEASE_API, {signal: signal ? AbortSignal.any([signal, timeout]) : timeout, credentials: 'omit'});
    if (!response.ok) throw new Error(`GitHub returned HTTP ${response.status}`);
    const {answer, text} = await releaseAnswer(response);
    if (!answer || typeof answer !== 'object') throw new Error('GitHub returned no release');
    const {tag_name: tag, html_url: url} = answer as {tag_name?: unknown; html_url?: unknown};
    let latest: number[];
    try { latest = version(tag); }
    catch (e) { throw located(e, RELEASE_API, text, fieldOffset(text, 'tag_name')); }
    if (typeof url !== 'string' || url.length > URL_LIMIT || url !== `${RELEASE_PREFIX}tag/${tag}`)
      throw located(new Error('GitHub returned an invalid release page URL'), RELEASE_API, text, fieldOffset(text, 'html_url'));
    const differing = latest.findIndex((n, i) => n !== current[i]);
    const newer = differing >= 0 ? latest[differing] > current[differing] : preRelease;
    return newer
      ? {text: `xppautX ${latest.join('.')} is available. Nothing is downloaded or installed by xppautX.`, url}
      : {text: `xppautX ${current.join('.')} is the latest`};
  } catch (e) { throw e instanceof UpdateError ? e : located(e, RELEASE_API); }
}

export function requestUpdateCheck(): void {
  window.dispatchEvent(new Event('xpp-check-updates'));
}

export async function openRelease(url: string): Promise<void> {
  const bound = (window as unknown as {__xppOpenRelease?: (url: string) => Promise<unknown>}).__xppOpenRelease;
  if (bound) await bound(url);
  else window.open(url, '_blank', 'noopener,noreferrer');
}
