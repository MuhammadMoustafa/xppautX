/* The layout shell: title bar, command menu (a drawer on narrow screens),
   the plot windows, messages, status, notifications and the prompt dialog.
   Panels still to come (docs/ui-v2.md) get their own components here. */
import {placeWords} from '../protocol/errors';
import type {LoadErrorEvent} from '../protocol/types';
import type {Session} from '../session';
import {AplotView} from './AplotView';
import {AniView} from './AniView';
import {AskDialog} from './AskDialog';
import {AutoShow, AutoView} from './AutoView';
import {SessionContext, useStore} from './context';
import {UpdateDialog} from './UpdateDialog';
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
import {KeymapEditor} from './KeymapEditor';
import {ReplaceDialog} from './FileDialog';
import {ErrorSource} from './ErrorSource';
import {Toasts} from './Toasts';
import {SliderStrip} from './SliderStrip';
import {ValuesPanel} from './ValuesPanel';
import {RunToolbar} from './RunToolbar';

/* a model that did not load (W63c): where, the line as written with a
   caret under the column when there is one, and what is wrong */
function LoadError({e}: {e: LoadErrorEvent}) {
  return (
    <div class="banner error load-error" role="alert">
      <p class="load-error-title">The model does not load: {placeWords(e)}</p>
      <ErrorSource p={e} />
      <pre class="load-error-cause">{e.error}</pre>
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
  const modelFile = useStore(s => s.hello?.file);
  const playerOpen = useStore(s => s.player.open);
  const dark = useDark(theme);
  return (
    <div class="shell">
      <a class="skip-link" href="#main">Skip to the plot</a>
      <TitleBar />
      <MenuPanel />
      <main id="main" class="workspace">
        <Banner />
        {playerOpen ? null : <RunToolbar key={modelFile} />}
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
      <UpdateDialog />
      <StatusBar />
      <Toasts />
      <AskDialog />
      <ErrorDialog />
      <KeymapEditor />
      <ReplaceDialog />
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
