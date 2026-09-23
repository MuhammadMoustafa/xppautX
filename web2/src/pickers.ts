/* The browser's own file dialogs (docs/ui-v2.md section 4): the File System
   Access pickers where the browser has them (Chrome, Edge, the VS Code
   webview), else an <input type=file> for opening and a download for
   saving. Thin: what is picked goes to the session. */
import {download} from './plot/export';

/** where showSaveFilePicker put the user's file */
export interface SaveHandle {
  name: string;
  createWritable(): Promise<{write(data: Blob): Promise<void>; close(): Promise<void>}>;
}

interface PickerWindow {
  showOpenFilePicker?: (o?: {multiple?: boolean}) => Promise<{getFile(): Promise<File>}[]>;
  showSaveFilePicker?: (o?: {suggestedName?: string}) => Promise<SaveHandle>;
}

const w = () => window as unknown as PickerWindow;

export const canPickOpen = () => typeof w().showOpenFilePicker === 'function';
export const canPickSave = () => typeof w().showSaveFilePicker === 'function';

const cancelled = (e: unknown) => e instanceof DOMException && e.name === 'AbortError';

/** the files picked, or null when the user closed the dialog */
export async function pickOpen(multiple: boolean): Promise<File[] | null> {
  try {
    const handles = await w().showOpenFilePicker!({multiple});
    return await Promise.all(handles.map(h => h.getFile()));
  } catch (e) {
    if (cancelled(e)) return null;
    throw e;
  }
}

/** where to save, or null when the user closed the dialog */
export async function pickSave(suggestedName: string): Promise<SaveHandle | null> {
  try {
    return await w().showSaveFilePicker!({suggestedName});
  } catch (e) {
    if (cancelled(e)) return null;
    throw e;
  }
}

export async function writeTo(handle: SaveHandle, data: Blob): Promise<void> {
  const out = await handle.createWritable();
  await out.write(data);
  await out.close();
}

export function offerDownload(name: string, data: Blob): void {
  const url = URL.createObjectURL(data);
  download(name, url);
  setTimeout(() => URL.revokeObjectURL(url), 10000);
}
