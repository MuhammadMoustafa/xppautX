/* The player (W59b, docs/protocol.md "Playing a recording"): the recording
   the core plays, its steps and notes (the `player` event), where it is
   (state.player), and what it presses next (the `press` event: the page
   lights it for `ms` before the core sends it). Pure: no DOM, no clock;
   the lighting's own timing is the components' (ui/Player.tsx). */
import type {PlayerEvent, PlayerStep, PressEvent, StateEvent} from '../protocol/types';

export interface PlayerState {
  /** a recording is open in the player */
  open: boolean;
  file: string;
  model: string;
  /** its fingerprint matches (false: changed after it was made; it plays all the same) */
  intact: boolean;
  steps: PlayerStep[];
  /** the changed-file banner was dismissed (until another recording opens) */
  dismissed: boolean;
  /** the step running (-1 between steps): its asks are the player's, never the page's */
  running: number;
  /** the step whose note the caption shows: the one running, else the last that ran (-1: none yet) */
  shown: number;
  /** what the running step presses next, and a count that tells two presses apart */
  press: PressEvent | null;
  presses: number;
  /** the step picked in the list, whose note is being edited (-1: none) */
  selected: number;
}

export const initialPlayer: PlayerState = {
  open: false, file: '', model: '', intact: true, steps: [], dismissed: false, running: -1, shown: -1,
  press: null, presses: 0, selected: -1,
};

export type PlayerAction =
  | {type: 'player'; ev: PlayerEvent}
  | {type: 'press'; ev: PressEvent}
  | {type: 'core'; player: StateEvent['player']}
  | {type: 'dismiss'}
  | {type: 'select'; step: number};

export function reducePlayer(state: PlayerState, a: PlayerAction): PlayerState {
  switch (a.type) {
    case 'player': {
      const same = state.open && state.file === a.ev.file;
      return {
        ...state, open: true, file: a.ev.file, model: a.ev.model, intact: a.ev.intact, steps: a.ev.steps,
        dismissed: same && state.dismissed, running: same ? state.running : -1, shown: same ? state.shown : -1,
        press: same ? state.press : null, selected: same && state.selected < a.ev.steps.length ? state.selected : -1,
      };
    }
    case 'press':
      return {...state, press: a.ev, presses: state.presses + 1, running: a.ev.step, shown: a.ev.step};
    case 'core': {
      if (!a.player) return state.open ? initialPlayer : state;
      const running = a.player.running;
      /* a restart: nothing has run yet */
      const shown = running >= 0 ? running : a.player.step === 0 ? -1
        : state.shown >= 0 ? state.shown : Math.min(a.player.step, state.steps.length) - 1;
      if (running === state.running && shown === state.shown && state.open) return state;
      return {...state, open: true, running, shown, press: running < 0 ? null : state.press};
    }
    case 'dismiss':
      return {...state, dismissed: true};
    case 'select':
      return {...state, selected: a.step};
  }
}

/** the caption above the plot: the shown step's note, or what stands for it */
export function caption(p: PlayerState): {text: string; note: boolean} {
  const st = p.shown >= 0 ? p.steps[p.shown] : undefined;
  if (!st) return {text: 'Press Play to watch the recording.', note: false};
  if (st.note) return {text: st.note, note: true};
  return {text: `Step ${p.shown + 1}: ${st.step} (no note)`, note: false};
}

/** one keycap of the box at the top of the plot */
export interface Keycap {
  text: string;
  /** pressed now (lit) */
  down: boolean;
  /** not reached yet (dimmed) */
  ahead: boolean;
}

/** a key's name as a keycap shows it */
export function keyText(k: string): string {
  return k.length === 1 ? k.toUpperCase() : k === 'Escape' ? 'Esc' : k;
}

/** what the keycap box shows while step `i` runs: its button, or its keys,
    lit up to the one pressed now, and whether its answers are being given */
export function keycaps(p: PlayerState): {label: string; caps: Keycap[]; answering: boolean} | null {
  const st = p.running >= 0 ? p.steps[p.running] : undefined;
  if (!st || !p.press || p.press.step !== p.running) return null;
  const {what, index} = p.press;
  if (what === 'cmd') return {label: 'Command', caps: [{text: st.step, down: true, ahead: false}], answering: false};
  if (st.button && what === 'key' && index === 0)
    return {label: 'Button', caps: [{text: st.button, down: true, ahead: false}], answering: false};
  const keys = st.keys ?? [];
  const at = what === 'key' ? index : keys.length;
  return {
    label: 'Keys',
    caps: keys.map((k, i) => ({text: keyText(k), down: i === at, ahead: i > at})),
    answering: what === 'answer' || what === 'alert',
  };
}

/** the main menu's key the running step presses now (its first key, no window), or null */
export function litMenuKey(p: PlayerState): string | null {
  const st = p.running >= 0 ? p.steps[p.running] : undefined;
  if (!st || !p.press || p.press.what !== 'key' || p.press.index !== 0 || st.win || st.button) return null;
  return st.keys?.[0] ?? null;
}

/** the control the running step clicks now (docs: data-button), or null */
export function litButton(p: PlayerState): string | null {
  const st = p.running >= 0 ? p.steps[p.running] : undefined;
  if (!st?.button || !p.press || p.press.what !== 'key' || p.press.index !== 0) return null;
  return st.win ? `${st.win}:${st.button}` : st.button;
}

/** the key of an open menu the running step picks now (a menu's or a choice's answer), or null */
export function litAskKey(p: PlayerState): string | null {
  const st = p.running >= 0 ? p.steps[p.running] : undefined;
  if (!st || !p.press || p.press.what !== 'key' || p.press.index < 1) return null;
  return st.keys?.[p.press.index] ?? null;
}

/** the answer the running step gives now, as the text a dialog's fields show, or null */
export function answerFill(p: PlayerState): string[] | null {
  const st = p.running >= 0 ? p.steps[p.running] : undefined;
  if (!st || !p.press || p.press.what !== 'answer') return null;
  return answerTexts(st.answers?.[p.press.index]);
}

/** a recorded answer as field texts: a value, a form's values, an object's members (a file's name) */
export function answerTexts(v: unknown): string[] {
  if (v === null || v === undefined) return ['(cancelled)'];
  if (Array.isArray(v)) return v.map(x => String(x));
  if (typeof v === 'object') return Object.values(v as Record<string, unknown>).map(x => String(x));
  return [String(v)];
}

/** the progress bar: one segment per step, a view step's half as wide */
export function segments(p: PlayerState, next: number): {view: boolean; done: boolean; now: boolean; title: string}[] {
  return p.steps.map((s, i) => ({
    view: !!s.view,
    done: i < next && i !== p.running,
    now: i === p.running,
    title: `${i + 1}: ${s.step}`,
  }));
}
