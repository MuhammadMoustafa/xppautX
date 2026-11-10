/* The page of a view in a window of its own (popout.ts, W645): the same page the main window is, one more client of the
   same session, showing only the view its address names (`#popout=graph:name`), with Pop in. Commands sent from here
   run as from the main window. The hotkeys, asks, errors and notifications are the main window's too: a question the core
   asks is shown wherever the user is. */
import {useEffect} from 'preact/hooks';
import type {Session} from '../session';
import {DOCK_TABS} from '../store/dock';
import {DockBody} from './Dock';
import {SessionContext, useSession, useStore} from './context';
import {viewLabel, type ViewKey} from '../popout';
import {AutoxPopOut} from './AutoxView';
import {GraphView} from './Plots';
import {AskDialog} from './AskDialog';
import {ErrorDialog} from './ErrorDialog';
import {Banner} from './App';
import {Toasts} from './Toasts';
import {useHotkeys} from './hotkeys';
import {useDark} from './theme';

/** whether the core has no such view any more (once it has said what it has) */
function useGone(view: ViewKey): boolean {
  return useStore(s => {
    if (!s.hello || !s.windows.length || view.kind === 'dock') return false;
    const kind = view.kind === 'graph' ? 'graph' : 'diagram';
    return !s.windows.some(w => w.kind === kind && w.name === view.name);
  });
}

function PopOutShell({view}: {view: ViewKey}) {
  const session = useSession();
  const dark = useDark(useStore(s => s.theme));
  const ready = useStore(s => !!s.hello);
  const gone = useGone(view);
  const label = viewLabel(view);
  useEffect(() => {
    document.title = `${label} — xppautX`;
  }, [label]);
  /* a dock tab's panel opens as its tab opens it in the main window (the table, the model's text, the manual) */
  useEffect(() => {
    const tab = DOCK_TABS.find(t => t.id === view.name);
    if (ready && view.kind === 'dock' && tab) session.selectDockTab(tab.id);
  }, [ready]);
  /* a view the core closed has nothing to show: back to where it was */
  useEffect(() => {
    if (gone) session.popouts.close(`${view.kind}:${view.name}`);
  }, [gone]);
  return (
    <div class="popout-shell" data-popout-kind={view.kind} data-popout-name={view.name}>
      <header class="popout-head">
        <h1>{label}</h1>
        <span class="spacer" />
        <button class="popout-in" title="Close this window and bring the view back into its tab"
          onClick={() => session.popouts.close(`${view.kind}:${view.name}`)}>Pop in</button>
      </header>
      <Banner />
      <main class="popout-body">
        {view.kind === 'graph' && <div class="plots"><GraphView win={view.name} dark={dark} shown tabbed={false} windowRow={null} /></div>}
        {view.kind === 'autox' && <AutoxPopOut name={view.name} dark={dark} />}
        {view.kind === 'dock' && <aside id="dock" class="dock open popout-dock" data-tab={view.name} aria-label={label}><DockBody /></aside>}
      </main>
      <Toasts />
      <AskDialog />
      <ErrorDialog />
    </div>
  );
}

export function PopOutApp({session, view}: {session: Session; view: ViewKey}) {
  useHotkeys(session);
  return (
    <SessionContext.Provider value={session}>
      <PopOutShell view={view} />
    </SessionContext.Provider>
  );
}
