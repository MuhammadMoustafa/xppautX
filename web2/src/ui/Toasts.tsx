/* Non-blocking notifications: the core's errors and alerts. They stay until
   dismissed (errors) or for a few seconds (the rest); none takes the focus. */
import {useEffect} from 'preact/hooks';
import type {Toast} from '../store/state';
import {useSession, useStore} from './context';

const INFO_MS = 6000;

function Item({toast}: {toast: Toast}) {
  const session = useSession();
  const dismiss = () => session.store.dispatch({type: 'dismiss', id: toast.id});
  useEffect(() => {
    if (toast.kind === 'error') return;
    const t = setTimeout(dismiss, INFO_MS);
    return () => clearTimeout(t);
  }, [toast.id]);
  return (
    <li class={`toast ${toast.kind}`} role={toast.kind === 'error' ? 'alert' : 'status'}>
      <span>{toast.text}</span>
      <button class="icon" onClick={dismiss} aria-label="Dismiss">×</button>
    </li>
  );
}

export function Toasts() {
  const toasts = useStore(s => s.toasts);
  return (
    <ul class="toasts" aria-label="Notifications">
      {toasts.map(t => <Item key={t.id} toast={t} />)}
    </ul>
  );
}
