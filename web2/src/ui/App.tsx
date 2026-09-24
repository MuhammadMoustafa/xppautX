/* The layout shell: title bar, command menu (a drawer on narrow screens),
   the plot windows, messages, status, notifications and the prompt dialog.
   Panels still to come (docs/ui-v2.md) get their own components here. */
import type {Session} from '../session';
import {AplotView} from './AplotView';
import {AniView} from './AniView';
import {AskDialog} from './AskDialog';
import {AutoView} from './AutoView';
import {SessionContext, useStore} from './context';
import {HelpView} from './Help';
import {useHotkeys} from './hotkeys';
import {MenuPanel} from './MenuPanel';
import {Messages} from './Messages';
import {Plots} from './Plots';
import {StatusBar} from './StatusBar';
import {TableView} from './TableView';
import {TextViews} from './TextViews';
import {useDark} from './theme';
import {TitleBar} from './TitleBar';
import {Toasts} from './Toasts';
import {SliderStrip} from './SliderStrip';
import {ValuesPanel} from './ValuesPanel';

/* what is wrong with the connection, in words, or nothing */
function Banner() {
  const connected = useStore(s => s.connected);
  const exited = useStore(s => s.exited);
  const hello = useStore(s => s.hello);
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
        <Plots dark={dark} />
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
