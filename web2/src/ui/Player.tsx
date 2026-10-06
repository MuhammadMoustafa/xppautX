/* The player (W59b, docs/mockups/record-play.html's player screen, docs/
   protocol.md "Playing a recording"): while the core plays a recording,
   the step's note as a large caption above the plot, the keys it presses
   as keycaps in a box at the top of the plot (lit one by one, as the
   core's `press` events say, each before the core sends it), the controls
   (Play/Pause, Step, Restart, the speed, one progress segment per step, a
   view step's half as wide), the step list with each note (click a step:
   edit its note and Save it into the .recx, or Play from here) and the
   banner of a recording changed after it was made. The core keeps the
   pace; the page only shows what it is told. */
import {useEffect, useLayoutEffect, useRef, useState} from 'preact/hooks';
import {caption, keycaps, litButton, segments} from '../store/player';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {Plots} from './Plots';



function ChangedBanner() {
  const session = useSession();
  const show = useStore(s => !s.player.intact && !s.player.dismissed);
  if (!show) return null;
  return (
    <div class="banner warn player-changed" role="alert">
      <span><b>This recording was changed after it was made.</b> A step or an embedded file no longer matches its
        fingerprint. It still plays, but it may not show what really happened.</span>
      <button class="small" onClick={() => session.store.dispatch({type: 'player', action: {type: 'dismiss'}})}>Dismiss</button>
    </div>
  );
}

function Caption() {
  const {text, note} = useStore(s => caption(s.player));
  return <div class={'player-caption' + (note ? '' : ' empty')} aria-live="polite">{text}</div>;
}

/* the keycaps at the top of the plot: what the running step presses */
function KeysBox() {
  const shown = useStore(s => keycaps(s.player));
  if (!shown) return null;
  return (
    <div class="player-keys" role="status" aria-label="What the step presses">
      <span class="player-keys-label">{shown.label}</span>
      {shown.caps.map((c, i) => (
        <span key={i} class={'keycap' + (c.down ? ' down' : '') + (c.ahead ? ' ahead' : '') + (c.text.length > 2 ? ' wide' : '')}>
          {c.text}
        </span>
      ))}
      {shown.answering && <span class="player-keys-label">then the answers</span>}
    </div>
  );
}

/* the control the step clicks, lit where it is on the page (data-button) */
function ButtonLight() {
  const id = useStore(s => litButton(s.player));
  /* AUTO's buttons exist only while its view shows: look again when it opens */
  const autoShown = useStore(s => s.diagram.open && s.diagram.shown);
  useEffect(() => {
    if (!id) return;
    const el = document.querySelector<HTMLElement>(`[data-button="${CSS.escape(id)}"]`);
    el?.classList.add('lit');
    return () => el?.classList.remove('lit');
  }, [id, autoShown]);
  return null;
}

function Controls() {
  const session = useSession();
  const may = useMay();
  const steps = useStore(s => s.player.steps);
  const cp = useStore(s => s.core?.player);
  const speedRange = useStore(s => s.hello?.player_speed);
  const progress = useStore(s => s.progress);
  const n = steps.length, next = cp?.step ?? 0, running = cp?.running ?? -1, playing = !!cp?.playing;
  const ready = n > 0 && !!cp;
  const ended = ready && next >= n && running < 0;
  const mayRestart = may({cmd: 'play', op: 'from'});
  const segs = segments(useStore(s => s.player), next);
  const frac = progress && progress.of > 0 ? Math.min(1, progress.n / progress.of) : 0.5;
  const at = running >= 0 ? running : Math.min(next, n) - 1;
  const play = () => {
    if (!ready) return;
    if (ended) {
      if (mayRestart) session.playFrom(0, true);
    } else session.play(playing ? 'pause' : 'start');
  };
  return (
    <section class="player-controls" aria-label="Player controls">
      <div class="player-buttons">
        <button class="primary player-play" onClick={play} disabled={!ready} aria-disabled={ended && !mayRestart}>
          {!ready ? 'Loading…' : playing ? '❚❚ Pause' : ended ? '▶ Play again' : '▶ Play'}
        </button>
        <button class="player-step-button" onClick={() => session.play('step')} disabled={!ready || ended} title="Play one step, then pause">
          Step &#9197;
        </button>
        <button class="player-restart" aria-label="Restart" aria-disabled={!mayRestart}
          title={mayRestart ? 'Back to the start, paused' : BUSY_TITLE} onClick={() => { if (mayRestart) session.playFrom(0, false); }}>
          &#8634;
        </button>
      </div>
      <div class="player-speed seg" role="group" aria-label="Speed">
        <span class="muted">Speed</span>
        {(speedRange ? Array.from({length: Math.round(Math.log2(speedRange.max / speedRange.min)) + 1}, (_, i) => speedRange.min * 2 ** i) : []).map(x => (
          <button key={x} class="small" aria-pressed={cp?.speed === x} onClick={() => session.playSpeed(x)}>{x}x</button>
        ))}
      </div>
      <div class="player-progress">
        <div class="player-segs" role="progressbar" aria-valuemin={0} aria-valuemax={n} aria-valuenow={Math.min(next, n)}
          aria-label="Steps played">
          {segs.map((g, i) => (
            <i key={i} class={(g.view ? 'view ' : '') + (g.done ? 'done' : g.now ? 'now' : '')} title={g.title}
              style={g.now ? {'--p': `${Math.round(frac * 100)}%`} : undefined} />
          ))}
        </div>
        <div class="player-progress-text">
          <span>Step {Math.max(at + 1, 0)} of {n}</span>
          <span>{at >= 0 && steps[at] ? steps[at].step : 'not started'}</span>
        </div>
      </div>
      <button class="small player-close" disabled={running >= 0} onClick={() => session.playClose()}
        title="Leave the player; the model stays">Close</button>
    </section>
  );
}

function NoteEditor({step}: {step: number}) {
  const session = useSession();
  const may = useMay();
  const st = useStore(s => s.player.steps[step]);
  const file = useStore(s => s.player.file);
  /* one editor a step (StepList keys it by the step): its text starts as the note */
  const [text, setText] = useState(st?.note ?? '');
  if (!st) return null;
  const mayWrite = may({cmd: 'play', op: 'note'});
  const name = file.split(/[\\/]/).pop();
  return (
    <div class="player-editor">
      <label>
        <b>Step {step + 1}: {st.step}</b>
        <span class="muted">Note shown above this step. It is not part of the fingerprint.</span>
        <textarea rows={2} value={text} onInput={e => setText((e.target as HTMLTextAreaElement).value)} />
      </label>
      <div class="player-editor-row">
        <button class="primary small" aria-disabled={!mayWrite} title={mayWrite ? `Write the note into ${name}` : BUSY_TITLE}
          onClick={() => {
            if (mayWrite) session.playNote(step, text.trim());
          }}>Save note</button>
        <button class="small" aria-disabled={!mayWrite} title={mayWrite ? 'Run the steps before it at once, then play from it' : BUSY_TITLE}
          onClick={() => { if (mayWrite) session.playFrom(step, true); }}>Play from here</button>
      </div>
      <span class="player-hint muted" aria-live="polite">
        {st.note === text.trim() && st.note !== '' ? `Saved in ${name}; the fingerprint is unchanged.` : ''}
      </span>
    </div>
  );
}

function StepList() {
  const session = useSession();
  const steps = useStore(s => s.player.steps);
  const selected = useStore(s => s.player.selected);
  const cp = useStore(s => s.core?.player);
  const next = cp?.step ?? 0, running = cp?.running ?? -1;
  const select = (step: number) => session.store.dispatch({type: 'player', action: {type: 'select', step}});
  return (
    <aside class="player-steps" aria-label="Steps">
      <h2 class="player-steps-title">Steps ({steps.length})</h2>
      <ol>
        {steps.map((s, i) => (
          <li key={i}>
            <button class={'player-step' + (i === selected ? ' sel' : '') + (i === running ? ' cur' : '') + (i < next && i !== running ? ' done' : '')}
              aria-current={i === running ? 'step' : undefined} onClick={() => select(i)}>
              {s.note && <span class="player-step-note">{`“${s.note}”`}</span>}
              <span class="player-step-label">
                <span class="n">{i + 1}</span><b>{s.step}</b>
                {s.view && <span class="tag">view</span>}
                {s.button && <span class="tag">button</span>}
              </span>
            </button>
          </li>
        ))}
      </ol>
      {selected >= 0 && <NoteEditor key={selected} step={selected} />}
    </aside>
  );
}

/* a full view covers the plots: the AUTO view, the data table, the text
   views, the animation, the array plot (each a fixed sheet or panel over
   the page) */
function useCovered(): boolean {
  return useStore(s => (s.diagram.open && s.diagram.shown) || s.table.open || s.text.open || s.ani.open || s.aplot.open);
}

/* the player while a view covers the plots: a dock over the view's top
   edge (the views leave its height free, theme.css) with the caption, the
   keycaps, the controls and the step list */
function Dock() {
  const ref = useRef<HTMLDivElement>(null);
  useLayoutEffect(() => {
    const el = ref.current;
    if (!el) return;
    const root = document.documentElement;
    const set = () => root.style.setProperty('--player-dock-h', `${el.offsetHeight}px`);
    root.classList.add('player-docked');
    set();
    const ro = new ResizeObserver(set);
    ro.observe(el);
    return () => {
      ro.disconnect();
      root.classList.remove('player-docked');
      root.style.removeProperty('--player-dock-h');
    };
  }, []);
  return (
    <div class="player-dock" ref={ref}>
      <Controls />
      <Caption />
      <KeysBox />
      <details class="player-dock-steps">
        <summary>Steps</summary>
        <StepList />
      </details>
    </div>
  );
}

/** the plots, and around them the player while a recording is open in it;
 *  a view over the plots gets the same player as a dock (the plots stay
 *  mounted in the same place, so they keep their state) */
export function PlayerStage({dark}: {dark: boolean}) {
  const open = useStore(s => s.player.open);
  const covered = useCovered();
  if (!open) return <Plots dark={dark} />;
  return (
    <div class="player">
      <div class="player-main">
        <ChangedBanner />
        {covered ? null : <Controls />}
        {covered ? null : <Caption />}
        <div class="player-stage">
          {covered ? null : <KeysBox />}
          <Plots dark={dark} />
        </div>
      </div>
      {covered ? <Dock /> : <StepList />}
      <ButtonLight />
    </div>
  );
}
