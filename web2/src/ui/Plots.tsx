import {CommandButton} from './CommandButton';
/* The plot windows as tabs (docs/ui-v2.md T6): one tab per core window, the
   active one selected, each tab its own PlotView with its own zoom. The tabs
   follow the core: window.create adds one, close and destroy_all remove
   them (the `plots` event), and picking a tab selects that named window, so keys and menus act on the window shown.
   Keyboard: the WAI-ARIA tabs pattern, automatic activation (arrow keys,
   Home, End move and select; Tab leaves the tab list). */
import {useEffect, useRef} from 'preact/hooks';
import {PLOT_KEYS_HELP} from '../plot/plotKeys';
import {BUSY_TITLE, useMay, useMayMain, useSession, useStore} from './context';
import {Plot3DView} from './Plot3DView';
import {PlotView} from './PlotView';
import {WindowName} from './WindowName';
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
        title="Kinescope/Capture: keep this plot as a frame (k, c)">Capture</button>
      {frames.length > 0 && (
        <>
          <span class="kinescope-count">{frames.length} frame{frames.length === 1 ? '' : 's'}
            {playing && shown !== null ? `, showing ${shown + 1}` : ''}</span>
          <button class="small" disabled={busy || playing} onClick={() => session.kinescopePlay()}
            title="Kinescope/Playback: show the captured frames (k, p)">Play</button>
          <button class="small" disabled={!playing} onClick={() => session.kinescopeStop()}>Stop</button>
          <button class="small" disabled={busy || playing} onClick={() => session.downloadKinescopeGif()}
            title="Export the captured frames as an animated GIF (K, M)">
            Export GIF
          </button>
          <button class="small" disabled={busy} onClick={() => session.kinescopeReset()}
            title="Kinescope/Reset: clear the captured frames (k, r)">Reset</button>
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
  /* a tab pick is a view: it works during a run too, only an open question waits (W95) */
  const tabOff = !may({cmd:'window.select',window:active});
  const windowOff = !may({cmd:'window.create'}); /* Makewindow */
  const tabs = useRef<HTMLDivElement>(null);
  /* the tab a key picked: a pick while the core is busy is held for its idle
     (session.selectWindow); the tab the key picked gets the focus once it is
     the active one, so the next key is the tabs' again, not an XPP hotkey (W93) */
  const keyPicked = useRef<string | null>(null);
  useEffect(() => {
    if (keyPicked.current !== active) return;
    keyPicked.current = null;
    tabs.current?.querySelectorAll<HTMLElement>('[role=tab]')[windows.findIndex(w=>w.win===active)]?.focus();
  }, [active]);
  /* before the first `plots` or `series`: window 1, empty */
  const shownWins = windows.length ? windows.map(w => w.win) : ['graph_1'];
  const tabbed = windows.length > 1;

  const onKeyDown = (e: KeyboardEvent) => {
    const i = windows.findIndex(w => w.win === active);
    const n = windows.length;
    const next = e.key === 'ArrowRight' || e.key === 'ArrowDown' ? (i + 1) % n
      : e.key === 'ArrowLeft' || e.key === 'ArrowUp' ? (i - 1 + n) % n
      : e.key === 'Home' ? 0 : e.key === 'End' ? n - 1 : -1;
    if (next < 0 || i < 0) return;
    e.preventDefault();
    e.stopPropagation(); /* not an XPP hotkey */
    const win = windows[next].win;
    keyPicked.current = win;
    session.selectWindow(win);
    tabs.current?.querySelectorAll<HTMLElement>('[role=tab]')[next]?.focus();
  };

  return (
    <div class="plots">
      <div class="plot-windows">
        {tabbed && (
          <div class="plot-tabs" role="tablist" aria-label="Plot windows" ref={tabs} onKeyDown={onKeyDown}>
            {windows.map(w => (
              <button
                key={w.win}
                id={`plot-tab-${encodeURIComponent(w.win)}`}
                class="plot-tab"
                role="tab"
                aria-selected={w.win === active}
                aria-controls={`plot-panel-${encodeURIComponent(w.win)}`}
                tabIndex={w.win === active ? 0 : -1}
                disabled={tabOff && w.win !== active}
                title={tabOff && w.win !== active ? BUSY_TITLE : undefined}
                onClick={() => session.selectWindow(w.win)}>
                                <span class="plot-tab-title">{w.win}</span>
              </button>
            ))}
          </div>
        )}
        <div class="plot-window-tools">
          <WindowName name={active} disabled={windowOff} />
          <button class="small" disabled={windowOff} onClick={() => session.newWindow()}
            title="Create a graph copied from the active graph">New window</button>
          {tabbed && (
            <button class="small" disabled={windowOff || active === windows[0]?.win} onClick={() => session.closeWindow()}
              title="Close this graph; the main graph stays open">Close window</button>
          )}
          <details class="plot-window-options">
            <summary>Window options</summary>
            <div class="plot-window-options-menu">
                <button class="small" disabled={windowOff} onClick={()=>session.send({cmd:'window.redraw',window:active})}>Redraw plot</button>
                <button class="small" disabled={windowOff} onClick={()=>session.send({cmd:'window.clear',window:active})}>Clear plot</button>
                {['annotation.add_text','annotation.add_arrow','annotation.add_pointer','annotation.add_marker'].map(command => <CommandButton command={command} />)}
                <button class="small" disabled={windowOff} onClick={()=>session.send({cmd:'window.destroy_all'})}>Close extra windows</button>
                <button class="small" disabled={windowOff} onClick={()=>session.send({cmd:'window.send_to_bottom',window:active})}>Send to bottom</button>
                <select aria-label="Redraw mode" disabled={windowOff} value={current?.info?.redraw_mode} onChange={e=>session.send({cmd:'window.set_redraw_mode',window:active,mode:e.currentTarget.value})}>
                  {modes.map(mode=><option value={mode}>{choiceLabel(mode)}</option>)}
                </select>
                <label><input type="checkbox" checked={simultaneous} disabled={windowOff} onChange={e=>session.send({cmd:'window.set_simultaneous_plot',enabled:e.currentTarget.checked})}/>Simultaneous plot</label>
            </div>
          </details>
          <KinescopeBar />
        </div>
      </div>
      {shownWins.map(win => {
        const shown = win === active || shownWins.length === 1;
        const three = windows.find(w => w.win === win)?.info?.three;
        return three
          ? <Plot3DView key={win} win={win} dark={dark} shown={shown} tabbed={tabbed} />
          : <PlotView key={win} win={win} dark={dark} shown={shown} tabbed={tabbed} />;
      })}
      <p id="plot-keys-help" class="visually-hidden">{PLOT_KEYS_HELP}</p>
    </div>
  );
}
