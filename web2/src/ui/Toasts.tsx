/* Non-blocking notifications: the core's alerts and other information, for a
   few seconds; none takes the focus. Errors are the error dialog's
   (ErrorDialog.tsx), which uses AddFile below: an error about a file the core could not open offers "Add file…", which
   copies the file picked into the model's folder under that name and runs
   the command again (docs/ui-v2.md section 4). */
import {useEffect, useRef} from 'preact/hooks';
import {canPickOpen, pickOpen} from '../pickers';
import type {Toast} from '../store/state';
import {useSession, useStore} from './context';

const INFO_MS = 6000;

export function AddFile({toast}: {toast: Toast}) {
  const session = useSession();
  const input = useRef<HTMLInputElement>(null);
  const add = (files: File[] | null) => {
    if (files?.length) void session.addMissingFile(toast.id, files[0]);
  };
  const choose = async () => {
    if (!canPickOpen()) {
      input.current!.value = '';
      input.current!.click();
      return;
    }
    add(await pickOpen(false).catch(() => null));
  };
  return (
    <>
      <input ref={input} type="file" class="visually-hidden" tabIndex={-1} aria-hidden="true" data-file-input="add"
        onChange={e => {
          const el = e.target as HTMLInputElement, files = [...(el.files ?? [])];
          el.value = '';
          add(files);
        }} />
      <button class="small" data-add-file={toast.action!.name} onClick={() => void choose()}
        title={`Copy a file into the model's folder as ${toast.action!.name} and run the command again`}>
        Add file…
      </button>
    </>
  );
}

function Item({toast}: {toast: Toast}) {
  const session = useSession();
  const dismiss = () => session.store.dispatch({type: 'dismiss', id: toast.id});
  useEffect(() => {
    const t = setTimeout(dismiss, INFO_MS);
    return () => clearTimeout(t);
  }, [toast.id]);
  return (
    <li class={`toast ${toast.kind}`} role="status">
      <span>{toast.text}{toast.action && !toast.text.includes(toast.action.name) && <> ({toast.action.name})</>}</span>
      {toast.action?.kind === 'addFile' && <AddFile toast={toast} />}
      <button class="icon" onClick={dismiss} aria-label="Dismiss">×</button>
    </li>
  );
}

export function Toasts() {
  const toasts = useStore(s => s.toasts);
  return (
    <ul class="toasts" aria-label="Notifications">
      {toasts.filter(t => t.kind !== 'error').map(t => <Item key={t.id} toast={t} />)}
    </ul>
  );
}
