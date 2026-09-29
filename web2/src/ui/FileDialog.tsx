/* The `file` ask (docs/ui-v2.md section 4): the browser's own dialogs, with
   the model's folder as the workspace. Opening a file shows the browser's
   picker, filtered by the ask's `wild`, and copies what the user picks into
   the folder XPP reads (a name taken by other content asks first: Replace,
   Keep both, Cancel); a picker needs a user gesture, and the ask comes from
   the core, so the dialog is one prompt with "Choose file…" (focused: Enter
   opens the picker, Esc cancels). Saving lets XPP write into the folder and
   hands the file to the location picked (showSaveFilePicker) or to the
   browser as a download. */
import {useEffect, useRef, useState} from 'preact/hooks';
import {canPickOpen, canPickSave, pickOpen, pickSave, wildExtensions} from '../pickers';
import type {AskEvent} from '../protocol/types';
import {baseName, safeName} from '../store/files';
import {FILE} from '../store/fieldKinds';
import {useSession, useStore} from './context';
import {Field} from './Field';

/* Enter in a field submits its form (A3), as in AskDialog's forms */
function enterSubmits(e: KeyboardEvent) {
  if (e.key !== 'Enter' || (e.target as HTMLElement).tagName !== 'INPUT') return;
  e.preventDefault();
  (e.currentTarget as HTMLFormElement).requestSubmit();
}

function ReplaceConfirm() {
  const session = useSession();
  const confirm = useStore(s => s.files.confirm)!;
  const box = useRef<HTMLDivElement>(null);
  useEffect(() => box.current?.querySelector<HTMLElement>('[data-choice=keep]')?.focus(), [confirm.name]);
  const onKeyDown = (e: KeyboardEvent) => {
    if (e.key !== 'Escape') return;
    e.preventDefault();
    e.stopPropagation(); /* cancels the copy, not the whole prompt */
    session.resolveReplace('cancel');
  };
  return (
    <div class="file-confirm" role="group" aria-labelledby="file-confirm-text" data-confirm="replace" ref={box}
      onKeyDown={onKeyDown}>
      <p id="file-confirm-text">
        The model's folder already has a <b>{confirm.name}</b> with other content. Replace it, or keep both and
        copy yours as <b>{confirm.keepBoth}</b>?
      </p>
      <div class="dialog-actions">
        <button type="button" data-choice="cancel" onClick={() => session.resolveReplace('cancel')}>Cancel</button>
        <button type="button" data-choice="keep" onClick={() => session.resolveReplace('keep')}>Keep both</button>
        <button type="button" data-choice="replace" class="danger" onClick={() => session.resolveReplace('replace')}>
          Replace
        </button>
      </div>
    </div>
  );
}

/* a file ask for reading: pick, copy into the folder, answer */
function OpenFromComputer({ask}: {ask: AskEvent}) {
  const session = useSession();
  const confirm = useStore(s => s.files.confirm);
  const input = useRef<HTMLInputElement>(null);
  const [copying, setCopying] = useState(false);
  const open = (files: File[]) => {
    if (!files.length) return;
    setCopying(true);
    void session.openFiles(ask, files).finally(() => setCopying(false));
  };
  const choose = async () => {
    if (!canPickOpen()) {
      input.current!.value = '';
      input.current!.click();
      return;
    }
    try {
      const files = await pickOpen(true, ask.wild);
      if (files) open(files);
    } catch {
      input.current!.click(); /* a picker that failed: the plain input still works */
    }
  };
  return (
    <>
      <p>
        Pick the file to open{ask.wild && ask.wild !== '*' ? <> ({ask.wild})</> : null}. XPP reads it from the model's
        folder, so it is copied there first. Pick the files it refers to at the same time (tables, included files)
        to copy them too.
      </p>
      <input ref={input} type="file" multiple accept={wildExtensions(ask.wild).join(',') || undefined} class="visually-hidden" tabIndex={-1} aria-hidden="true"
        data-file-input="open" onChange={e => {
          const el = e.target as HTMLInputElement, files = [...(el.files ?? [])];
          el.value = ''; /* the same file can be picked again (after Cancel at the confirm) */
          open(files);
        }} />
      {confirm && confirm.ask === ask.id ? <ReplaceConfirm /> : (
        <div class="dialog-actions">
          <button type="button" onClick={() => session.cancel(ask)}>Cancel</button>
          <button type="button" class="primary" data-autofocus="" disabled={copying} onClick={() => void choose()}>
            {copying ? 'Copying…' : 'Choose file…'}
          </button>
        </div>
      )}
    </>
  );
}

/* a file ask for writing: a name, then the core writes and the page copies */
function SaveToComputer({ask}: {ask: AskEvent}) {
  const session = useSession();
  const [name, setName] = useState(baseName(ask.file ?? ''));
  const ok = safeName(name);
  const picker = canPickSave();
  const save = async (e: Event) => {
    e.preventDefault();
    if (!ok) return;
    if (!picker) {
      session.saveFile(ask, name, null);
      return;
    }
    const handle = await pickSave(name, ask.wild).catch(() => null);
    if (!handle) return;
    session.saveFile(ask, safeName(handle.name) ? handle.name : name, handle);
  };
  return (
    <form onSubmit={e => void save(e)} onKeyDown={enterSubmits}>
      <p>
        XPP writes the file into the model's folder,{' '}
        {picker ? 'then it is copied to where you choose.' : 'then your browser downloads it.'}
      </p>
      <div class="form-grid">
        <label>
          <span>File name</span>
          <Field id="file-name" spec={FILE} value={name} data-autofocus="" data-file-name="" onInput={setName} />
        </label>
      </div>
      <div class="dialog-actions">
        <button type="button" onClick={() => session.cancel(ask)}>Cancel</button>
        <button type="submit" class="primary" disabled={!ok}>{picker ? 'Save…' : 'Save'}</button>
      </div>
    </form>
  );
}

export function FileAsk({ask}: {ask: AskEvent}) {
  return (
    <div class="file-ask" data-mode={ask.mode ?? 'read'}>
      {ask.mode === 'write' ? <SaveToComputer ask={ask} /> : <OpenFromComputer ask={ask} />}
    </div>
  );
}
