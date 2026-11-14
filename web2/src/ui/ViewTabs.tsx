/* The work area's views as tabs above the plot (docs/ui-v2.md R6, T6, T26; W644): one tab per graph window and
   per autoX view, by its name, the one shown selected; ＋ adds a graph or an autoX view; a tab's × closes that view,
   an autoX one stopping a running continuation first (A10). The tabs follow the core: a pick selects that named
   window, so keys and menus act on the window shown. Keyboard: the WAI-ARIA tabs pattern, automatic activation
   (arrow keys, Home, End move and select; Tab leaves the tab list). A view is renamed on its tab (W685): a double
   click, the tab's context menu's Rename, or F2 on the focused tab edits its name in place; Enter applies it
   through window.rename, Escape or leaving the field cancels. The tabs are reordered as VS Code's are (W719): a tab dragged onto
   another of its kind takes that tab's place (a marker shows the side it lands on), Ctrl+Shift+PageUp and PageDown (the keymap's
   window.move_earlier and window.move_later) and the tab menu's Move left and Move right (the way on a phone) move it one place. The
   order is the core's (window.move); the main graph stays first, and a graph and an autoX view do not trade places. In a split view (W720,
   ui/split.ts) each group has its strip: the focused one holds the selected view; a tab's menu has Split right, Split down and Move to
   first group, a tab dragged onto the other strip changes its group, and ◫ at a strip's end splits its shown view right. Narrow, or
   with an autoX view shown, the strips are one with every tab, in the core's order. */
import type {ComponentChildren} from 'preact';
import {useEffect, useLayoutEffect, useRef, useState} from 'preact/hooks';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {CommandButton, CommandLabel} from './CommandButton';
import {ContextMenu} from './ContextMenu';
import {ShownName} from './CutText';
import {useDropDown} from './SetsRow';
import {viewKeyText, type ViewKey} from '../popout';
import {shownInGroup, useNarrow, useSplitView} from './split';

/** the main graph's tab says why it stays (W696) */
const MAIN_TITLE = 'The main graph: always open';

/** the views shown as tabs: the graph windows (a hidden one has none until shown again; the active one always has), then autoX's */
interface ViewTab {kind: 'graph' | 'autox'; name: string; index: number; second: boolean}

/** `tabs` as the core orders them (a tab's `index` is its place among the views of its kind, hidden graphs counted) */
export function useViewTabs(): {tabs: ViewTab[]; active: string} {
  const all = useStore(s => s.plots.windows);
  const plotsActive = useStore(s => s.plots.active);
  const hidden = useStore(s => s.windows.filter(w => w.kind === 'graph' && !w.shown).map(w => w.name).join('\n')).split('\n');
  const diagram = useStore(s => s.diagram);
  const selected = useStore(s => s.split.selected);
  const drawn = useSplitView() !== null;
  const seconds = useStore(s => s.windows.filter(w => w.group === 'second').map(w => w.name).join('\n')).split('\n');
  const graphs = all.filter(w => w.win === plotsActive || !hidden.includes(w.win)).map(w => w.win);
  /* before the first `plots` or `series`: the main graph, empty */
  const named = graphs.length ? graphs : ['main'];
  const tabs: ViewTab[] = named.map(name => ({kind: 'graph', name, index: Math.max(0, all.findIndex(w => w.win === name)), second: seconds.includes(name)}));
  if (diagram.open) diagram.views.forEach((v, index) => tabs.push({kind: 'autox', name: v.name, index, second: seconds.includes(v.name)}));
  return {tabs, active: drawn ? selected : diagram.open && diagram.shown ? diagram.views[diagram.active].name : plotsActive};
}

/** the tab being dragged (the strips share it: a tab is dropped on another strip too, and on the plots, ui/Plots.tsx) */
let draggedTab: ViewTab | null = null;
export const draggedView = (): ViewTab | null => draggedTab;

/** the ＋ that adds a graph or an autoX view */
function AddView() {
  const session = useSession();
  const may = useMay();
  const add = useDropDown();
  const windowOff = !may({cmd: 'window.new'});
  return (
    <details class="view-add" ref={add}>
      <summary aria-label="New view" title="A new graph or autoX view">＋</summary>
      <ul role="menu" onClick={() => {if (add.current) add.current.open = false;}}>
        <li><button class="small" data-command="window.new" disabled={windowOff} onClick={() => session.newWindow()}
          title="Create a graph copied from the active graph"><CommandLabel command="window.new" /></button></li>
        <li><CommandButton command="autox.new_view" /></li>
      </ul>
    </details>
  );
}

/** the row of strips above the work area: one with every tab, or in a split view one per group (the second's here beside the first when
    the split goes right, over its plot when it goes down, ui/Plots.tsx) */
export function ViewTabs() {
  const split = useSplitView();
  return (
    <div class="view-tabs">
      {!split ? <GroupStrip group="all"><AddView /></GroupStrip>
        : split.direction === 'right' ? (
          <div class="split-strips">
            <GroupStrip group="first" />
            <div class="split-gap" aria-hidden="true" />
            <GroupStrip group="second"><AddView /></GroupStrip>
          </div>
        ) : <GroupStrip group="first"><AddView /></GroupStrip>}
    </div>
  );
}

type StripGroup = 'all' | 'first' | 'second';

/** one strip of tabs: every view (group `all`) or one group's; `children` come after ◫ */
export function GroupStrip({group, children}: {group: StripGroup; children?: ComponentChildren}) {
  const session = useSession();
  const may = useMay();
  const {tabs: every, active} = useViewTabs();
  const split = useStore(s => s.split);
  const narrow = useNarrow();
  const tabs = group === 'all' ? every : every.filter(t => t.second === (group === 'second'));
  /* the tab shown in this strip: the view shown, or its group's remembered one (the core's) */
  const selected = useStore(s => group === 'all' ? active : shownInGroup(s, group));
  const focused = tabs.some(t => t.name === active);
  /* the main graph is the first graph window the core lists (its index 0): pinned first, it never closes (W696) */
  const mainName = useStore(s => s.windows.find(w => w.kind === 'graph')?.name);
  const autoxShown = useStore(s => s.diagram.open && s.diagram.shown);
  const list = useRef<HTMLDivElement>(null);
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
  /* where a dragged tab's drop would land: on a tab, at the side the tab moves from, or on this strip, which changes its group */
  const [drop, setDrop] = useState<{name: string; side: 'before' | 'after'} | null>(null);
  const [dropGroup, setDropGroup] = useState(false);
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

  /* a tab is dragged onto another of its kind in its group and takes its place, the main graph's excepted: that is as far left as a
     graph goes; dragged onto a strip of the other group (a tab of it too) it changes group */
  const sameGroup = (from: ViewTab) => group === 'all' || from.second === (group === 'second');
  const landing = (over: ViewTab): {position: number; side: 'before' | 'after'} | null => {
    const from = draggedTab;
    if (!from || from.kind !== over.kind || from.name === over.name || !sameGroup(from)) return null;
    const first = over.kind === 'graph' ? 1 : 0;
    return {position: Math.max(first, over.index), side: over.index < from.index && over.index >= first ? 'before' : 'after'};
  };
  const endDrag = () => { draggedTab = null; setDrop(null); setDropGroup(false); };
  /* the dragged tab joins this strip's group */
  const toThisGroup = (from: ViewTab) => {
    if (group === 'second') session.splitWindow(from.name, split.direction === 'none' ? 'right' : split.direction);
    else session.joinWindow(from.name);
  };
  /* after a move the focus stays on the moved tab, which is the active one, so the keys go on moving it */
  const order = tabs.map(t => t.name).join('\n');
  useEffect(() => {
    const focusedElement = document.activeElement;
    if (focusedElement?.getAttribute('role') !== 'tab' || !list.current?.contains(focusedElement)) return;
    list.current.querySelectorAll<HTMLElement>('[role=tab]')[tabs.findIndex(t => t.name === active)]?.focus();
  }, [order]);

  const viewOf = (tab: ViewTab): ViewKey => ({kind: tab.kind, name: tab.name});
  /* a tab whose view is in its own window brings that window forward as well */
  const pick = (tab: ViewTab) => {
    if (tab.kind === 'autox') session.selectAutoxView(tab.index);
    else session.selectWindow(tab.name);
    if (popouts.includes(viewKeyText(viewOf(tab)))) void session.popouts.show(viewOf(tab));
  };
  const onKeyDown = (e: KeyboardEvent) => {
    const i = tabs.findIndex(t => t.name === selected);
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
  /* the tab of its kind beside this one in the strip, for Move left and Move right: a graph goes no further left than the place after
     the main graph, which stays first */
  const beside = (tab: ViewTab, step: -1 | 1): ViewTab | undefined => {
    if (tab.kind === 'graph' && tab.index < 1) return undefined; /* the main graph does not move */
    const kin = tabs.filter(t => t.kind === tab.kind);
    const next = kin[kin.indexOf(tab) + step];
    return next && !(tab.kind === 'graph' && next.index < 1) ? next : undefined;
  };
  const shownTab = tabs.find(t => t.name === selected);
  const splitOff = !shownTab || (shownTab.kind === 'graph' && shownTab.name === mainName) || !may({cmd: 'window.split'});
  const label = group === 'all' ? 'Views' : `Views, ${group} group`;
  return (
    <div class={'group-strip' + (group !== 'all' ? ' grouped' : '') + (group !== 'all' && focused ? ' focused' : '') + (dropGroup ? ' drop-group' : '')} data-group={group}
      onDragOver={e => {
        if (group === 'all' || !draggedTab || sameGroup(draggedTab) || !may({cmd: 'window.split'})) return;
        e.preventDefault();
        if (!dropGroup) setDropGroup(true);
      }}
      onDragLeave={e => { if (!e.currentTarget.contains(e.relatedTarget as Node | null)) setDropGroup(false); }}
      onDrop={e => {
        const from = draggedTab;
        if (group === 'all' || !from || sameGroup(from)) return;
        e.preventDefault();
        endDrag();
        toThisGroup(from);
      }}>
      <div class="plot-tabs" role="tablist" aria-label={label} ref={list} onKeyDown={onKeyDown}>
        {tabs.map(tab => {
          if (tab.name === renaming) return <ViewRename key={slot(tab)} name={tab.name} done={endRename} />;
          /* leaving the autoX view always works (it cancels the grab or pick waiting on it, session.selectWindow) */
          const off = tabOff && tab.name !== active && !autoxShown;
          /* the main graph has no × (the core refuses to close it), and its tab says why it stays (W696) */
          const isMain = tab.kind === 'graph' && tab.name === mainName;
          const out = popouts.includes(viewKeyText(viewOf(tab)));
          const marked = drop?.name === tab.name ? ` drop-${drop.side}` : '';
          return (
            <span key={slot(tab)} class={'view-tab' + (tab.name === selected ? ' active' : '') + (out ? ' out' : '') + marked} data-view-kind={tab.kind}
              draggable={!isMain && !off && may({cmd: 'window.move'})}
              onDragStart={e => { if (isMain) { e.preventDefault(); return; } draggedTab = tab; if (e.dataTransfer) { e.dataTransfer.effectAllowed = 'move'; e.dataTransfer.setData('text/plain', tab.name); } }}
              onDragOver={e => {
                const to = landing(tab);
                if (!to) return; /* no drop here: no marker, and the cursor says so (the strip takes a drop of another group) */
                e.preventDefault();
                e.stopPropagation();
                if (drop?.name !== tab.name || drop.side !== to.side) setDrop({name: tab.name, side: to.side});
              }}
              onDragLeave={() => { if (drop?.name === tab.name) setDrop(null); }}
              onDrop={e => {
                const to = landing(tab), from = draggedTab;
                if (!to || !from) return; /* the strip's own drop, for another group's tab */
                e.preventDefault();
                e.stopPropagation();
                endDrag();
                session.moveWindow(from.name, to.position);
              }}
              onDragEnd={endDrag}>
              <button id={id(tab)} class="plot-tab" role="tab" aria-selected={tab.name === selected} tabIndex={tab.name === selected ? 0 : -1}
                disabled={off} title={off ? BUSY_TITLE : isMain ? MAIN_TITLE : undefined} aria-description={isMain ? MAIN_TITLE : undefined} onClick={() => pick(tab)} onDblClick={() => startRename(tab.name)}
                onContextMenu={e => { e.preventDefault(); setMenu({name: tab.name, x: e.clientX, y: e.clientY}); }}>
                {tab.kind === 'autox' ? <span class="view-tab-kind">autoX · </span> : null}
                <span class="plot-tab-title"><ShownName name={tab.name} copy={false} /></span>
                {out && <span class="view-tab-out" title="In a window of its own">
                  <span aria-hidden="true">⧉</span><span class="visually-hidden"> (in a window of its own)</span></span>}
              </button>
              {!isMain && <button class="view-tab-close" aria-label={`Close ${tab.kind === 'autox' ? 'autoX view' : 'graph'} ${tab.name}`}
                title={tab.kind === 'autox' ? 'Done with this view: a running continuation is stopped first' : 'Close this graph'}
                disabled={tab.kind === 'graph' && windowOff}
                onClick={() => tab.kind === 'autox' ? session.closeAutox(tab.index) : session.closeWindow(tab.name)}>×</button>}
            </span>
          );
        })}
      </div>
      {group !== 'all' || !narrow ? (
        <button class="group-split" aria-label="Split right" disabled={splitOff}
          title={splitOff ? 'Pick a view other than the main graph, which stays in the first group' : 'Show this view beside the first group (Split right)'}
          onClick={() => shownTab && session.splitWindow(shownTab.name, 'right')}>◫</button>
      ) : null}
      {children}
      {menu && (() => {
        const tab = tabs.find(t => t.name === menu.name);
        const view = tab && viewOf(tab);
        const out = !!view && popouts.includes(viewKeyText(view));
        const left = tab && beside(tab, -1), right = tab && beside(tab, 1);
        const mayMove = may({cmd: 'window.move'});
        /* a view other than the main graph splits; a window of the second group goes back to the first, on a phone too */
        const grouped = !!tab && tab.name !== mainName && may({cmd: 'window.split'});
        return <ContextMenu x={menu.x} y={menu.y} label={`View ${menu.name}`} close={() => setMenu(null)}
          items={[{label: 'Rename', off: !may({cmd: 'window.rename'}), run: () => startRename(menu.name)},
            {label: 'Move left', off: !left || !mayMove, run: () => session.moveWindowBeside(menu.name, -1)},
            {label: 'Move right', off: !right || !mayMove, run: () => session.moveWindowBeside(menu.name, 1)},
            ...(grouped && !narrow ? [{label: 'Split right', run: () => session.splitWindow(menu.name, 'right')},
              {label: 'Split down', run: () => session.splitWindow(menu.name, 'down')}] : []),
            ...(tab?.second && may({cmd: 'window.join'}) ? [{label: 'Move to first group', run: () => session.joinWindow(menu.name)}] : []),
            {label: out ? 'Pop in' : 'Pop out', off: !view, run: () => view && (out ? session.popouts.popIn(view) : void session.popouts.show(view))}]} />;
      })()}
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
