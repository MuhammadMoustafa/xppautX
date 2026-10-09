/* The start screen (W232): what the program shows when it started with no
   model file and the Open model dialog was cancelled. Open model is the
   same operation as File > Open model (session.openModel), the recent
   models are hello.start (core/xpp_recent.h); a recent model whose file is
   gone is listed as missing, never dropped. */
import {placeWords} from '../protocol/errors';
import {baseName} from '../store/files';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {ErrorSource} from './ErrorSource';

export function StartScreen() {
  const session = useSession();
  const start = useStore(s => s.hello?.start);
  const may = useMay()({cmd: 'open'});
  if (!start) return null;
  const err = start.error ? {file: start.file ?? '', line: start.line ?? 0, col: start.col ?? 0, source: start.source ?? ''} : null;
  return (
    <section class="start-screen" aria-labelledby="start-title" data-start-screen>
      <h2 id="start-title">Open a model</h2>
      <p class="muted">A model is an .ode or .odex file, a saved session (.snapx) or a recording (.recx).</p>
      <button class="primary" data-start-open aria-disabled={!may} title={may ? undefined : BUSY_TITLE}
        onClick={() => { if (may) session.openModel(); }}>Open model…</button>
      <h3>Recent models</h3>
      {err && start.error && (
        <div class="banner error" role="alert" data-start-error>
          <p class="load-error-title">The recent models cannot be read: {placeWords(err)}</p>
          <ErrorSource p={err} />
          <pre class="load-error-cause">{start.error}</pre>
        </div>
      )}
      {start.recent.length === 0 && !err && <p class="muted" data-start-none>No model has been opened yet.</p>}
      <ul class="start-recent">
        {start.recent.map(m => (
          <li key={m.path}>
            <button data-start-recent={m.path} aria-disabled={m.missing || !may}
              title={m.missing ? `${m.path} is no longer there` : m.path}
              onClick={() => { if (!m.missing && may) session.openModel(m.path); }}>
              {baseName(m.path)}{m.missing && <span class="start-missing"> (missing)</span>}
            </button>
            <span class="muted start-path">{m.path}</span>
          </li>
        ))}
      </ul>
    </section>
  );
}
