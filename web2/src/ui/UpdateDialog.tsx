import {useEffect, useRef, useState} from 'preact/hooks';
import {offerText, openRelease, UpdateError, type UpdateResult} from '../help/updates';
import {placeWords, type ErrorPlace} from '../protocol/errors';
import {ErrorSource} from './ErrorSource';
import {useSession, useStore} from './context';
import {Dialog, type DialogAction} from './Dialog';

export function UpdateDialog() {
  const session = useSession();
  const [result, setResult] = useState<UpdateResult | null>(null);
  const [place, setPlace] = useState<ErrorPlace>();
  const offer = useStore(s => s.update);
  const install = useStore(s => s.hello?.update);
  const busy = useStore(s => s.busy);
  const progress = useStore(s => s.progress);
  const controller = useRef<AbortController>();
  const cancel = () => {
    controller.current?.abort();
    controller.current = undefined;
  };
  useEffect(() => {
    const run = async () => {
      if (controller.current) return;
      /* an update on its way (downloading, or ready) is shown as it is: no question to GitHub */
      const now = session.store.getState().update;
      if (now && now.stage !== 'available') { setPlace(undefined); setResult({text: offerText(now)}); return; }
      const request = new AbortController();
      controller.current = request;
      setPlace(undefined);
      setResult({text: 'Checking for updates…'});
      try {
        const answer = await session.checkForUpdate(request.signal);
        if (controller.current === request) { setResult(answer); session.offerUpdate(answer); }
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
  const openPage = () => {
    void openRelease(active?.url ?? result.url!).catch(e => setResult({text: `Opening the release page failed: ${String(e)}`}));
  };
  /* an update on its way (downloading, or ready) is what the dialog shows; else the answer of the check just made */
  const active = offer && offer.stage !== 'available' ? offer : null;
  const installable = !!install && install.install !== 'release_page' && !!result.asset;
  const action: DialogAction | undefined = active?.stage === 'restart' ? {label: 'Restart now', onClick: () => session.switchUpdate(), attrs: {'data-update-restart': ''}}
    : active?.stage === 'install' ? {label: 'Install and restart', onClick: () => session.switchUpdate(), attrs: {'data-update-restart': ''}}
    : active ? undefined
    : !result.url ? undefined
    : installable ? {label: 'Update', onClick: () => session.downloadUpdate(), attrs: {'data-update-download': ''}}
    : {label: 'Open the release page', onClick: openPage};
  const waiting = active?.stage === 'restart' || active?.stage === 'install';
  const why = !active && result.url && !installable ? (install?.install === 'release_page' && install.why ? install.why : 'the release list has no file for this installation') : '';
  return <Dialog title="Check for updates" name="update check" onClose={close} attrs={{'data-update-dialog': ''}} action={action}
    extra={<>
      {waiting && <button type="button" data-update-later onClick={close}>Later</button>}
      {active?.stage === 'downloading' && <button type="button" class="danger" onClick={() => session.abort()}>Stop</button>}
      {!active && installable && <button type="button" onClick={openPage}>Open the release page</button>}
    </>}>
    <p>{active ? offerText(active) : result.text}</p>
    {why && <p data-update-why>The release page is offered because {why}.</p>}
    {waiting && busy && <p data-update-busy>A computation is running: stop it first, then restart.</p>}
    {active?.stage === 'install' && <p>The system's installer asks for permission itself; xppautX quits so it can replace the program.</p>}
    {active?.stage === 'downloading' && <progress class="dialog-progress" max={progress?.of ?? 1} value={progress?.n ?? 0} aria-label="Download progress" data-update-progress />}
    {place && <><p data-error-place>{placeWords(place)}</p><ErrorSource p={place}/></>}
  </Dialog>;
}
