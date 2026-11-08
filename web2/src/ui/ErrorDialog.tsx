/* An error the core sends (`message` `error`: a command the user asked for
   did not happen) opens one centred dialog with an OK button (docs/ui-v2.md
   A11). Errors that arrive before OK are lines of the same dialog, not a
   stack. Enter and Escape close it, and the focus goes back
   where it was. It is also in Messages, as before. The core is not waiting
   on it (the command that failed has ended), so nothing is blocked. While an
   ask is on screen the dialog waits (renders nothing, keeps its errors): the
   ask is the core's question and keeps the focus; the dialog opens when the
   ask closes. A file the core could not open offers "Add file…" here. An
   error the core places (W140) shows its file and line under its text, and
   the line as written when the core read it. */
import {useSession, useStore} from './context';
import {Dialog} from './Dialog';
import {askIsModal} from './AskDialog';
import {AddFile} from './Toasts';
import {ErrorSource} from './ErrorSource';
import {placeWords} from '../protocol/errors';

export function ErrorDialog() {
  const session = useSession();
  const toasts = useStore(s => s.toasts);
  const errors = toasts.filter(t => t.kind === 'error');
  const modal = useStore(s => askIsModal(s.ask, s.pick, s.diagram.open));
  const shown = errors.length > 0 && !modal;
  if (!shown) return null;
  const close = () => session.store.dispatch({type: 'dismissErrors'});
  return (
    <Dialog title={<span class="error-title">{errors.length > 1 ? `${errors.length} errors` : 'Error'}</span>} name="error" role="alertdialog" className="error-dialog" onClose={close}
      action={{label: 'Dismiss', onClick: close, attrs: {'data-error-ok': ''}}} focusKey={shown}
      onKeyDown={e => {
        if (e.key !== 'Enter') return false;
        e.preventDefault(); e.stopPropagation(); close();
        return true;
      }}
      attrs={{'data-error-dialog': errors.length}}>
      <ul class="error-list">
        {errors.map(t => (
          <li key={t.id} data-error>
            <div class="error-item">
              <span>{t.text}</span>
              {t.place && <div class="error-place" data-error-place>{placeWords(t.place)}</div>}
              {t.place?.source && <ErrorSource p={t.place} />}
            </div>
            {t.action?.kind === 'addFile' && <AddFile toast={t} />}
            {t.offer && <button class="small" data-error-offer onClick={() => {
              const offer = t.offer!;
              close();
              for (const c of offer.commands) session.send(c);
            }}>{t.offer.label}</button>}
          </li>
        ))}
      </ul>
    </Dialog>
  );
}
