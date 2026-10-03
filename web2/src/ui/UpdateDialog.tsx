import {useEffect, useRef, useState} from 'preact/hooks';
import {checkUpdates, openRelease, UpdateError, type UpdateResult} from '../help/updates';
import {placeWords, type ErrorPlace} from '../protocol/errors';
import {ErrorSource} from './ErrorSource';
import {useSession} from './context';
import {useDialogFocus} from './dialogFocus';

export function UpdateDialog() {
  const session = useSession();
  const [result, setResult] = useState<UpdateResult | null>(null);
  const [place, setPlace] = useState<ErrorPlace>();
  const checking = useRef(false);
  const controller = useRef<AbortController>();
  const generation = useRef(0);
  const box = useRef<HTMLDivElement>(null);
  useDialogFocus(box, [!!result]);
  useEffect(() => {
    const run = async () => {
      if (checking.current) return;
      checking.current = true;
      const started = ++generation.current;
      const request = new AbortController();
      controller.current = request;
      setPlace(undefined);
      setResult({text: 'Checking for updates…'});
      try {
        const answer = await checkUpdates(session.store.getState().hello?.about ?? '', request.signal);
        if (started === generation.current) setResult(answer);
      }
      catch (e) {
        if (started === generation.current) {
          setPlace(e instanceof UpdateError ? e.place : undefined);
          setResult({text: `Check for updates failed: ${e instanceof Error ? e.message : String(e)}`});
        }
      }
      finally { if (started === generation.current) { checking.current = false; controller.current = undefined; } }
    };
    window.addEventListener('xpp-check-updates', run);
    return () => {
      window.removeEventListener('xpp-check-updates', run);
      generation.current++;
      controller.current?.abort();
      controller.current = undefined;
      checking.current = false;
    };
  }, [session]);
  if (!result) return null;
  const close = () => { generation.current++; controller.current?.abort(); controller.current = undefined; checking.current = false; setResult(null); setPlace(undefined); };
  return <div class="dialog-backdrop">
    <div class="dialog" ref={box} role="dialog" aria-modal="true" aria-labelledby="update-title" data-update-dialog
      onKeyDown={e => {
        if (e.key === 'Escape') { e.stopPropagation(); close(); }
        if (e.key === 'Tab') {
          e.preventDefault();
          const buttons = [...box.current!.querySelectorAll<HTMLButtonElement>('button')];
          buttons[(buttons.indexOf(document.activeElement as HTMLButtonElement) + (e.shiftKey ? buttons.length - 1 : 1)) % buttons.length].focus();
        }
      }}>
      <h2 id="update-title">Check for updates</h2>
      <p>{result.text}</p>
      {place && <><p data-error-place>{placeWords(place)}</p><ErrorSource p={place}/></>}
      <div class="dialog-actions">
        {result.url && <button onClick={() => {
          void openRelease(result.url!).catch(e => setResult({text: `Opening the release page failed: ${String(e)}`}));
        }}>Open the release page</button>}
        <button onClick={close}>Close</button>
      </div>
    </div>
  </div>;
}
