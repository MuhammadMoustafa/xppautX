/* The animation (docs/ui-v2.md T13): the core's frames (the `ani` `frame`
   event, in unit coordinates) drawn on a canvas of any size at the
   dimension box's own aspect (ani/render.ts), and the player: Play/Pause,
   a frame back and forward, first and last, a seek slider over the stored
   rows, the delay between frames, Load and Grab. A floating panel (R6: a
   side panel from 48rem, a full-screen sheet with Back under that), opened
   by the title bar's Animation button or when the core opens its
   animation window (Viewaxes/Toon, -anifile).

   Keys on the picture (A3): Space plays or pauses, the arrows step a frame
   (Shift: ten), PageUp/PageDown ten, Home/End go to the first and last
   frame, Escape closes the panel. Nothing plays by itself (A6): Go is only
   ever the user's. */
import {useEffect, useRef, useState} from 'preact/hooks';
import {fromCanvas} from '../ani/frame';
import {drawAniFrame, frameBox} from '../ani/render';
import {HELP} from '../help/links';
import {sixSig} from '../store/values';
import {useSession, useStore} from './context';
import {HelpButton} from './HelpButton';
import {useDark} from './theme';

/** the delays offered, ms between two frames of Go */
const DELAYS = [0, 5, 10, 20, 50, 100, 200, 500, 1000];

export function AniView() {
  const session = useSession();
  const open = useStore(s => s.ani.open);
  const exists = useStore(s => s.ani.exists);
  const loaded = useStore(s => s.ani.loaded);
  const playing = useStore(s => s.ani.playing);
  const frame = useStore(s => s.ani.frame);
  const aniRows = useStore(s => s.ani.rows);
  const coreRows = useStore(s => s.core?.rows ?? 0);
  const speed = useStore(s => s.ani.speed);
  const grab = useStore(s => s.ani.grab);
  const busy = useStore(s => s.busy);
  const theme = useStore(s => s.theme);
  const dark = useDark(theme);
  const rows = Math.max(aniRows, coreRows);
  const last = Math.max(0, rows - 1);
  const shown = frame?.pos ?? 0;

  const panel = useRef<HTMLElement>(null);
  const stage = useRef<HTMLDivElement>(null);
  const canvas = useRef<HTMLCanvasElement>(null);
  const [size, setSize] = useState({w: 0, h: 0});
  /* the slider while it is dragged; the frame's own position otherwise */
  const [seeking, setSeeking] = useState<number | null>(null);

  const close = () => session.closeAni();

  /* focus to the picture on open, back to the toggle on close; Escape inside closes */
  useEffect(() => {
    if (!open) {
      if (panel.current?.contains(document.activeElement)) document.querySelector<HTMLElement>('.ani-toggle')?.focus();
      return;
    }
    stage.current?.focus();
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== 'Escape' || session.store.getState().ask) return;
      if (!panel.current?.contains(document.activeElement)) return;
      e.preventDefault();
      e.stopPropagation();
      close();
    };
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, [open]);

  useEffect(() => {
    const el = stage.current;
    if (!el) return;
    const ro = new ResizeObserver(() => setSize({w: el.clientWidth, h: el.clientHeight}));
    ro.observe(el);
    setSize({w: el.clientWidth, h: el.clientHeight});
    return () => ro.disconnect();
  }, []);

  useEffect(() => {
    if (!open || !canvas.current || !size.w || !size.h) return;
    const surface = getComputedStyle(document.documentElement).getPropertyValue('--surface').trim() || '#fff';
    drawAniFrame(canvas.current, size.w, size.h, frame, dark, surface);
  }, [open, frame, size.w, size.h, dark]);

  const playPause = () => (playing ? session.aniPause() : session.aniPlay());
  const canStep = loaded && rows >= 2;

  const onKeyDown = (e: KeyboardEvent) => {
    if (e.ctrlKey || e.metaKey || e.altKey || !canStep) return;
    const big = e.shiftKey ? 10 : 1;
    const act: Record<string, () => void> = {
      ' ': playPause,
      ArrowLeft: () => session.aniStep(-big),
      ArrowDown: () => session.aniStep(-big),
      ArrowRight: () => session.aniStep(big),
      ArrowUp: () => session.aniStep(big),
      PageUp: () => session.aniStep(-10),
      PageDown: () => session.aniStep(10),
      Home: () => session.aniSeek(0),
      End: () => session.aniSeek(last),
    };
    const f = act[e.key];
    if (!f) return;
    e.preventDefault();
    e.stopPropagation();
    f();
  };

  /* Grab: the pointer on the picture, in unit coordinates */
  const lastMove = useRef(0);
  const pointer = (what: 'down' | 'move' | 'up', e: PointerEvent) => {
    if (!grab || !frame || !canvas.current) return;
    const r = canvas.current.getBoundingClientRect();
    const [u, v] = fromCanvas(frameBox(r.width, r.height, frame.dim), e.clientX - r.left, e.clientY - r.top);
    if (what === 'move') {
      if (!(e.buttons & 1) || e.timeStamp - lastMove.current < 50) return;
      lastMove.current = e.timeStamp;
    }
    if (what === 'down') (e.target as Element).setPointerCapture?.(e.pointerId);
    session.aniPointer(what, u, v);
  };

  const speeds = DELAYS.includes(speed) ? DELAYS : [...DELAYS, speed].sort((a, b) => a - b);
  const status = !exists ? 'The animation window is closed.'
    : !loaded ? 'No animation loaded: Load an .ani file.'
      : rows < 2 ? 'No data yet: integrate first. An animation plays the stored rows.'
        : !frame ? 'Loaded. Play, step or seek to draw a frame.'
          : `Frame ${shown} of ${rows}` + (frame.t === null ? '' : `, t = ${sixSig(frame.t)}`)
            + (playing ? '. Playing.' : '') + (grab ? '. Grab: drag a point on the picture.' : '');

  return (
    <section id="ani-panel" ref={panel} class={'ani-panel' + (open ? ' open' : '')} aria-label="Animation">
      <div class="ani-header">
        <button class="ani-back" onClick={close}>Back</button>
        <h2>Animation</h2>
        <HelpButton target={HELP.animation} label="the animation" />
        {!exists && <button onClick={() => session.openAni()} disabled={busy}>Open</button>}
        <button onClick={() => session.aniLoad()} disabled={busy || !exists} title="Load an animation (.ani) file">
          Load…
        </button>
      </div>
      <div class={'ani-stage' + (grab ? ' grabbing' : '')} ref={stage} tabIndex={0} role="application"
        aria-roledescription="animation" aria-label={`Animation. ${status}`} aria-describedby="ani-keys"
        onKeyDown={onKeyDown} onPointerDown={e => pointer('down', e)} onPointerMove={e => pointer('move', e)}
        onPointerUp={e => pointer('up', e)}>
        <canvas ref={canvas} class="ani-canvas" aria-hidden="true" />
      </div>
      <p id="ani-keys" class="visually-hidden">
        Space plays or pauses; the arrow keys step a frame, with Shift ten; Home and End go to the first and last frame.
      </p>
      <p class="ani-info" role="status">{status}</p>
      <div class="ani-seek">
        <input type="range" class="ani-slider" min={0} max={last} step={1} value={seeking ?? shown}
          disabled={!canStep} aria-label="Frame" aria-valuetext={`Frame ${seeking ?? shown} of ${rows}`}
          onInput={e => setSeeking(Number((e.target as HTMLInputElement).value))}
          onChange={e => {
            setSeeking(null);
            session.aniSeek(Number((e.target as HTMLInputElement).value));
          }} />
      </div>
      <div class="ani-controls" role="group" aria-label="Player">
        <button onClick={() => session.aniSeek(0)} disabled={!canStep} title="First frame (Home)"
          aria-label="First frame">⏮</button>
        <button onClick={() => session.aniStep(-1)} disabled={!canStep} title="One frame back (Left arrow)"
          aria-label="One frame back">◀</button>
        <button class="primary ani-play" onClick={playPause} disabled={!canStep || (busy && !playing)}
          title="Play or pause (Space)" aria-pressed={playing}>
          {playing ? 'Pause' : 'Play'}
        </button>
        <button onClick={() => session.aniStep(1)} disabled={!canStep} title="One frame forward (Right arrow)"
          aria-label="One frame forward">▶</button>
        <button onClick={() => session.aniSeek(last)} disabled={!canStep} title="Last frame (End)"
          aria-label="Last frame">⏭</button>
        <label class="ani-speed">
          <span>Delay</span>
          <select value={String(speed)} disabled={!loaded}
            onChange={e => session.aniSpeed(Number((e.target as HTMLSelectElement).value))}>
            {speeds.map(ms => <option key={ms} value={String(ms)}>{ms} ms</option>)}
          </select>
        </label>
        <button onClick={() => session.aniGrab()} disabled={!canStep || busy} aria-pressed={grab}
          title="Drag the animation's grab points with the pointer">Grab</button>
      </div>
    </section>
  );
}
