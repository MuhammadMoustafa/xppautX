/* The plot windows as tabs (docs/ui-v2.md T6): one tab per core window, the
   active one selected, each tab its own PlotView with its own zoom. The tabs
   follow the core: Makewindow/Create adds one, Destroy and Kill all remove
   them (the `plots` event), and picking a tab makes that window the core's
   active one (`click`), so keys and menus act on the window shown.
   Keyboard: the WAI-ARIA tabs pattern, automatic activation (arrow keys,
   Home, End move and select; Tab leaves the tab list). */
import {useEffect, useRef} from 'preact/hooks';
import {PLOT_KEYS_HELP} from '../plot/plotKeys';
import type {PlotWindow} from '../store/plots';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {Plot3DView} from './Plot3DView';
import {PlotView} from './PlotView';

function tabTitle(w: PlotWindow): string {
  if (w.info) return w.info.title;
  const c = w.series?.curves[0], name = (col: number) => w.series?.names.get(col) ?? (col === 0 ? 'T' : '');
  return c ? `${name(c.y)} vs ${name(c.x)}` : '';
}

/** the Kinescope's own controls (docs/ui-v2.md T15): the core's menu
    (Capture, Reset, Playback, Make Anigif/Export GIF) plus Stop, the
    client's own (the core keeps no play position). Shown once there is
    something to play or export; Capture works from the start. */
function KinescopeBar() {
  const session = useSession();
  const {frames, playing, shown} = useStore(s => s.kinescope);
  const may = useMay();
  const busy = !may({cmd: 'key', key: 'k'}); /* the Kinescope menu's kind (W95) */
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
            title="Kinescope/Make AniGif (k, m): an animated GIF of the captured frames, written by the core and downloaded">
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
  const windows = useStore(s => s.plots.windows);
  const active = useStore(s => s.plots.active);
  const may = useMay();
  /* a tab pick is a view: it works during a run too, only an open question waits (W95) */
  const tabOff = !may({cmd: 'click', win: active});
  const windowOff = !may({cmd: 'key', key: 'm'}); /* Makewindow */
  const tabs = useRef<HTMLDivElement>(null);
  /* the tab a key picked: a pick while the core is busy is held for its idle
     (session.selectWindow); the tab the key picked gets the focus once it is
     the active one, so the next key is the tabs' again, not an XPP hotkey (W93) */
  const keyPicked = useRef<number | null>(null);
  useEffect(() => {
    if (keyPicked.current !== active) return;
    keyPicked.current = null;
    tabs.current?.querySelector<HTMLElement>(`#plot-tab-${active}`)?.focus();
  }, [active]);
  /* before the first `plots` or `series`: window 1, empty */
  const shownWins = windows.length ? windows.map(w => w.win) : [1];
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
    tabs.current?.querySelector<HTMLElement>(`#plot-tab-${win}`)?.focus();
  };

  return (
    <div class="plots">
      <div class="plot-windows">
        {tabbed && (
          <div class="plot-tabs" role="tablist" aria-label="Plot windows" ref={tabs} onKeyDown={onKeyDown}>
            {windows.map(w => (
              <button
                key={w.win}
                id={`plot-tab-${w.win}`}
                class="plot-tab"
                role="tab"
                aria-selected={w.win === active}
                aria-controls={`plot-panel-${w.win}`}
                tabIndex={w.win === active ? 0 : -1}
                disabled={tabOff && w.win !== active}
                title={tabOff && w.win !== active ? BUSY_TITLE : undefined}
                onClick={() => session.selectWindow(w.win)}>
                <span class="plot-tab-num">{w.win}</span>
                <span class="plot-tab-title">{tabTitle(w)}</span>
              </button>
            ))}
          </div>
        )}
        <div class="plot-window-tools">
          <button class="small" disabled={windowOff} onClick={() => session.newWindow()}
            title="Makewindow/Create: a new plot window, a copy of this one (M, C)">New window</button>
          {tabbed && (
            <button class="small" disabled={windowOff || active === 1} onClick={() => session.closeWindow()}
              title="Makewindow/Destroy: close this plot window (M, D); window 1 stays">Close window</button>
          )}
        </div>
        <KinescopeBar />
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
