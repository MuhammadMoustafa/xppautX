/* The work area's views as tabs above the plot (docs/ui-v2.md R6, T6, T26; W644): one tab per graph window and
   per autoX view, by its name, the one shown selected; ＋ adds a graph or an autoX view; a tab's × closes that view,
   an autoX one stopping a running continuation first (A10). The tabs follow the core: a pick selects that named
   window, so keys and menus act on the window shown. Keyboard: the WAI-ARIA tabs pattern, automatic activation
   (arrow keys, Home, End move and select; Tab leaves the tab list). A view is renamed on its tab (W685): a double
   click, the tab's context menu's Rename, or F2 on the focused tab edits its name in place; Enter applies it
   through window.rename, Escape or leaving the field cancels. */
import {useEffect, useLayoutEffect, useRef, useState} from 'preact/hooks';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {CommandButton, CommandLabel} from './CommandButton';
import {ContextMenu} from './ContextMenu';
import {ShownName} from './CutText';
import {useDropDown} from './SetsRow';
import {viewKeyText, type ViewKey} from '../popout';

/** the main graph's tab says why it stays (W696) */
const MAIN_PINNED = 'The main graph: always open';

/** the views shown as tabs: the graph windows (a hidden one has none until shown again; the active one always has), then autoX's */
interface ViewTab {kind: 'graph' | 'autox'; name: string; index: number}

export function useViewTabs(): {tabs: ViewTab[]; active: string} {
  const all = useStore(s => s.plots.windows);
  const plotsActive = useStore(s => s.plots.active);
  const hidden = useStore(s => s.windows.filter(w => w.kind === 'graph' && !w.shown).map(w => w.name).join('\n')).split('\n');
  const diagram = useStore(s => s.diagram);
  const graphs = all.filter(w => w.win === plotsActive || !hidden.includes(w.win)).map(w => w.win);
  /* before the first `plots` or `series`: the main graph, empty */
  const named = graphs.length ? graphs : ['main'];
  const tabs: ViewTab[] = named.map((name, index) => ({kind: 'graph', name, index}));
  if (diagram.open) diagram.views.forEach((v, index) => tabs.push({kind: 'autox', name: v.name, index}));
  return {tabs, active: diagram.open && diagram.shown ? diagram.views[diagram.active].name : plotsActive};
}

export function ViewTabs() {
  const session = useSession();
  const may = useMay();
  const {tabs, active} = useViewTabs();
  /* the main graph is the first graph window the core lists (its index 0): pinned first, it never closes (W696) */
  const mainName = useStore(s => s.windows.find(w => w.kind === 'graph')?.name);
  const autoxShown = useStore(s => s.diagram.open && s.diagram.shown);
  const list = useRef<HTMLDivElement>(null);
  const add = useDropDown();
  const popouts = useStore(s => s.popouts);
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
  /* the view being renamed (its name as the core has it), its context menu, and the tab the focus returns to after
     a rename that Enter or Escape ended (the tab's place in the strip) */
  const [renaming, setRenaming] = useState<string | null>(null);
  const [menu, setMenu] = useState<{name: string; x: number; y: number} | null>(null);
  const refocus = useRef<number | null>(null);
  useEffect(() => {
    if (renaming !== null || refocus.current === null) return;
    list.current?.querySelectorAll<HTMLElement>('[role=tab]')[refocus.current]?.focus();
    refocus.current = null;
  }, [renaming]);
  const startRename = (name: string) => { if (may({cmd: 'window.rename'})) setRenaming(name); };
  const endRename = (restore: boolean) => {
    if (restore) refocus.current = tabs.findIndex(t => t.name === renaming);
    setRenaming(null);
  };

  const viewOf = (tab: ViewTab): ViewKey => ({kind: tab.kind, name: tab.name});
  /* a tab whose view is in its own window brings that window forward as well */
  const pick = (tab: ViewTab) => {
    if (tab.kind === 'autox') session.selectAutoxView(tab.index);
    else session.selectWindow(tab.name);
    if (popouts.includes(viewKeyText(viewOf(tab)))) void session.popouts.show(viewOf(tab));
  };
  const onKeyDown = (e: KeyboardEvent) => {
    const i = tabs.findIndex(t => t.name === active);
    if (e.key === 'F2') {
      if (i < 0) return;
      e.preventDefault();
      e.stopPropagation(); /* not an XPP hotkey */
      startRename(tabs[i].name);
      return;
    }
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
  /* a tab's place in the strip keys its element, not its name: a rename keeps the element and so the focus on it (W685) */
  const slot = (tab: ViewTab) => `${tab.kind}-${tab.index}`;
  return (
    <div class="view-tabs">
      <div class="plot-tabs" role="tablist" aria-label="Views" ref={list} onKeyDown={onKeyDown}>
        {tabs.map(tab => {
          if (tab.name === renaming) return <ViewRename key={slot(tab)} name={tab.name} done={endRename} />;
          /* leaving the autoX view always works (it cancels the grab or pick waiting on it, session.selectWindow) */
          const off = tabOff && tab.name !== active && !autoxShown;
          /* the main graph has no × (the core refuses to close it), and its tab says why it stays (W696) */
          const pinned = tab.kind === 'graph' && tab.name === mainName;
          const out = popouts.includes(viewKeyText(viewOf(tab)));
          return (
            <span key={slot(tab)} class={'view-tab' + (tab.name === active ? ' active' : '') + (out ? ' out' : '')} data-view-kind={tab.kind}>
              <button id={id(tab)} class="plot-tab" role="tab" aria-selected={tab.name === active} tabIndex={tab.name === active ? 0 : -1}
                disabled={off} title={off ? BUSY_TITLE : pinned ? MAIN_PINNED : undefined} aria-description={pinned ? MAIN_PINNED : undefined} onClick={() => pick(tab)} onDblClick={() => startRename(tab.name)}
                onContextMenu={e => { e.preventDefault(); setMenu({name: tab.name, x: e.clientX, y: e.clientY}); }}>
                {tab.kind === 'autox' ? <span class="view-tab-kind">autoX · </span> : null}
                <span class="plot-tab-title"><ShownName name={tab.name} copy={false} /></span>
                {out && <span class="view-tab-out" title="In a window of its own">
                  <span aria-hidden="true">⧉</span><span class="visually-hidden"> (in a window of its own)</span></span>}
              </button>
              {!pinned && <button class="view-tab-close" aria-label={`Close ${tab.kind === 'autox' ? 'autoX view' : 'graph'} ${tab.name}`}
                title={tab.kind === 'autox' ? 'Done with this view: a running continuation is stopped first' : 'Close this graph'}
                disabled={tab.kind === 'graph' && windowOff}
                onClick={() => tab.kind === 'autox' ? session.closeAutox(tab.index) : session.closeWindow(tab.name)}>×</button>}
            </span>
          );
        })}
      </div>
      {menu && (() => {
        const tab = tabs.find(t => t.name === menu.name);
        const view = tab && viewOf(tab);
        const out = !!view && popouts.includes(viewKeyText(view));
        return <ContextMenu x={menu.x} y={menu.y} label={`View ${menu.name}`} close={() => setMenu(null)}
          items={[{label: 'Rename', off: !may({cmd: 'window.rename'}), run: () => startRename(menu.name)},
            {label: out ? 'Pop in' : 'Pop out', off: !view, run: () => view && (out ? session.popouts.popIn(view) : void session.popouts.show(view))}]} />;
      })()}
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

/** a view's name edited in its tab (W685): Enter applies it through window.rename, the name the core has now when it
    changed; Escape cancels, and so does leaving the field. `done(restore)` ends the edit: restore true puts the focus
    back on the tab (Enter, Escape), false leaves it where it went (the click or the Tab key that left the field) */
function ViewRename({name, done}: {name: string; done(restore: boolean): void}) {
  const session = useSession();
  const input = useRef<HTMLInputElement>(null);
  const ended = useRef(false);
  useLayoutEffect(() => { input.current?.focus(); input.current?.select(); }, []);
  const end = (apply: boolean, restore: boolean) => {
    if (ended.current) return; /* Enter's own end, then the field's blur as it goes */
    ended.current = true;
    const next = input.current?.value ?? name;
    if (apply && next !== name) session.renameWindow(name, next);
    done(restore);
  };
  return (
    <form class="view-tab-rename" onSubmit={e => { e.preventDefault(); end(true, true); }}>
      <input ref={input} aria-label={`Name of view ${name}`} defaultValue={name} onBlur={() => end(false, false)}
        onKeyDown={e => {
          e.stopPropagation(); /* not the tabs' keys, nor an XPP hotkey */
          if (e.key === 'Escape') { e.preventDefault(); end(false, true); }
        }} />
    </form>
  );
}
