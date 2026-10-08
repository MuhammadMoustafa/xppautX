/* Entry point: the session over HTTP, the store's theme from last time, the
   UI, the test hook, the desktop window's menu hooks. */
import {render} from 'preact';
import 'uplot/dist/uPlot.min.css';
import './theme.css';
import {HttpFiles} from './protocol/files';
import {HttpTransport} from './protocol/transport';
import {installBrowserLeave, installDesktopHooks} from './desktop';
import {Session} from './session';
import {installTestHook} from './testhook';
import {App} from './ui/App';
import {savedPanels} from './store/panels';
import {savedTheme} from './ui/theme';

const session = new Session(new HttpTransport(), new HttpFiles());
session.store.dispatch({type: 'theme', theme: savedTheme()});
session.store.dispatch({type: 'panels', panels: savedPanels()});
installTestHook(session);
installDesktopHooks(session);
installBrowserLeave();
render(<App session={session} />, document.getElementById('app')!);
session.start();
