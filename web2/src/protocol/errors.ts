/* An error's place, as the core sends it on every error event (`error`,
   `message` `error`: ErrorFields, docs/protocol.md "Errors", W140), in the
   page's words. Pure: no DOM. */
import type {ErrorFields} from './types';

/** where an error is: its file, line, column and the line as written */
export type ErrorPlace = Omit<ErrorFields, 'error'>;

/** the place of an error event, or nothing when it names none */
export function errorPlace(e: Partial<ErrorFields>): ErrorPlace | undefined {
  const file = e.file ?? '', line = e.line ?? 0;
  if (!file && line <= 0) return undefined;
  return {file, line, col: e.col ?? 0, source: e.source ?? ''};
}

/** "file:line:col: what", leaving out what is not known ("line N: what"
    with no file): the core's own rendering (xpp::Error::text, core/xpp_error.h),
    so Messages read as the console does */
export function errorText(e: Partial<ErrorFields>): string {
  const p = errorPlace(e);
  const what = e.error ?? '';
  if (!p) return what;
  let t = p.file;
  if (p.line > 0) t += (t ? ':' : 'line ') + p.line + (p.col > 0 ? ':' + p.col : '');
  return `${t}: ${what}`;
}

/** the place in words: the file, its line and column when known */
export function placeWords(p: ErrorPlace): string {
  if (p.line <= 0) return p.file;
  return (p.file ? `${p.file}, line ${p.line}` : `line ${p.line}`) + (p.col > 0 ? `, column ${p.col}` : '');
}

/** a file the command could not read (a file and no line): the page offers to add it under its own name */
export function unreadFile(e: Partial<ErrorFields>): string | undefined {
  if (!e.file || (e.line ?? 0) > 0) return undefined;
  return e.file.split(/[\\/]/).pop() || undefined;
}
