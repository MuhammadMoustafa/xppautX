/* The recording bar (W59a, docs/mockups/record-play.html): while the core
   records the session's steps (File/recorD, the title bar's Record), a red
   dot, the steps taken so far, Stop (the core asks the file's name) and the
   note for the next step, a wrapping box sent to the core when it is left
   (docs/protocol.md "Recordings": record note). */
import {useEffect, useRef, useState} from 'preact/hooks';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';

export function RecordBar() {
  const session = useSession();
  const rec = useStore(s => s.core?.recording);
  const may = useMay();
  const [draft, setDraft] = useState('');
  const [attached, setAttached] = useState('');
  /* the note last sent, until a step takes it */
  const sent = useRef('');
  const steps = rec?.steps ?? 0;
  useEffect(() => {
    /* the core's note went with the step just taken: the box empties */
    if (rec && rec.note === '' && sent.current !== '') {
      setAttached(`Note attached to step ${steps}.`);
      if (draft.trim() === sent.current) setDraft('');
      sent.current = '';
    }
  }, [steps, rec?.note]);
  if (!rec) return null;
  const mayStop = may({cmd: 'record', op: 'stop'});
  /* the box's own value: a blur right behind an input comes before the render with it */
  const send = (value: string) => {
    const text = value.trim();
    if (text === rec.note) return;
    sent.current = text;
    setAttached('');
    session.recordNote(text);
  };
  return (
    <section class="recbar" aria-label="Recording">
      <span class="rec-dot" aria-hidden="true" />
      <b>Recording</b>
      <span class="rec-count" aria-live="polite">{steps} step{steps === 1 ? '' : 's'}</span>
      <button class="danger rec-stop" aria-disabled={!mayStop} title={mayStop ? 'Stop and save the recording (a .recx file)' : BUSY_TITLE}
        onClick={() => { if (mayStop) session.stopRecording(); }}>&#9632; Stop</button>
      <label class="rec-note">
        <span class="muted">Note for the next step</span>
        <textarea rows={2} value={draft} placeholder="e.g. Now switch the current on"
          onInput={e => setDraft((e.target as HTMLTextAreaElement).value)} onBlur={e => send((e.target as HTMLTextAreaElement).value)} />
      </label>
      <span class="rec-hint muted" aria-live="polite">
        {rec.note ? 'This note goes with the next step you take.' : attached || 'Idle time between steps is not recorded.'}
      </span>
    </section>
  );
}
