/* Search over the manual's rendered HTML (virtual:manual, W12): a small
   regex walk of each chapter's top-level blocks, not a DOM parse, so it
   works the same in the browser and under `node build.mjs --test` (no
   DOMParser there). Built once per set of chapters and cached: the manual
   does not change while the page runs. */
import type {ManualChapter} from 'virtual:manual';

export interface SearchHit {
  chapter: string;
  chapterTitle: string;
  anchor: string;
  heading: string;
  text: string;
}

const ENTITIES: Record<string, string> = {amp: '&', lt: '<', gt: '>', quot: '"', '#39': "'", apos: "'"};

function decode(s: string): string {
  return s.replace(/&(#39|amp|lt|gt|quot|apos);/g, (_, e) => ENTITIES[e]);
}

function plainText(html: string): string {
  return decode(html.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ')).trim();
}

/* a heading (its id captured) or a top-level block (p, li, pre, blockquote,
   td/th for the rare table): good enough for our own generated HTML, which
   never nests one of these inside another */
const BLOCK_RE = /<h([1-6])\s+id="([^"]*)"[^>]*>([\s\S]*?)<\/h\1>|<(p|li|pre|blockquote)[^>]*>([\s\S]*?)<\/\4>/g;

function chapterHits(chapter: ManualChapter): SearchHit[] {
  const hits: SearchHit[] = [];
  let anchor = '', heading = chapter.title;
  for (const m of chapter.html.matchAll(BLOCK_RE)) {
    if (m[1] !== undefined) {
      anchor = m[2];
      heading = plainText(m[3]);
      /* the heading itself is searchable (its own line, under its own anchor) */
      if (heading) hits.push({chapter: chapter.id, chapterTitle: chapter.title, anchor, heading, text: heading});
      continue;
    }
    const text = plainText(m[5]);
    if (text) hits.push({chapter: chapter.id, chapterTitle: chapter.title, anchor, heading, text});
  }
  return hits;
}

let cacheOf: ManualChapter[] | null = null;
let cache: SearchHit[] = [];

function index(chapters: ManualChapter[]): SearchHit[] {
  if (cacheOf !== chapters) {
    cache = chapters.flatMap(chapterHits);
    cacheOf = chapters;
  }
  return cache;
}

const HIT_LIMIT = 40;

/** every indexed line (a paragraph, a list item, a code block, a heading)
    whose text contains `query`, case-insensitively; the heading it falls
    under is what a "?" link and the table of contents also use as the
    anchor's name. Empty or whitespace-only queries match nothing. */
export function searchManual(chapters: ManualChapter[], query: string, limit = HIT_LIMIT): SearchHit[] {
  const q = query.trim().toLowerCase();
  if (!q) return [];
  const out: SearchHit[] = [];
  for (const h of index(chapters)) {
    if (h.text.toLowerCase().includes(q) || h.heading.toLowerCase().includes(q)) {
      out.push(h);
      if (out.length >= limit) break;
    }
  }
  return out;
}
