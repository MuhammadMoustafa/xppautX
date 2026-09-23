/* The `file` ask (docs/ui-v2.md section 4): the browser's own dialogs, with
   the model's folder as the workspace. Opening a file copies what the user
   picks into the folder XPP reads (a name taken by other content asks
   first: Replace, Keep both, Cancel); saving lets XPP write into the folder
   and hands the file to the location picked (showSaveFilePicker) or to the
   browser as a download. The core's own listing stays as a second tab, "In
   the model's folder", answered with `cd`, `wild` and `file` as always. */
import {useEffect, useRef, useState} from 'preact/hooks';
import {canPickOpen, canPickSave, pickOpen, pickSave} from '../pickers';
import type {AskEvent} from '../protocol/types';
import {baseName, safeName} from '../store/files';
import {useSession, useStore} from './context';

type Tab = 'computer' | 'folder';

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
      const files = await pickOpen(true);
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
      <input ref={input} type="file" multiple class="visually-hidden" tabIndex={-1} aria-hidden="true"
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
    const handle = await pickSave(name).catch(() => null);
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
          <input value={name} data-autofocus="" data-file-name="" aria-invalid={!ok}
            aria-describedby={ok ? undefined : 'file-name-error'}
            onInput={e => setName((e.target as HTMLInputElement).value)} />
        </label>
      </div>
      {!ok && (
        <p id="file-name-error" class="field-error">
          A name only: no folders, no leading dot, none of \ / : * ? " &lt; &gt; |.
        </p>
      )}
      <div class="dialog-actions">
        <button type="button" onClick={() => session.cancel(ask)}>Cancel</button>
        <button type="submit" class="primary" disabled={!ok}>{picker ? 'Save…' : 'Save'}</button>
      </div>
    </form>
  );
}

/* the core's own listing (the X11 file selector's), for a file elsewhere on its machine */
function InTheFolder({ask}: {ask: AskEvent}) {
  const session = useSession();
  const [file, setFile] = useState(ask.file ?? '');
  const [wild, setWild] = useState(ask.wild ?? '*');
  const dirs = (ask.dirs ?? []).filter(d => d !== '.' && d !== '..');
  return (
    <form onKeyDown={enterSubmits} onSubmit={e => {
      e.preventDefault();
      session.answer(ask, {file});
    }}>
      <p class="muted file-dir">{ask.dir}</p>
      <div class="form-grid">
        <label>
          <span>File</span>
          <input value={file} data-folder-file="" onInput={e => setFile((e.target as HTMLInputElement).value)} />
        </label>
        <label>
          <span>Show</span>
          <input value={wild} title="Which files to list; Enter lists again"
            onInput={e => setWild((e.target as HTMLInputElement).value)}
            onKeyDown={e => {
              if (e.key !== 'Enter') return;
              e.preventDefault();
              e.stopPropagation();
              session.answer(ask, {wild});
            }} />
        </label>
      </div>
      <ul class="file-list" aria-label="Files and folders">
        <li><button type="button" class="file-entry dir" onClick={() => session.answer(ask, {cd: '..'})}>../</button></li>
        {dirs.map(d => (
          <li key={`d${d}`}>
            <button type="button" class="file-entry dir" onClick={() => session.answer(ask, {cd: d})}>{d}/</button>
          </li>
        ))}
        {(ask.files ?? []).map(f => (
          <li key={`f${f}`}>
            <button type="button" class={'file-entry' + (f === file ? ' selected' : '')} aria-pressed={f === file}
              onClick={() => setFile(f)} onDblClick={() => session.answer(ask, {file: f})}>{f}</button>
          </li>
        ))}
      </ul>
      <div class="dialog-actions">
        <button type="button" onClick={() => session.cancel(ask)}>Cancel</button>
        <button type="submit" class="primary">{ask.mode === 'write' ? 'Save' : 'Open'}</button>
      </div>
    </form>
  );
}

export function FileAsk({ask}: {ask: AskEvent}) {
  const [tab, setTab] = useState<Tab>('computer');
  const tabs: [Tab, string][] = [['computer', 'This computer'], ['folder', "In the model's folder"]];
  const onKeyDown = (e: KeyboardEvent) => {
    if (e.key !== 'ArrowLeft' && e.key !== 'ArrowRight') return;
    e.preventDefault();
    const next = tab === 'computer' ? 'folder' : 'computer';
    setTab(next);
    document.getElementById(`file-tab-${next}`)?.focus();
  };
  return (
    <div class="file-ask" data-mode={ask.mode ?? 'read'}>
      <div class="file-tabs" role="tablist" aria-label="Where the file is" onKeyDown={onKeyDown}>
        {tabs.map(([t, label]) => (
          <button key={t} id={`file-tab-${t}`} type="button" role="tab" class="plot-tab" aria-selected={tab === t}
            aria-controls={`file-panel-${t}`} tabIndex={tab === t ? 0 : -1} onClick={() => setTab(t)}>
            {label}
          </button>
        ))}
      </div>
      <div id={`file-panel-${tab}`} role="tabpanel" aria-labelledby={`file-tab-${tab}`}>
        {tab === 'folder' ? <InTheFolder ask={ask} />
          : ask.mode === 'write' ? <SaveToComputer ask={ask} /> : <OpenFromComputer ask={ask} />}
      </div>
    </div>
  );
}
