/* What tests read and drive (tools/web2check.mjs): the store's state, the
   actions it took (newest last) and a plot window's chart, never pixels.
   window.__xpp exists in every build; only `send` changes anything, and it
   is what the UI itself does. `sent` lists the commands the page sent
   (newest last), such as an ask's answer. `longTasks` lists the main thread's tasks of
   more than 50 ms (the Long Tasks API), so a test can tell that a gesture
   never held a frame back longer than that. */
import {diagramChart} from './plot/diagramChart';
import {aniDrawInfo} from './ani/render';
import {bytesToBase64} from './plot/gif';
import {chartOf} from './plot/registry';
import type {Session} from './session';
import type {Action} from './store/state';

const KEEP = 200;

interface LongTask {
  start: number;
  duration: number;
}

function watchLongTasks(): LongTask[] {
  const tasks: LongTask[] = [];
  try {
    new PerformanceObserver(list => {
      for (const e of list.getEntries()) {
        tasks.push({start: e.startTime, duration: e.duration});
        if (tasks.length > KEEP) tasks.shift();
      }
    }).observe({type: 'longtask', buffered: true});
  } catch {
    /* a browser without the Long Tasks API: the list stays empty */
  }
  return tasks;
}

export function installTestHook(session: Session): void {
  const actions: string[] = [];
  const sent: unknown[] = [];
  const tasks = watchLongTasks();
  /* the `diagram` events as they came, so a test can rebuild the diagram on its
     own: from the last that started it over (a reset to no points), for AUTO's
     window as it is now */
  let diagramEvents: unknown[] = [];
  const dispatch = session.store.dispatch;
  session.store.dispatch = (a: Action) => {
    actions.push(a.type === 'event' ? `event:${a.ev.ev}` : a.type === 'viewport' ? `viewport${a.push ? ':push' : ''}` : a.type);
    if (actions.length > KEEP) actions.shift();
    if (a.type === 'event' && a.ev.ev === 'diagram') {
      if (a.ev.op === 'reset' && !a.ev.keep) diagramEvents = [];
      diagramEvents.push(a.ev);
    } else if (a.type === 'event' && a.ev.ev === 'window' && a.ev.win === 101 && a.ev.op === 'destroy') diagramEvents = [];
    if (a.type === 'sent') {
      sent.push(a.cmd);
      if (sent.length > KEEP) sent.shift();
    }
    dispatch(a);
  };
  (window as unknown as {__xpp: unknown}).__xpp = {
    state: () => session.store.getState(),
    actions: () => actions.slice(),
    sent: () => sent.slice(),
    /** window `win`'s chart (the active window's by default) */
    plot: (win?: number) => chartOf(win ?? session.store.getState().plots.active)?.info() ?? null,
    /** the AUTO diagram's chart: its curves, label marks and ranges (null while AUTO is not shown) */
    diagram: () => diagramChart()?.info() ?? null,
    /** every `diagram` event received, oldest first */
    diagramEvents: () => diagramEvents.slice(),
    /** long tasks that started at or after `since` (performance.now() milliseconds) */
    longTasks: (since = 0) => tasks.filter(t => t.start >= since),
    longTasksSupported: () => PerformanceObserver.supportedEntryTypes?.includes('longtask') ?? false,
    send: (cmd: {cmd: string}) => session.send(cmd),
    /** the animation's last drawing: the frame, its primitive count, the canvas and the box on it */
    ani: () => aniDrawInfo(),
    /** Stop: a client-only action, nothing to send (session.ts kinescopeStop) */
    kinescopeStop: () => session.kinescopeStop(),
    /** the captured frames as an animated GIF (plot/gif.ts), base64; null with nothing captured */
    kinescopeGif: () => {
      const bytes = session.kinescopeGifBytes();
      return bytes ? bytesToBase64(bytes) : null;
    },
  };
}
