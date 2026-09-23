/* The plot windows as tabs (docs/ui-v2.md T6): one tab per core window, the
   active one selected, each tab its own PlotView with its own zoom. The tabs
   follow the core: Makewindow/Create adds one, Destroy and Kill all remove
   them (the `plots` event), and picking a tab makes that window the core's
   active one (`click`), so keys and menus act on the window shown.
   Keyboard: the WAI-ARIA tabs pattern, automatic activation (arrow keys,
   Home, End move and select; Tab leaves the tab list). */
import {useRef} from 'preact/hooks';
import {PLOT_KEYS_HELP} from '../plot/plotKeys';
import type {PlotWindow} from '../store/plots';
import {useSession, useStore} from './context';
import {Plot3DView} from './Plot3DView';
import {PlotView} from './PlotView';

function tabTitle(w: PlotWindow): string {
  if (w.info) return w.info.title;
  const c = w.series?.curves[0], name = (col: number) => w.series?.names.get(col) ?? (col === 0 ? 'T' : '');
  return c ? `${name(c.y)} vs ${name(c.x)}` : '';
}

export function Plots({dark}: {dark: boolean}) {
  const session = useSession();
  const windows = useStore(s => s.plots.windows);
  const active = useStore(s => s.plots.active);
  const busy = useStore(s => s.busy);
  const tabs = useRef<HTMLDivElement>(null);
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
                onClick={() => session.selectWindow(w.win)}>
                <span class="plot-tab-num">{w.win}</span>
                <span class="plot-tab-title">{tabTitle(w)}</span>
              </button>
            ))}
          </div>
        )}
        <div class="plot-window-tools">
          <button class="small" disabled={busy} onClick={() => session.newWindow()}
            title="Makewindow/Create: a new plot window, a copy of this one (M, C)">New window</button>
          {tabbed && (
            <button class="small" disabled={busy || active === 1} onClick={() => session.closeWindow()}
              title="Makewindow/Destroy: close this plot window (M, D); window 1 stays">Close window</button>
          )}
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
