import {useEffect, useRef, useState} from 'preact/hooks';
import {checkUpdates, openRelease, UpdateError, type UpdateResult} from '../help/updates';
import {placeWords, type ErrorPlace} from '../protocol/errors';
import {ErrorSource} from './ErrorSource';
import {useSession} from './context';
import {Dialog} from './Dialog';

export function UpdateDialog() {
  const session = useSession();
  const [result, setResult] = useState<UpdateResult | null>(null);
  const [place, setPlace] = useState<ErrorPlace>();
  const controller = useRef<AbortController>();
  const cancel = () => {
    controller.current?.abort();
    controller.current = undefined;
  };
  useEffect(() => {
    const run = async () => {
      if (controller.current) return;
      const request = new AbortController();
      controller.current = request;
      setPlace(undefined);
      setResult({text: 'Checking for updates…'});
      try {
        const answer = await checkUpdates(session.store.getState().hello?.about ?? [], request.signal);
        if (controller.current === request) setResult(answer);
      }
      catch (e) {
        if (controller.current === request) {
          setPlace(e instanceof UpdateError ? e.place : undefined);
          setResult({text: `Check for updates failed: ${e instanceof Error ? e.message : String(e)}`});
        }
      }
      finally { if (controller.current === request) controller.current = undefined; }
    };
    window.addEventListener('xpp-check-updates', run);
    return () => {
      window.removeEventListener('xpp-check-updates', run);
      cancel();
    };
  }, [session]);
  if (!result) return null;
  const close = () => { cancel(); setResult(null); setPlace(undefined); };
  return <Dialog title="Check for updates" name="update check" onClose={close} attrs={{'data-update-dialog': ''}}
    action={result.url ? {label: 'Open the release page', onClick: () => {
      void openRelease(result.url!).catch(e => setResult({text: `Opening the release page failed: ${String(e)}`}));
    }} : undefined}>
    <p>{result.text}</p>
    {place && <><p data-error-place>{placeWords(place)}</p><ErrorSource p={place}/></>}
  </Dialog>;
}
