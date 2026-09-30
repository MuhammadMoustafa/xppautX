/* The layout shell: title bar, command menu (a drawer on narrow screens),
   the plot windows, messages, status, notifications and the prompt dialog.
   Panels still to come (docs/ui-v2.md) get their own components here. */
import type {LoadErrorEvent} from '../protocol/types';
import type {Session} from '../session';
import {AplotView} from './AplotView';
import {AniView} from './AniView';
import {AskDialog} from './AskDialog';
import {AutoShow, AutoView} from './AutoView';
import {SessionContext, useStore} from './context';
import {HelpView} from './Help';
import {useHotkeys} from './hotkeys';
import {MenuPanel} from './MenuPanel';
import {Messages} from './Messages';
import {PlayerStage} from './Player';
import {RecordBar} from './RecordBar';
import {StatusBar} from './StatusBar';
import {TableView} from './TableView';
import {TextViews} from './TextViews';
import {useDark} from './theme';
import {TitleBar} from './TitleBar';
import {ErrorDialog} from './ErrorDialog';
import {Toasts} from './Toasts';
import {SliderStrip} from './SliderStrip';
import {ValuesPanel} from './ValuesPanel';

/* where a load error is, in words: the file, its line and column when known */
function loadErrorPlace(e: LoadErrorEvent): string {
  if (e.line <= 0) return e.file;
  return `${e.file}, line ${e.line}` + (e.col > 0 ? `, column ${e.col}` : '');
}

/* a model that did not load (W63c): where, the line as written with a
   caret under the column when there is one, and what is wrong */
function LoadError({e}: {e: LoadErrorEvent}) {
  return (
    <div class="banner error load-error" role="alert">
      <p class="load-error-title">The model does not load: {loadErrorPlace(e)}</p>
      {e.line > 0 && (
        <div class="source-lines load-error-source" aria-label={`Line ${e.line} of ${e.file}`}>
          <div class="source-row">
            <span class="source-lineno" aria-hidden="true">{e.line}</span>
            <span class="source-text">{e.source || ' '}</span>
          </div>
          {e.col > 0 && (
            <div class="source-row" aria-hidden="true">
              <span class="source-lineno" />
              <span class="source-text load-error-caret">{' '.repeat(e.col - 1) + '^'}</span>
            </div>
          )}
        </div>
      )}
      <pre class="load-error-cause">{e.cause}</pre>
      <p>Correct the model and start XPP again; Messages below show everything it printed.</p>
    </div>
  );
}

/* what is wrong with the connection, in words, or nothing */
function Banner() {
  const connected = useStore(s => s.connected);
  const exited = useStore(s => s.exited);
  const hello = useStore(s => s.hello);
  const loadError = useStore(s => s.loadError);
  if (loadError) return <LoadError e={loadError} />;
  if (exited !== null) {
    return (
      <div class="banner error" role="alert">
        XPP has stopped{exited ? ' with an error' : ''}. Messages below show what it printed; start it again to
        continue.
      </div>
    );
  }
  if (!connected && hello) return <div class="banner" role="status">Connection lost. Reconnecting…</div>;
  return null;
}

function Shell() {
  const theme = useStore(s => s.theme);
  const dark = useDark(theme);
  return (
    <div class="shell">
      <a class="skip-link" href="#main">Skip to the plot</a>
      <TitleBar />
      <MenuPanel />
      <main id="main" class="workspace">
        <Banner />
        <RecordBar />
        <AutoShow />
        <PlayerStage dark={dark} />
        <SliderStrip />
        <Messages />
      </main>
      <ValuesPanel />
      <TableView />
      <TextViews />
      <AutoView dark={dark} />
      <AplotView />
      <AniView />
      <HelpView />
      <StatusBar />
      <Toasts />
      <AskDialog />
      <ErrorDialog />
    </div>
  );
}

export function App({session}: {session: Session}) {
  useHotkeys(session);
  return (
    <SessionContext.Provider value={session}>
      <Shell />
    </SessionContext.Provider>
  );
}
