/* The work area's views as tabs above the plot (docs/ui-v2.md R6, T6, T26; W644): one tab per graph window and
   per autoX view, by its name, the one shown selected; ＋ adds a graph or an autoX view; a tab's × closes that view,
   an autoX one stopping a running continuation first (A10). The tabs follow the core: a pick selects that named
   window, so keys and menus act on the window shown. Keyboard: the WAI-ARIA tabs pattern, automatic activation
   (arrow keys, Home, End move and select; Tab leaves the tab list). */
import {useEffect, useRef} from 'preact/hooks';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {CommandButton, CommandLabel} from './CommandButton';
import {ShownName} from './CutText';
import {useDropDown} from './SetsRow';

/** the views shown as tabs: the graph windows (a hidden one has none until shown again; the active one always has), then autoX's */
interface ViewTab {kind: 'graph' | 'autox'; name: string; index: number}

export function useViewTabs(): {tabs: ViewTab[]; active: string} {
  const all = useStore(s => s.plots.windows);
  const plotsActive = useStore(s => s.plots.active);
  const hidden = useStore(s => s.windows.filter(w => w.kind === 'graph' && !w.shown).map(w => w.name).join('\n')).split('\n');
  const diagram = useStore(s => s.diagram);
  const graphs = all.filter(w => w.win === plotsActive || !hidden.includes(w.win)).map(w => w.win);
  /* before the first `plots` or `series`: window 1, empty */
  const named = graphs.length ? graphs : ['graph_1'];
  const tabs: ViewTab[] = named.map((name, index) => ({kind: 'graph', name, index}));
  if (diagram.open) diagram.views.forEach((v, index) => tabs.push({kind: 'autox', name: v.name, index}));
  return {tabs, active: diagram.open && diagram.shown ? diagram.views[diagram.active].name : plotsActive};
}

export function ViewTabs() {
  const session = useSession();
  const may = useMay();
  const {tabs, active} = useViewTabs();
  const autoxShown = useStore(s => s.diagram.open && s.diagram.shown);
  const list = useRef<HTMLDivElement>(null);
  const add = useDropDown();
  /* a tab pick is a view: it works during a run too, only an open question waits (W95) */
  const tabOff = !may({cmd: 'window.select', window: active});
  const windowOff = !may({cmd: 'window.new'});
  /* the tab a key picked: a pick while the core is busy is held for its done (session.selectWindow); the tab the key
     picked gets the focus once it is the active one, so the next key is the tabs' again, not an XPP hotkey (W93) */
  const keyPicked = useRef<string | null>(null);
  useEffect(() => {
    if (keyPicked.current !== active) return;
    keyPicked.current = null;
    list.current?.querySelectorAll<HTMLElement>('[role=tab]')[tabs.findIndex(t => t.name === active)]?.focus();
  }, [active]);

  const pick = (tab: ViewTab) => tab.kind === 'autox' ? session.selectAutoxView(tab.index) : session.selectWindow(tab.name);
  const onKeyDown = (e: KeyboardEvent) => {
    const i = tabs.findIndex(t => t.name === active);
    const n = tabs.length;
    const next = e.key === 'ArrowRight' || e.key === 'ArrowDown' ? (i + 1) % n
      : e.key === 'ArrowLeft' || e.key === 'ArrowUp' ? (i - 1 + n) % n
      : e.key === 'Home' ? 0 : e.key === 'End' ? n - 1 : -1;
    if (next < 0 || i < 0) return;
    e.preventDefault();
    e.stopPropagation(); /* not an XPP hotkey */
    keyPicked.current = tabs[next].name;
    pick(tabs[next]);
    list.current?.querySelectorAll<HTMLElement>('[role=tab]')[next]?.focus();
  };
  const id = (tab: ViewTab) => `view-tab-${tab.kind}-${encodeURIComponent(tab.name)}`;
  return (
    <div class="view-tabs">
      <div class="plot-tabs" role="tablist" aria-label="Views" ref={list} onKeyDown={onKeyDown}>
        {tabs.map(tab => {
          /* leaving the autoX view always works (it cancels the grab or pick waiting on it, session.selectWindow) */
          const off = tabOff && tab.name !== active && !autoxShown;
          /* the main graph stays: the core refuses to close window 1 */
          const closable = tab.kind === 'autox' || tab.index > 0;
          return (
            <span key={id(tab)} class={'view-tab' + (tab.name === active ? ' active' : '')} data-view-kind={tab.kind}>
              <button id={id(tab)} class="plot-tab" role="tab" aria-selected={tab.name === active} tabIndex={tab.name === active ? 0 : -1}
                disabled={off} title={off ? BUSY_TITLE : undefined} onClick={() => pick(tab)}>
                {tab.kind === 'autox' ? <span class="view-tab-kind">autoX · </span> : null}
                <span class="plot-tab-title"><ShownName name={tab.name} copy={false} /></span>
              </button>
              {closable && <button class="view-tab-close" aria-label={`Close ${tab.kind === 'autox' ? 'autoX view' : 'graph'} ${tab.name}`}
                title={tab.kind === 'autox' ? 'Done with this view: a running continuation is stopped first' : 'Close this graph'}
                disabled={tab.kind === 'graph' && windowOff}
                onClick={() => tab.kind === 'autox' ? session.closeAutox(tab.index) : session.closeWindow(tab.name)}>×</button>}
            </span>
          );
        })}
      </div>
      <details class="view-add" ref={add}>
        <summary aria-label="New view" title="A new graph or autoX view">＋</summary>
        <ul role="menu" onClick={() => {if (add.current) add.current.open = false;}}>
          <li><button class="small" data-command="window.new" disabled={windowOff} onClick={() => session.newWindow()}
            title="Create a graph copied from the active graph"><CommandLabel command="window.new" /></button></li>
          <li><CommandButton command="autox.new_view" /></li>
        </ul>
      </details>
    </div>
  );
}
