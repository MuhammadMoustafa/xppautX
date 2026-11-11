/* The right dock (docs/ui-v2.md R6, T26; W644): Values, Table, Model and Help are its tabs, below the header and the menu bar,
   one width (the splitter's), one × that hides it. From 48rem it is a column beside the plot; under that a full-screen sheet with
   the same tabs and the same ×. The panels are the tabs' contents, each its own component; which one shows is store/dock.ts's. */
import type {ComponentChildren} from 'preact';
import {DOCK_SHEET_WIDTH, DOCK_TABS, dockTab, type DockTab} from '../store/dock';
import {useSession, useStore} from './context';
import {viewKeyText, type ViewKey} from '../popout';
import {HelpView} from './Help';
import {TableView} from './TableView';
import {TextViews} from './TextViews';
import {ValuesPanel} from './ValuesPanel';

/** the id of each tab's panel */
const PANEL: Record<DockTab, string> = {values: 'values-panel', table: 'table-panel', text: 'text-panel', help: 'help-panel'};

/** a tab and the × of the dock's head, one place for the dock and the start screen's Help */
function DockHead({tabs, popOut, onClose}: {tabs: ComponentChildren; popOut?: ComponentChildren; onClose: () => void}) {
  return (
    <div class="dock-head">
      <div class="dock-tabs" role="tablist" aria-label="Dock">{tabs}</div>
      {popOut}
      <button class="dock-close" aria-label="Hide the dock" title="Hide the dock" onClick={onClose}>×</button>
    </div>
  );
}

/** the start screen (no model) has no panels but Help: the dock with that one tab */
export function HelpDock() {
  const session = useSession();
  const open = useStore(s => s.help.open);
  return (
    <aside id="dock" class={'dock help-dock' + (open ? ' open' : '')} data-tab="help" aria-label="Dock">
      <DockHead onClose={() => session.store.dispatch({type: 'help', action: {type: 'close'}})}
        tabs={<button id="dock-tab-help" class="dock-tab" role="tab" aria-selected="true" aria-controls="help-panel" data-dock-tab="help">Help</button>} />
      <div class="dock-body"><HelpView /></div>
    </aside>
  );
}

/** the dock's four panels, one shown by the aside's data-tab (css/panels.css): the dock's body, and a popped-out tab's */
export function DockBody() {
  return (
    <div class="dock-body">
      <ValuesPanel />
      <TableView />
      <TextViews />
      <HelpView />
    </div>
  );
}

const dockView = (tab: DockTab): ViewKey => ({kind: 'dock', name: tab});

export function Dock() {
  const session = useSession();
  const tab = useStore(dockTab);
  const popouts = useStore(s => s.popouts);
  const isOut = (t: DockTab) => popouts.includes(viewKeyText(dockView(t)));
  const shownOut = isOut(tab);
  const label = DOCK_TABS.find(t => t.id === tab)?.label;
  const sheet = useStore(s => s.dockOpen || dockTab(s) !== 'values');
  return (
    <aside id="dock" class={'dock' + (sheet ? ' open' : '')} data-tab={tab} aria-label="Dock">
      <DockHead onClose={() => session.hideDock(window.matchMedia(DOCK_SHEET_WIDTH).matches)} tabs={DOCK_TABS.map(t => (
        <button key={t.id} id={`dock-tab-${t.id}`} class={'dock-tab' + (isOut(t.id) ? ' out' : '')} role="tab" aria-selected={tab === t.id}
          aria-controls={PANEL[t.id]} data-dock-tab={t.id} onClick={() => session.selectDockTab(t.id)}>
          {t.label}
          {isOut(t.id) && <span class="view-tab-out" title="In a window of its own">
            <span aria-hidden="true">⧉</span><span class="visually-hidden"> (in a window of its own)</span></span>}
        </button>
      ))} popOut={<button class="dock-popout" aria-label={`${shownOut ? 'Pop in' : 'Pop out'} ${label}`}
        title={shownOut ? 'Bring this tab back into the dock' : 'Open this tab in a window of its own'}
        onClick={() => shownOut ? session.popouts.popIn(dockView(tab)) : session.popOutDockTab(tab)}>{shownOut ? '⇲' : '⇱'}</button>} />
      <DockBody />
    </aside>
  );
}
