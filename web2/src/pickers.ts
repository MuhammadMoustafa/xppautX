/* The browser's own file dialogs (docs/ui-v2.md section 4): the File System
   Access pickers where the browser has them (Chrome, Edge, the VS Code
   webview), else an <input type=file> for opening and a download for
   saving. Thin: what is picked goes to the session. */
/** an anchor click: the browser's own "Save As" for `url` (an object URL
    or a data URL), named `name`. The one place a download is triggered
    (docs/roadmap.md W66: every file itself is written by the core; the
    page only offers what the core wrote as a download or through a save
    picker). */
export function download(name: string, url: string): void {
  const a = document.createElement('a');
  a.href = url;
  a.download = name;
  document.body.appendChild(a);
  a.click();
  a.remove();
}

interface PickerWindow {
  showOpenFilePicker?: (o?: {
    multiple?: boolean;
    excludeAcceptAllOption?: boolean;
    types?: {description?: string; accept: Record<string, string[]>}[];
  }) => Promise<{getFile(): Promise<File>}[]>;
}

const w = () => window as unknown as PickerWindow;

export const canPickOpen = () => typeof w().showOpenFilePicker === 'function';

const cancelled = (e: unknown) => e instanceof DOMException && e.name === 'AbortError';

/** the extensions an ask's `wild` pattern ("*.set", "*.dat *.tab") names, for a
    picker's filter: [] (no filter) for "*", for a pattern that is not just
    "*.ext" words (a "*.pars*" cannot be an extension), and when any is odd */
export function wildExtensions(wild: string | undefined): string[] {
  const words = (wild ?? '').split(/[\s,;|]+/).filter(Boolean);
  if (!words.length) return [];
  const exts: string[] = [];
  for (const word of words) {
    const m = /^\*(\.[A-Za-z0-9_-]{1,15})$/.exec(word);
    if (!m) return [];
    exts.push(m[1].toLowerCase());
  }
  return exts;
}

/** the files picked, or null when the user closed the dialog; `wild` filters
    (the All files option stays, for the tables a .set refers to) */
export async function pickOpen(multiple: boolean, wild?: string): Promise<File[] | null> {
  try {
    const exts = wildExtensions(wild);
    const handles = await w().showOpenFilePicker!(exts.length
      ? {multiple, excludeAcceptAllOption: false, types: [{description: wild, accept: {'application/octet-stream': exts}}]}
      : {multiple});
    return await Promise.all(handles.map(h => h.getFile()));
  } catch (e) {
    if (cancelled(e)) return null;
    throw e;
  }
}

export function offerDownload(name: string, data: Blob): void {
  const url = URL.createObjectURL(data);
  download(name, url);
  setTimeout(() => URL.revokeObjectURL(url), 10000);
}

/** what the desktop window's own file dialog is asked (core/xpp_window.cpp) */
export interface NativeFileRequest {
  mode: 'read' | 'write';
  ask?: number; /* binds native write authorization to the core prompt */
  title: string;
  /** the folder shown first ('': the system's choice) */
  dir: string;
  /** the name offered */
  file: string;
  /** the filter's name */
  wild: string;
  /** what it filters by (wildExtensions); All files stays */
  exts: string[];
  /** the name written is the stem of several files (W794): the window grants the picked path's folder to the files written in it, in place of
      the one path of a save; __xppReleaseFolder ends the grant */
  several?: boolean;
}

/** the operating system's own open or save dialog (docs/ui-v2.md section 4,
    W88): the desktop window binds it into the page as `__xppFileDialog`;
    it resolves with the full path picked, or null when the user cancelled,
    and rejects when it could not open. Undefined in a browser. */
export type NativeFileDialog = (request: NativeFileRequest) => Promise<string | null>;

export const nativeFileDialog = (): NativeFileDialog | undefined => {
  if (typeof window === 'undefined') return undefined;
  const f = (window as unknown as {__xppFileDialog?: unknown}).__xppFileDialog;
  return typeof f === 'function' ? f as NativeFileDialog : undefined;
};

/** a file ask's native dialog: its folder is the one `file` names when that
    is a full path, else the ask's `dir`; the name offered is `file`'s base
    name; the filter is wildExtensions' reading of `wild` */
export function nativeFileRequest(ask: {title?: string; mode?: string; file?: string; wild?: string; dir?: string}): NativeFileRequest {
  const file = ask.file ?? '';
  const cut = Math.max(file.lastIndexOf('/'), file.lastIndexOf('\\'));
  const absolute = /^([A-Za-z]:)?[\\/]/.test(file);
  return {
    mode: ask.mode === 'write' ? 'write' : 'read',
    title: ask.title ?? '',
    dir: cut >= 0 && absolute ? file.slice(0, cut + 1) : ask.dir ?? '',
    file: file.slice(cut + 1),
    wild: ask.wild ?? '',
    exts: wildExtensions(ask.wild),
  };
}

/** the save dialog of File > Export > Several (W794): one name for every file, no type filter, `dir` shown first and `file` offered; the full
    path picked (the window has granted its folder to the files written in it), or null when the user cancelled. A browser has no such
    dialog: undefined there, its exports are downloads. */
export function pickStem(title: string, dir: string, file: string): Promise<string | null> | undefined {
  const dialog = nativeFileDialog();
  return dialog?.({mode: 'write', title, dir, file, wild: '', exts: [], several: true});
}
/** the folder grant of pickStem is over (the batch is done) */
export async function releaseFolder(): Promise<void> {
  const f = typeof window === 'undefined' ? undefined : (window as unknown as {__xppReleaseFolder?: unknown}).__xppReleaseFolder;
  if (typeof f === 'function') await (f as () => Promise<null>)();
}
