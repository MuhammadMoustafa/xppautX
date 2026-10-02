/* Files (docs/ui-v2.md section 4, T5): the model's folder is the workspace.
   A `file` ask for reading is answered by uploading what the user picks
   into that folder (the /files endpoints of core/xpp_http.cpp), one for
   writing by letting the core write there and offering the file back to
   the browser. This slice keeps what tests and the dialog look at: the
   folder's listing, the replace confirm that is open, the last uploads and
   the last file offered to the browser, and the command that is running
   (its first command and the answers to its prompts), so a command whose
   file was missing can run again once the file is added. Pure: no DOM, no
   I/O. */
import {isStep, menuKey, menuName, menuNumber} from '../protocol/kinds';
import type {AskEvent, Command, HelloEvent} from '../protocol/types';

/** one file of GET /files */
export interface FolderFile {
  name: string;
  size: number;
  mtime: number;
  sha256: string;
}

/** what an upload asks when the name it goes under is taken by other content
    (session.ts upload: every upload of the page) */
export interface ReplaceConfirm {
  /** the ask the upload answers (the file dialog shows the confirm), or null:
      an upload of the page's own (Values > Load, Add file…), its own dialog */
  ask: number | null;
  name: string;
  /** the name Keep both uploads it under; null: the upload needs this name
      (Add file… copies the file the core asked for), no Keep both */
  keepBoth: string | null;
}

export type ReplaceChoice = 'replace' | 'keep' | 'cancel';

export interface Upload {
  /** the picked file's own name */
  picked: string;
  /** the name it has in the model's folder */
  name: string;
  sha256: string;
  /** false: the folder had it already, with the same content */
  copied: boolean;
}

/** a file the core wrote, handed to the browser */
export interface Offered {
  name: string;
  size: number;
  sha256: string;
  /** into the location showSaveFilePicker gave, or as a download */
  how: 'picker' | 'download';
}

/** an answer of the running command, with the prompt it answered */
export interface RunAnswer {
  kind: string;
  mode?: string;
  fields: Record<string, unknown>;
}

/** the running (or last) command as the user gave it */
export interface RunRecord {
  /** the main menu (state.menu) when it was sent: File/Import XPPAUT set is a key of the File menu */
  menu: number;
  cmd: Command;
  answers: RunAnswer[];
}

export interface FilesState {
  listing: FolderFile[] | null;
  confirm: ReplaceConfirm | null;
  uploads: Upload[];
  offered: Offered | null;
  run: RunRecord | null;
  /** the command reported an error: what it wrote is not offered */
  runFailed: boolean;
  /** the main menu the next command runs in, when a command just sent
      switches it (File, nUmerics) before the core has said so */
  nextMenu: number | null;
}

export const initialFiles: FilesState =
  {listing: null, confirm: null, uploads: [], offered: null, run: null, runFailed: false, nextMenu: null};

export type FilesAction =
  | {type: 'listing'; files: FolderFile[]}
  | {type: 'confirm'; confirm: ReplaceConfirm | null}
  | {type: 'uploaded'; uploads: Upload[]}
  | {type: 'offered'; offered: Offered};

export function reduceFiles(state: FilesState, action: FilesAction): FilesState {
  switch (action.type) {
    case 'listing':
      return {...state, listing: action.files};
    case 'confirm':
      return {...state, confirm: action.confirm};
    case 'uploaded':
      return {...state, uploads: action.uploads};
    case 'offered':
      return {...state, offered: action.offered};
  }
}

/** a command was sent while `ask` was open and the main menu was `menu`: a step
    of the user's (hello.commands' step: not an answer, a query or a data
    subscription) starts a new run record */
export function onSent(state: FilesState, hello: HelloEvent | null, cmd: Command, ask: AskEvent | null, menu: number): FilesState {
  if (cmd.cmd === 'answer') {
    if (!state.run || !ask) return state;
    const {cmd: _c, id: _i, ...fields} = cmd;
    const answer: RunAnswer = {kind: ask.kind, fields};
    if (typeof ask.mode === 'string') answer.mode = ask.mode;
    return {...state, run: {...state.run, answers: [...state.run.answers, answer]}};
  }
  if (!hello || !isStep(hello, cmd)) return state;
  const at = state.nextMenu ?? menu, key = cmd.cmd === 'key' && !cmd.win ? String(cmd.key) : '';
  /* File and nUmerics in the main menu switch to those menus (core/commands.cpp commander) */
  const main = menuName(hello, at) === 'main';
  const nextMenu = main && key === menuKey(hello, 'main', 'file') ? menuNumber(hello, 'file')
    : main && key === menuKey(hello, 'main', 'numerics') ? menuNumber(hello, 'num') : null;
  return {...state, run: {menu: at, cmd, answers: []}, runFailed: false, nextMenu};
}

/* ---- names ---------------------------------------------------------------------- */

/** the last part of a path, either separator */
export function baseName(path: string): string {
  return path.slice(Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\')) + 1);
}

/** the rule of core/xpp_files.cpp xpp_files_name_ok, so the page never sends a name the core refuses */
export function safeName(name: string): boolean {
  if (!name || name.length > 255 || name.startsWith('.') || name.startsWith(' ') || name.includes('..')) return false;
  if (new TextEncoder().encode(name).length > 255) return false;
  if (/[\u0000-\u001f\u007f/\\:<>"|?*]/.test(name) || /[. ]$/.test(name)) return false;
  const stem = name.split('.')[0].replace(/ +$/, '').toLowerCase();
  return !/^(con|prn|aux|nul|conin\$|conout\$|com[0-9]|lpt[0-9])$/.test(stem);
}

/** name-2.ext, name-3.ext, ...: the first one the folder does not have */
export function keepBothName(name: string, taken: Iterable<string>): string {
  const have = new Set(taken);
  const dot = name.lastIndexOf('.');
  const stem = dot > 0 ? name.slice(0, dot) : name, ext = dot > 0 ? name.slice(dot) : '';
  for (let i = 2; ; i++) {
    const n = `${stem}-${i}${ext}`;
    if (!have.has(n)) return n;
  }
}

/** does `name` match the ask's pattern ("*.set", "*.pars*")? */
export function matchesWild(name: string, wild: string | undefined): boolean {
  if (!wild || wild === '*') return true;
  const re = new RegExp('^' + wild.split('*').map(s => s.replace(/[.+?^${}()|[\]\\]/g, '\\$&')).join('.*') + '$', 'i');
  return re.test(name);
}

/** of several files picked at once (a .set and the tables it uses), the one
    the ask is answered with: the first that matches its pattern */
export function answerName(names: string[], wild: string | undefined): string {
  return names.find(n => matchesWild(n, wild)) ?? names[0];
}

/** what uploading `picked` (its digest) as `name` needs, against the folder */
export function uploadPlan(name: string, sha256: string, listing: FolderFile[]): 'copy' | 'same' | 'confirm' {
  const there = listing.find(f => f.name === name);
  return !there ? 'copy' : there.sha256 === sha256 ? 'same' : 'confirm';
}

/** the keys that bring the main menu from `now` to `want` before a command
    runs again (the File and nUmerics menus go back to the main one after a
    command), or null when that cannot be done */
export function menuKeys(hello: HelloEvent | null, now: number, want: number): string[] | null {
  if (now === want) return [];
  if (menuName(hello, now) !== 'main') return null;
  const to = menuName(hello, want);
  return to === 'file' ? [menuKey(hello, 'main', 'file')] : to === 'num' ? [menuKey(hello, 'main', 'numerics')] : null;
}
