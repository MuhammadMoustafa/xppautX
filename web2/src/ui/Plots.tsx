import type {ComponentChildren} from 'preact';
import {useState} from 'preact/hooks';
import {CommandButton, CommandLabel, useCommandLabel} from './CommandButton';
/* The plot windows (docs/ui-v2.md T6): one PlotView per core window, the one
   selected shown, each with its own zoom; the tabs that pick one are ui/ViewTabs.tsx's. In a split view (W720, ui/split.ts) the shown
   one of each group is drawn, placed by its group (css/plot.css), with the bar between them (ui/GroupSplitter.tsx) and, when the
   groups are stacked, the second group's strip over its plot; a tab dragged over the plots shades where it would go. */
import {PLOT_KEYS_HELP} from '../plot/plotKeys';
import {useMay, useMayMain, useSession, useStore} from './context';
import {Plot3DView} from './Plot3DView';
import {PlotView} from './PlotView';
import {PoppedOut} from './PoppedOut';
import {viewKeyText} from '../popout';
import {AutoxGroupView} from './AutoxView';
import {GroupSplitter} from './GroupSplitter';
import {PlotOptions} from './PlotOptions';
import {draggedView, GroupStrip} from './ViewTabs';
import {shownInGroup, useSplitView, type SplitView} from './split';
import {choiceLabel} from '../store/fieldKinds';

/** the Kinescope's own controls (docs/ui-v2.md T15): the core's menu
    (Capture, Reset, Playback, Make Anigif/Export GIF) plus Stop, the
    client's own (the core keeps no play position). Shown once there is
    something to play or export; Capture works from the start. */
function KinescopeBar() {
  const session = useSession();
  const {frames, playing, shown} = useStore(s => s.kinescope);
  const mayMain = useMayMain();
  const busy = !mayMain('kinescope.capture'); /* the Kinescope menu's kind (W95) */
  return (
    <div class="kinescope-bar" role="group" aria-label="Kinescope">
      <button class="small" disabled={busy} onClick={() => session.kinescopeCapture()}
        title="Keep this plot as a frame"><CommandLabel command="kinescope.capture" /></button>
      {frames.length > 0 && (
        <>
          <span class="kinescope-count">{frames.length} frame{frames.length === 1 ? '' : 's'}
            {playing && shown !== null ? `, showing ${shown + 1}` : ''}</span>
          <button class="small" disabled={busy || playing} onClick={() => session.kinescopePlay()}
            title="Show the captured frames"><CommandLabel command="kinescope.play" /></button>
          <button class="small" disabled={!playing} onClick={() => session.kinescopeStop()}>Stop</button>
          <button class="small" disabled={busy || playing} onClick={() => session.downloadKinescopeGif()}
            title="Export the captured frames as an animated GIF">
            <CommandLabel command="kinescope.export_gif" />
          </button>
          <button class="small" disabled={busy} onClick={() => session.kinescopeReset()}
            title="Clear the captured frames"><CommandLabel command="kinescope.reset" /></button>
          <CommandButton command="kinescope.export_frames" />
          <CommandButton command="kinescope.autoplay" />
        </>
      )}
    </div>
  );
}

/** one graph window: its 3-D or its 2-D view (the plots column's and a pop-out's) */
export function GraphView({win, dark, shown, focused, group, tabbed, windowRow}:
  {win: string; dark: boolean; shown: boolean; focused: boolean; group?: 'first' | 'second'; tabbed: boolean; windowRow: ComponentChildren}) {
  const three = useStore(s => s.plots.windows.find(w => w.win === win)?.info?.three);
  return three
    ? <Plot3DView win={win} dark={dark} shown={shown} focused={focused} group={group} tabbed={tabbed} windowRow={windowRow} />
    : <PlotView win={win} dark={dark} shown={shown} focused={focused} group={group} tabbed={tabbed} windowRow={windowRow} />;
}

/** where a dragged tab would go if dropped on the plots: a new second group beside or below (no split yet), or the other group */
type DropZone = {split: 'right' | 'down'} | {group: 'first' | 'second'};

/** the zone under the pointer at (fx, fy), fractions of the plots' area, for the graph tab being dragged (null: no drop here) */
function zoneAt(fx: number, fy: number, split: SplitView | null, mainName: string | undefined): DropZone | null {
  const from = draggedView();
  if (!from || from.name === mainName) return null;
  if (!split) return fx > 0.5 && fx - 0.5 >= fy - 0.5 ? {split: 'right'} : fy > 0.5 ? {split: 'down'} : null;
  const group = (split.direction === 'right' ? fx : fy) < split.share ? 'first' : 'second';
  return from.second === (group === 'second') ? null : {group};
}

/** the shaded part of the plots a drop would use, with its words */
function DropShade({zone, name, split}: {zone: DropZone; name: string; split: SplitView | null}) {
  const stacked = split ? split.direction === 'down' : 'split' in zone && zone.split === 'down';
  const share = split?.share ?? 0.5;
  /* the zone's start and size along the split's axis, as fractions of the area: the second half, or a group's part */
  const [from, size] = 'split' in zone || zone.group === 'second' ? [share, 1 - share] : [0, share];
  const at = stacked ? {top: `${from * 100}%`, height: `${size * 100}%`} : {left: `${from * 100}%`, width: `${size * 100}%`};
  const words = 'split' in zone ? `Drop: split ${zone.split} with “${name}”` : `Drop: move “${name}” to the ${zone.group} group`;
  return <div class={'drop-zone' + ('split' in zone ? ` drop-${zone.split}` : '')} style={at} role="status">{words}</div>;
}

export function Plots({dark}: {dark: boolean}) {
  const session = useSession();
  const all = useStore(s => s.plots.windows);
  const popouts = useStore(s => s.popouts);
  const active = useStore(s => s.plots.active);
  /* a hidden graph (window.hide) has no tab until it is shown again; the active one always has */
  const hidden = useStore(s => s.windows.filter(w => w.kind === 'graph' && !w.shown).map(w => w.name).join('\n')).split('\n');
  const windows = all.filter(w => w.win === active || !hidden.includes(w.win));
  const may = useMay();
  const current=all.find(w=>w.win===active);
  const modes=useStore(s=>s.hello?.commands.find(c=>c.cmd==='window.set_redraw_mode')?.arguments?.find(a=>a.name==='mode')?.choices ?? []);
  const simultaneous=useStore(s=>s.plots.simultaneous);
  const redrawModeLabel = useCommandLabel('window.set_redraw_mode');
  const windowOff = !may({cmd:'window.new'}); /* Makewindow */
  /* before the first `plots` or `series`: window 1, empty */
  const shownWins = windows.length ? windows.map(w => w.win) : ['main'];
  const tabbed = windows.length > 1;
  const split = useSplitView();
  const seconds = useStore(s => s.windows.filter(w => w.group === 'second').map(w => w.name).join('\n')).split('\n');
  const shownIn = [useStore(s => shownInGroup(s, 'first')), useStore(s => shownInGroup(s, 'second'))];
  const mainName = useStore(s => s.windows.find(w => w.kind === 'graph')?.name);
  const diagrams = useStore(s => s.windows.filter(w => w.kind === 'diagram').map(w => w.name).join('\n')).split('\n');
  const selected = useStore(s => s.split.selected);
  const [zone, setZone] = useState<DropZone | null>(null);
  /* the zone under a drag (a tab of ViewTabs.tsx) and what dropping it does */
  const over = (e: DragEvent): DropZone | null => {
    const box = (e.currentTarget as HTMLElement).getBoundingClientRect();
    return zoneAt((e.clientX - box.left) / box.width, (e.clientY - box.top) / box.height, split, mainName);
  };

  /* the window row (the tabs, the name, New window, Window options, Capture) is drawn in the header of the
     window that is shown (W639): the plot's own tools stay beside it */
  const windowRow = (
      <div class="plot-windows">
        <div class="plot-window-tools">
          <PlotOptions>
                <button class="small" disabled={windowOff} onClick={()=>session.send({cmd:'window.redraw',window:active})}><CommandLabel command="window.redraw" /></button>
                <button class="small" disabled={windowOff} onClick={()=>session.send({cmd:'window.clear',window:active})}><CommandLabel command="window.clear" /></button>
                {['annotation.add_text','annotation.add_arrow','annotation.add_pointer','annotation.add_marker'].map(command => <CommandButton command={command} />)}
                <button class="small" disabled={windowOff} onClick={()=>session.send({cmd:'window.close_extra'})}><CommandLabel command="window.close_extra" /></button>
                <button class="small" disabled={windowOff} onClick={()=>session.send({cmd:'window.send_to_bottom',window:active})}><CommandLabel command="window.send_to_bottom" /></button>
                <select aria-label={redrawModeLabel} disabled={windowOff} value={current?.info?.redraw_mode} onChange={e=>session.send({cmd:'window.set_redraw_mode',window:active,mode:e.currentTarget.value})}>
                  {modes.map(mode=><option value={mode}>{choiceLabel(mode)}</option>)}
                </select>
                <label><input type="checkbox" checked={simultaneous} disabled={windowOff} onChange={e=>session.send({cmd:'window.set_simultaneous_plot',enabled:e.currentTarget.checked})}/><CommandLabel command="window.set_simultaneous_plot" /></label>
            </PlotOptions>
          <KinescopeBar />
        </div>
      </div>
  );
  return (
    <div class={'plots' + (split ? ` split split-${split.direction}` : '')}
      onDragOver={e => {
        const next = over(e);
        if (!next) { if (zone) setZone(null); return; }
        e.preventDefault();
        if (JSON.stringify(next) !== JSON.stringify(zone)) setZone(next);
      }}
      onDragLeave={e => { if (!e.currentTarget.contains(e.relatedTarget as Node | null)) setZone(null); }}
      onDrop={e => {
        const target = over(e), from = draggedView();
        setZone(null);
        if (!target || !from) return;
        e.preventDefault();
        if ('split' in target) session.splitWindow(from.name, target.split);
        else if (target.group === 'second') session.splitWindow(from.name, split!.direction);
        else session.joinWindow(from.name);
      }}>
      {shownWins.map(win => {
        const group = split ? (seconds.includes(win) ? 'second' : 'first') : undefined;
        const shown = group ? win === shownIn[group === 'second' ? 1 : 0] : win === active || shownWins.length === 1;
        const focused = win === (split ? selected : active);
        /* a graph in a window of its own (popout.ts) is not drawn here too: its tab says where it is */
        if (popouts.includes(viewKeyText({kind: 'graph', name: win}))) return shown ? <div key={win} class="plot-slot" data-group={group}><PoppedOut view={{kind: 'graph', name: win}} /></div> : null;
        return <GraphView key={win} win={win} dark={dark} shown={shown} focused={focused} group={group} tabbed={tabbed} windowRow={shown && focused ? windowRow : null} />;
      })}
      {split && (['first', 'second'] as const).map((group, i) => {
        /* an autoX view shown in its group is drawn there beside the other group's view; the buttons are in the focused group's, or the only one's */
        const name = shownIn[i];
        if (!diagrams.includes(name)) return null;
        const focused = name === selected;
        return <AutoxGroupView key={'autox-' + group} name={name} group={group} focused={focused} dark={dark} />;
      })}
      {split && <GroupSplitter split={split} />}
      {split?.direction === 'down' && <GroupStrip group="second" />}
      {zone && draggedView() && <DropShade zone={zone} name={draggedView()!.name} split={split} />}
      <p id="plot-keys-help" class="visually-hidden">{PLOT_KEYS_HELP}</p>
    </div>
  );
}
