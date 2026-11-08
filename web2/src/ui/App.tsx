/* The layout shell: title bar, command menu (a drawer on narrow screens),
   the plot windows, messages, status, notifications and the prompt dialog.
   Panels still to come (docs/ui-v2.md) get their own components here. */
import {placeWords} from '../protocol/errors';
import type {LoadErrorEvent} from '../protocol/types';
import type {Session} from '../session';
import {AplotView} from './AplotView';
import {AniView} from './AniView';
import {AskDialog} from './AskDialog';
import {AutoxShow, AutoxView} from './AutoxView';
import {useEffect} from 'preact/hooks';
import {PANEL_LIMITS, savePanels, type PanelSize, type Panels} from '../store/panels';
import {SessionContext, useSession, useStore} from './context';
import {Splitter} from './Splitter';
import {UpdateDialog} from './UpdateDialog';
import {HelpView} from './Help';
import {useHotkeys} from './hotkeys';
import {MenuBar, MenuButton} from './MenuBar';
import {CommandPalette} from './CommandPalette';
import {CommandForm} from './CommandForm';
import {ValuesFileDialog} from './ValuesFileDialog';
import {OptionsPanel} from './OptionsPanel';
import {AnnotationsPanel} from './AnnotationsPanel';
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

/** `px`, held to the window's share for `size` (the stylesheet's limit, so a narrowed window keeps room for the plot) */
const track = (size: PanelSize, px: number) => `min(${px}px, ${PANEL_LIMITS[size].share * 100}vw)`;

/** the grid tracks the user's collapsing and dragging override (css/layout.css gives the defaults) */
function panelStyle(p: Panels, collapsed: boolean): Record<string, string> {
  const style: Record<string, string> = {};
  if (collapsed) {
    style['--values-col'] = '0px';
  } else if (p.valuesWidth !== null) style['--values-col'] = track('valuesWidth', p.valuesWidth);
  return style;
}

function Shell() {
  const theme = useStore(s => s.theme);
  const modelFile = useStore(s => s.hello?.file);
  const playerOpen = useStore(s => s.player.open);
  const dark = useDark(theme);
  const panels = useStore(s => s.panels);
  const tableOpen = useStore(s => s.table.open);
  useEffect(() => savePanels(panels), [panels]);
  /* the table docks in the right column (W648), so the column is there even where the user collapsed it */
  const collapsed = panels.valuesCollapsed && !tableOpen;
  return (
    <div class="shell" style={panelStyle(panels, collapsed)}
      data-values={collapsed ? 'collapsed' : 'open'} data-dock={tableOpen ? 'table' : undefined}>
      <a class="skip-link" href="#main">Skip to the plot</a>
      <TitleBar />
      <MenuBar />
      <CommandPalette />
      <CommandForm />
      <OptionsPanel />
      <ValuesFileDialog />
      <AnnotationsPanel />

      <main id="main" class="workspace">
        <Banner />
        {playerOpen ? null : <RunToolbar key={modelFile} />}
        <RecordBar />
        <AutoxShow />
        <PlayerStage dark={dark} />
      </main>
      <ValuesPanel />
      <Splitter controls="values-panel" size="valuesWidth" className="splitter-values" growth={-1} />
      <TableView />
      <TextViews />
      <AutoxView dark={dark} />
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
        <MenuButton />
        <h1>xppautX</h1>
        <span class="spacer" />
        <button class="help-toggle" aria-controls="help-panel" onClick={() => session.store.dispatch({type: 'help', action: {type: 'open'}})}
          title="The manual">Help</button>
      </header>
      <MenuBar /><CommandPalette /><CommandForm />
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
