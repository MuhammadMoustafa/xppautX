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
import {useRef} from 'preact/hooks';
import {useSession, useStore} from './context';
import {useDialogFocus} from './dialogFocus';
import {askIsModal} from './AskDialog';
import {AddFile} from './Toasts';
import {ErrorSource} from './ErrorSource';
import {placeWords} from '../protocol/errors';

export function ErrorDialog() {
  const session = useSession();
  const toasts = useStore(s => s.toasts);
  const errors = toasts.filter(t => t.kind === 'error');
  const modal = useStore(s => askIsModal(s.ask, s.pick, s.diagram.open));
  const box = useRef<HTMLDivElement>(null);
  const shown = errors.length > 0 && !modal;
  useDialogFocus(box, [shown]);
  if (!shown) return null;
  const close = () => session.store.dispatch({type: 'dismissErrors'});
  return (
    <div class="dialog-backdrop">
      <div class="dialog error-dialog" ref={box} role="alertdialog" aria-modal="true" aria-labelledby="error-title"
        data-error-dialog={errors.length}
        onKeyDown={e => {
          if (e.key === 'Escape' || e.key === 'Enter') { e.preventDefault(); e.stopPropagation(); close(); }
          else if (e.key === 'Tab') {
            const all = [...box.current!.querySelectorAll<HTMLElement>('button:not([disabled]), input')];
            if (!all.length) return;
            const i = all.indexOf(document.activeElement as HTMLElement);
            e.preventDefault();
            all[e.shiftKey ? (i <= 0 ? all.length - 1 : i - 1) : (i === all.length - 1 ? 0 : i + 1)].focus();
          }
        }}>
        <h2 id="error-title" class="error-title">{errors.length > 1 ? `${errors.length} errors` : 'Error'}</h2>
        <ul class="error-list">
          {errors.map(t => (
            <li key={t.id} data-error>
              <div class="error-item">
                <span>{t.text}</span>
                {t.place && <div class="error-place" data-error-place>{placeWords(t.place)}</div>}
                {t.place?.source && <ErrorSource p={t.place} />}
              </div>
              {t.action?.kind === 'addFile' && <AddFile toast={t} />}
            </li>
          ))}
        </ul>
        <div class="dialog-actions">
          <button data-autofocus data-error-ok onClick={close}>OK</button>
        </div>
      </div>
    </div>
  );
}
