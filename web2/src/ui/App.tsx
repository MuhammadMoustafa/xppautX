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
import {useEffect} from 'preact/hooks';
import {PANEL_LIMITS, savePanels, type PanelSize, type Panels} from '../store/panels';
import {SessionContext, useSession, useStore} from './context';
import {Splitter, useMinWidth} from './Splitter';
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
import {StartScreen} from './StartScreen';

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

/** the stylesheet's wide layout (theme.css: the Values panel a right column) starts here */
const WIDE_REM = 80;

/** `px`, held to the window's share for `size` (the stylesheet's limit, so a narrowed window keeps room for the plot) */
const track = (size: PanelSize, px: number, unit: 'vw' | 'vh') => `min(${px}px, ${PANEL_LIMITS[size].share * 100}${unit})`;

/** the grid tracks the user's collapsing and dragging override (theme.css gives the defaults) */
function panelStyle(p: Panels): Record<string, string> {
  const style: Record<string, string> = {};
  if (p.menuCollapsed) style['--menu-width'] = '0px';
  else if (p.menuWidth !== null) style['--menu-width'] = track('menuWidth', p.menuWidth, 'vw');
  if (p.valuesCollapsed) {
    style['--values-col'] = '0px';
    style['--values-row'] = '0px';
  } else {
    if (p.valuesWidth !== null) style['--values-col'] = track('valuesWidth', p.valuesWidth, 'vw');
    if (p.valuesHeight !== null) style['--values-row'] = track('valuesHeight', p.valuesHeight, 'vh');
  }
  return style;
}

function Shell() {
  const theme = useStore(s => s.theme);
  const modelFile = useStore(s => s.hello?.file);
  const playerOpen = useStore(s => s.player.open);
  const dark = useDark(theme);
  const panels = useStore(s => s.panels);
  const wide = useMinWidth(WIDE_REM);
  useEffect(() => savePanels(panels), [panels]);
  return (
    <div class="shell" style={panelStyle(panels)} data-menu={panels.menuCollapsed ? 'collapsed' : 'open'}
      data-values={panels.valuesCollapsed ? 'collapsed' : 'open'}>
      <a class="skip-link" href="#main">Skip to the plot</a>
      <TitleBar />
      <MenuPanel />
      <Splitter controls="command-menu" size="menuWidth" className="splitter-menu" orientation="vertical" growth={1} />
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
      {wide
        ? <Splitter controls="values-panel" size="valuesWidth" className="splitter-values" orientation="vertical" growth={-1} />
        : <Splitter controls="values-panel" size="valuesHeight" className="splitter-values" orientation="horizontal" growth={-1} />}
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

/* the session has no model (the program started with no file, hello.start): the start screen
   instead of the model's panels, which have nothing to show; the manual, errors and prompts stay */
function StartShell() {
  const session = useSession();
  return (
    <div class="start-shell">
      <header class="title-bar">
        <h1>xppautX</h1>
        <span class="spacer" />
        <button class="help-toggle" aria-controls="help-panel" onClick={() => session.store.dispatch({type: 'help', action: {type: 'open'}})}
          title="The manual (F1)">Help</button>
      </header>
      <main id="main" class="workspace">
        <Banner />
        <StartScreen />
        <Messages />
      </main>
      <HelpView />
      <UpdateDialog />
      <Toasts />
      <AskDialog />
      <ErrorDialog />
      <KeymapEditor />
      <ReplaceDialog />
    </div>
  );
}

function Root() {
  return useStore(s => !!s.hello?.start) ? <StartShell /> : <Shell />;
}

export function App({session}: {session: Session}) {
  useHotkeys(session);
  return (
    <SessionContext.Provider value={session}>
      <Root />
    </SessionContext.Provider>
  );
}
