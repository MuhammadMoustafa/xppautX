/* The Help view (docs/roadmap.md W12: the manual, step 3): a table of
   contents, a search box (help/search.ts, over the manual's headings and
   text), and the chapter itself, rendered from its already-built HTML
   (virtual:manual, tools/manualBuild.mjs -- marked runs at build time, not
   here). Links between chapters and to a heading (`href="06-....md#..."`,
   written by hand in docs/manual/) are caught on the content and turned
   into an in-app navigation (help/links.ts manualLinkTarget) instead of a
   dead reload. Follows the floating-overlay pattern of the other panels
   (ui/TableView.tsx): a side panel from 48rem, a full-screen sheet under
   that, opened by the title bar's Help button, a "?" link (ui/HelpButton.tsx)
   anywhere in the app, or F1 (not stealing it from a focused text field's
   own default, if any). */
import {useEffect, useMemo, useRef} from 'preact/hooks';
import manual from 'virtual:manual';
import {manualLinkTarget, type HelpTarget} from '../help/links';
import {searchManual} from '../help/search';
import {useSession, useStore} from './context';

const FOCUSABLE = 'button:not([disabled]), input, select, [tabindex]:not([tabindex="-1"])';
/** F1's own exclusion (narrower than hotkeys.ts's TYPING: a dialog or menu
    being open must not block Help -- only a field that might have its own
    F1 meaning does) */
const TEXT_TYPING = 'input, textarea, select, [contenteditable]';

function chapterOf(id: string) {
  return manual.find(c => c.id === id) ?? manual[0];
}

export function HelpView() {
  const session = useSession();
  const help = useStore(s => s.help);
  const panel = useRef<HTMLElement>(null);
  const content = useRef<HTMLDivElement>(null);
  const close = () => session.store.dispatch({type: 'help', action: {type: 'close'}});
  const go = (target: HelpTarget) => session.store.dispatch({type: 'help', action: {type: 'open', target}});

  /* F1 opens Help from anywhere in the page, wherever it was left (the
     'open' reducer case keeps the chapter and anchor when no target is given) */
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== 'F1' || e.defaultPrevented) return;
      if ((e.target as HTMLElement | null)?.closest?.(TEXT_TYPING)) return;
      e.preventDefault();
      session.store.dispatch({type: 'help', action: {type: 'open'}});
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [session]);

  /* focus in on open, back to the toggle on close; Escape closes it,
     wherever the focus is inside (narrow only: CSS keeps it open elsewhere) */
  useEffect(() => {
    if (!help.open) {
      if (panel.current?.contains(document.activeElement)) document.querySelector<HTMLElement>('.help-toggle')?.focus();
      return;
    }
    panel.current?.querySelector<HTMLElement>(FOCUSABLE)?.focus();
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== 'Escape' || session.store.getState().ask) return;
      e.preventDefault();
      e.stopPropagation();
      close();
    };
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, [help.open]);

  /* the anchor a "?" link, a chapter link or a search result asked for
     scrolls into view once its chapter has rendered; none: back to the top */
  useEffect(() => {
    if (!help.open) return;
    const el = help.anchor && content.current?.querySelector(`#${CSS.escape(help.anchor)}`);
    if (el) el.scrollIntoView({block: 'start'});
    else content.current?.scrollTo(0, 0);
  }, [help.open, help.chapter, help.anchor]);

  const chapter = chapterOf(help.chapter);
  const results = useMemo(() => searchManual(manual, help.query), [help.query]);

  /* a click on a link the chapter's own HTML has (dangerouslySetInnerHTML,
     below): a manual cross-reference navigates here, anything else (mailto:,
     an external doc) is left to the browser */
  const onContentClick = (e: MouseEvent) => {
    const a = (e.target as HTMLElement).closest?.('a[href]');
    if (!a) return;
    const target = manualLinkTarget(a.getAttribute('href') ?? '', chapter.id);
    if (!target) return;
    e.preventDefault();
    go(target);
  };

  return (
    <section id="help-panel" ref={panel} class={'help-panel' + (help.open ? ' open' : '')} aria-label="Help">
      <div class="help-header">
        <button class="help-back" onClick={close}>Back</button>
        <h2>Help</h2>
      </div>
      <label class="help-search">
        <span class="visually-hidden">Search the manual</span>
        <input type="search" placeholder="Search the manual" value={help.query}
          onInput={e => session.store.dispatch(
            {type: 'help', action: {type: 'query', query: (e.target as HTMLInputElement).value}})} />
      </label>
      {help.query.trim() !== '' && (
        <ul class="help-results" aria-label="Search results">
          {results.length === 0 && <li class="muted help-no-results">No match.</li>}
          {results.map((r, i) => (
            <li key={i}>
              <button type="button" class="help-result" onClick={() => go({chapter: r.chapter, anchor: r.anchor})}>
                <span class="help-result-heading">{r.chapterTitle} · {r.heading}</span>
                <span class="help-result-text">{r.text}</span>
              </button>
            </li>
          ))}
        </ul>
      )}
      <div class="help-body">
        <nav class="help-toc" aria-label="Chapters">
          <ol>
            {manual.map((c, i) => (
              <li key={c.id}>
                <button type="button" class={'help-toc-item' + (c.id === chapter.id ? ' current' : '')}
                  aria-current={c.id === chapter.id ? 'page' : undefined} onClick={() => go({chapter: c.id})}>
                  {i + 1}. {c.title}
                </button>
              </li>
            ))}
          </ol>
        </nav>
        <div class="help-content" ref={content} onClick={onContentClick}>
          <h1>{chapter.title}</h1>
          <div dangerouslySetInnerHTML={{__html: chapter.html}} />
        </div>
      </div>
    </section>
  );
}
