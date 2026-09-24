/* Turns docs/manual/*.md (the manual, docs/roadmap.md W12) into the data
   web2's Help view bundles (build.mjs's `virtual:manual` plugin, below;
   web2/src/help/manual.d.ts is its ambient type for tsc). Node-only: it
   runs at web2 build time, never in the browser, so the markdown parser
   (marked, a devDependency) never reaches app.js -- only its HTML output
   does. Anchors are computed the same way GitHub and pandoc slugify
   headings, so a link written by hand in the manual (`06-...md#total`)
   lands on the heading it names. */
import fs from 'node:fs';
import path from 'node:path';
import {Marked} from 'marked';

const CHAPTER_RE = /^(\d\d)-[a-z0-9-]+\.md$/;
const marked = new Marked();

/** GitHub/pandoc-style: lowercase, spaces to hyphens, punctuation dropped
    (not replaced by a hyphen), so "(T)ext, etc" -> "text-etc" */
export function slugify(text) {
  return text.toLowerCase().replace(/[^\w\- ]+/g, '').trim().replace(/\s+/g, '-');
}

function uniqueSlug(base, seen) {
  const id = base || 'section';
  const n = seen.get(id) ?? 0;
  seen.set(id, n + 1);
  return n === 0 ? id : `${id}-${n}`;
}

/** docs/manual/README.md's numbered chapter list ("1. [Introduction]
    (01-introduction.md) -- ...") is the chapters' titles: most chapters
    also start with the same text as a top `#` heading, but not all do
    (16-quick-reference.md opens with a quoted byline instead), so README
    is the one source asked here rather than each file's own first line. */
function chapterTitles(manualDir) {
  const readme = fs.readFileSync(path.join(manualDir, 'README.md'), 'utf8');
  const titles = new Map();
  for (const m of readme.matchAll(/^\d+\.\s+\[([^\]]+)\]\((\d\d-[a-z0-9-]+)\.md\)/gm)) titles.set(m[2], m[1]);
  return titles;
}

/** docs/manual/NN-name.md -> {id, title, html, headings}; the chapters are
    the manual's own files (docs/manual/README.md is the human index, not a
    chapter: it lists these and the menu/dialog map). */
export function loadManual(manualDir) {
  const files = fs.readdirSync(manualDir).filter(f => CHAPTER_RE.test(f)).sort();
  const titles = chapterTitles(manualDir);
  return files.map(file => {
    const id = file.replace(/\.md$/, '');
    const raw = fs.readFileSync(path.join(manualDir, file), 'utf8');
    const lines = raw.split(/\r?\n/);
    /* the chapter's own `# Title` line is dropped: README's Chapters list
       (above) is shown for it, and the body starts at its own first
       heading (##) instead */
    const hasH1 = lines[0]?.startsWith('# ');
    const title = titles.get(id) ?? (hasH1 ? lines[0].slice(2).trim() : id);
    const body = (hasH1 ? lines.slice(1) : lines).join('\n');

    /* one pass to fix the headings' ids (dedup), a second (marked.parse,
       deterministic on the same input) to render, taking the ids from the
       first pass in the same order so they agree */
    const seen = new Map();
    const headings = marked.lexer(body).filter(t => t.type === 'heading')
      .map(t => ({id: uniqueSlug(slugify(t.text), seen), text: t.text, level: t.depth}));
    let next = 0;
    const renderer = new Marked({
      renderer: {
        heading(token) {
          const h = headings[next++];
          const text = this.parser.parseInline(token.tokens);
          return `<h${token.depth} id="${h.id}">${text}</h${token.depth}>\n`;
        },
      },
    });
    const html = renderer.parse(body);
    return {id, title, html, headings};
  });
}
