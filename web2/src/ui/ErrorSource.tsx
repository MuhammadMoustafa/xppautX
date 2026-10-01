/* An error's line as written, its number beside it and a caret under the
   column when there is one (W63c, W140): the load error view (App.tsx) and
   the error dialog (ErrorDialog.tsx) show an error's place alike. */
import type {ErrorPlace} from '../protocol/errors';

export function ErrorSource({p}: {p: ErrorPlace}) {
  if (p.line <= 0) return null;
  return (
    <div class="source-lines load-error-source" aria-label={`Line ${p.line} of ${p.file}`} data-error-source>
      <div class="source-row">
        <span class="source-lineno" aria-hidden="true">{p.line}</span>
        <span class="source-text">{p.source || ' '}</span>
      </div>
      {p.col > 0 && (
        <div class="source-row" aria-hidden="true">
          <span class="source-lineno" />
          <span class="source-text load-error-caret">{' '.repeat(p.col - 1) + '^'}</span>
        </div>
      )}
    </div>
  );
}
