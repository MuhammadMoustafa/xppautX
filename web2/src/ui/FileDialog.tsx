/* The `file` ask (docs/ui-v2.md section 4): the browser's own dialogs, with
   the model's folder as the workspace. Opening a file shows the browser's
   picker, filtered by the ask's `wild`, and copies what the user picks into
   the folder XPP reads (a name taken by other content asks first: Replace,
   Keep both, Cancel); a picker needs a user gesture, and the ask comes from
   the core, so the dialog is one prompt with "Choose file…" (focused: Enter
   opens the picker, Esc cancels). A save has no dialog here: the page answers
   it at once and the browser downloads the file (session.ts browserSave). */
import {useRef, useState} from 'preact/hooks';
import {canPickOpen, pickOpen, wildExtensions} from '../pickers';
import type {AskEvent} from '../protocol/types';
import {useSession, useStore} from './context';
import {Dialog} from './Dialog';
import {AskShell} from './AskDialog';

/** the replace confirm's text: the name is taken by other content */
function ReplaceText() {
  const confirm = useStore(s => s.files.confirm)!;
  return (
    <p id="file-confirm-text" data-confirm="replace" class="file-confirm">
      The model's folder already has a <b>{confirm.name}</b> with other content.{' '}
      {confirm.keepBoth ? <>Replace it, or keep both and copy yours as <b>{confirm.keepBoth}</b>?</> : 'Replace it?'}
    </p>
  );
}

/** the footer of the replace confirm: Replace is the action, Keep both beside it, the × cancels the copy */
function replaceFooter(session: ReturnType<typeof useSession>, keepBoth: string | null) {
  return {
    action: {label: 'Replace file', onClick: () => session.resolveReplace('replace'), attrs: {'data-choice': 'replace'}},
    extra: keepBoth ? <button type="button" data-choice="keep" onClick={() => session.resolveReplace('keep')}>Keep both</button> : null,
  };
}

/** the replace confirm of an upload no `file` ask is open for (Values >
    Load, Add file…: session.ts upload), in a dialog of its own */
export function ReplaceDialog() {
  const session = useSession();
  const confirm = useStore(s => s.files.confirm);
  if (!confirm || confirm.ask !== null) return null;
  const {action, extra} = replaceFooter(session, confirm.keepBoth);
  return (
    <Dialog title="Copy into the model's folder" name="replacement prompt" onClose={() => session.resolveReplace('cancel')} action={action} extra={extra}
      focusKey={confirm.name} attrs={{'data-replace-dialog': ''}}>
      <ReplaceText />
    </Dialog>
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
  const confirming = !!confirm && confirm.ask === ask.id;
  const footer = confirming ? replaceFooter(session, confirm!.keepBoth) : {action: {label: copying ? 'Copying…' : 'Choose file…', onClick: () => void choose()}, extra: null};
  return (
    <AskShell ask={ask} action={footer.action} extra={footer.extra} onClose={confirming ? () => session.resolveReplace('cancel') : undefined}>
      <div class="file-ask" data-mode="read"
        onDragOver={e => e.preventDefault()} onDrop={e => { e.preventDefault(); open([...(e.dataTransfer?.files ?? [])]); }}>
        <p>
          Pick the file to open{ask.wild && ask.wild !== '*' ? <> ({ask.wild})</> : null}, or drop it here. xppautX reads it from the model's
          folder, so it is copied there first. Pick the files it refers to at the same time (tables, included files)
          to copy them too{wildExtensions(ask.wild).length ? <> (All files in the dialog's type list shows them)</> : null}.
        </p>
        <input ref={input} type="file" multiple accept={wildExtensions(ask.wild).join(',') || undefined} class="visually-hidden" tabIndex={-1} aria-hidden="true"
          data-file-input="open" onChange={e => {
            const el = e.target as HTMLInputElement, files = [...(el.files ?? [])];
            el.value = ''; /* the same file can be picked again (after Cancel at the confirm) */
            open(files);
          }} />
        {confirming && <ReplaceText />}
      </div>
    </AskShell>
  );
}

export function FileAsk({ask}: {ask: AskEvent}) {
  return <OpenFromComputer ask={ask} />;
}
