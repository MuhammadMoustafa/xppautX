import {CommandButton, CommandLabel, useCommandLabel} from './CommandButton';
/* The plot windows (docs/ui-v2.md T6): one PlotView per core window, the one
   selected shown, each with its own zoom; the tabs that pick one are ui/ViewTabs.tsx's. */
import {PLOT_KEYS_HELP} from '../plot/plotKeys';
import {useMay, useMayMain, useSession, useStore} from './context';
import {Plot3DView} from './Plot3DView';
import {PlotView} from './PlotView';
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

export function Plots({dark}: {dark: boolean}) {
  const session = useSession();
  const all = useStore(s => s.plots.windows);
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
  const shownWins = windows.length ? windows.map(w => w.win) : ['graph_1'];
  const tabbed = windows.length > 1;

  /* the window row (the tabs, the name, New window, Window options, Capture) is drawn in the header of the
     window that is shown (W639): the plot's own tools stay beside it */
  const windowRow = (
      <div class="plot-windows">
        <div class="plot-window-tools">
          <details class="plot-window-options">
            <summary>Window options</summary>
            <div class="plot-window-options-menu">
                <button class="small" disabled={windowOff} onClick={()=>session.send({cmd:'window.redraw',window:active})}><CommandLabel command="window.redraw" /></button>
                <button class="small" disabled={windowOff} onClick={()=>session.send({cmd:'window.clear',window:active})}><CommandLabel command="window.clear" /></button>
                {['annotation.add_text','annotation.add_arrow','annotation.add_pointer','annotation.add_marker'].map(command => <CommandButton command={command} />)}
                <button class="small" disabled={windowOff} onClick={()=>session.send({cmd:'window.close_extra'})}><CommandLabel command="window.close_extra" /></button>
                <button class="small" disabled={windowOff} onClick={()=>session.send({cmd:'window.send_to_bottom',window:active})}><CommandLabel command="window.send_to_bottom" /></button>
                <select aria-label={redrawModeLabel} disabled={windowOff} value={current?.info?.redraw_mode} onChange={e=>session.send({cmd:'window.set_redraw_mode',window:active,mode:e.currentTarget.value})}>
                  {modes.map(mode=><option value={mode}>{choiceLabel(mode)}</option>)}
                </select>
                <label><input type="checkbox" checked={simultaneous} disabled={windowOff} onChange={e=>session.send({cmd:'window.set_simultaneous_plot',enabled:e.currentTarget.checked})}/><CommandLabel command="window.set_simultaneous_plot" /></label>
            </div>
          </details>
          <KinescopeBar />
        </div>
      </div>
  );
  return (
    <div class="plots">
      {shownWins.map(win => {
        const shown = win === active || shownWins.length === 1;
        const three = windows.find(w => w.win === win)?.info?.three;
        return three
          ? <Plot3DView key={win} win={win} dark={dark} shown={shown} tabbed={tabbed} windowRow={shown ? windowRow : null} />
          : <PlotView key={win} win={win} dark={dark} shown={shown} tabbed={tabbed} windowRow={shown ? windowRow : null} />;
      })}
      <p id="plot-keys-help" class="visually-hidden">{PLOT_KEYS_HELP}</p>
    </div>
  );
}
