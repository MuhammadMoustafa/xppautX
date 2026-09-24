/* Entry point: the session over HTTP, the store's theme from last time, the
   UI, the test hook, the desktop window's menu hooks. */
import {render} from 'preact';
import 'uplot/dist/uPlot.min.css';
import './theme.css';
import {HttpFiles} from './protocol/files';
import {HttpTransport} from './protocol/transport';
import {installDesktopHooks} from './desktop';
import {Session} from './session';
import {installTestHook} from './testhook';
import {App} from './ui/App';
import {savedTheme} from './ui/theme';

const session = new Session(new HttpTransport(), new HttpFiles());
session.store.dispatch({type: 'theme', theme: savedTheme()});
installTestHook(session);
installDesktopHooks(session);
render(<App session={session} />, document.getElementById('app')!);
session.start();
