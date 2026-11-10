/* Entry point: the session over HTTP, the store's theme from last time, the
   UI, the test hook, the desktop window's menu hooks. */
import {render} from 'preact';
import 'uplot/dist/uPlot.min.css';
import './css/tokens.css';
import './css/layout.css';
import './css/controls.css';
import './css/values.css';
import './css/panels.css';
import './css/plot.css';
import './css/player.css';
import './css/dialogs.css';
import './css/autox.css';
import {HttpFiles} from './protocol/files';
import {HttpTransport} from './protocol/transport';
import {installBrowserLeave, installDesktopHooks} from './desktop';
import {Session} from './session';
import {installTestHook} from './testhook';
import {App} from './ui/App';
import {PopOutApp} from './ui/PopOut';
import {popOutRequest} from './popout';
import {savedPanels} from './store/panels';
import {savedTheme} from './ui/theme';

const session = new Session(new HttpTransport(), new HttpFiles());
session.store.dispatch({type: 'theme', theme: savedTheme()});
session.store.dispatch({type: 'panels', panels: savedPanels()});
installTestHook(session);
installDesktopHooks(session);
/* a view in a window of its own (popout.ts, W645): the same page, at the address with the view named */
const popOut = popOutRequest(location.hash);
if (typeof popOut === 'string') {
  document.getElementById('app')!.textContent = popOut;
} else if (popOut) {
  session.popouts.startPopOut(popOut);
  render(<PopOutApp session={session} view={popOut} />, document.getElementById('app')!);
  session.start();
} else {
  /* the pop-outs close with the main window; a pop-out never says it is leaving: the main window's page does */
  session.popouts.startMain();
  installBrowserLeave();
  render(<App session={session} />, document.getElementById('app')!);
  session.start();
}
