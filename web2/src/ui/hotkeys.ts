/* XPP's hotkeys: a key typed while the focus is on the page, the plot or a
   button (not in a text field, a select, a dialog or a menu) is sent as the
   protocol's key name (DOM KeyboardEvent.key), through session.typeKey,
   which lets a key typed before the menu of the one before it arrives
   answer that menu (typing I then G quickly integrates). A button keeps
   its own Enter, Space and arrows: only letters, digits, signs and Escape
   typed on it are hotkeys (docs/ui-v2.md T21: after a click the focus stays on the
   button, and F or G typed then must still act). Tab is never taken: it
   moves the focus. */
import {useEffect, useState} from 'preact/hooks';
import {kindOf, menuCommand} from '../protocol/kinds';
import type {MenuName} from '../protocol/types';
import type {Session} from '../session';
import {BUSY_TITLE} from './context';

/** the work areas F6 cycles, in reading order: the command search, the plot, then the panels. A pane
    that is hidden, or in a drawer that is closed, is skipped (focusPane) */
const PANE_SELECTORS = ['#command-search', '.plot-host', '#values-panel', '#table-panel', '#text-panel'];
/** the command drawer and the class it has while open: on a narrow screen it is off-canvas
    (position fixed, slid out) until then */
const DRAWER = '.menu-panel', DRAWER_OPEN = 'open';
const COMMAND_SEARCH = '#command-search';
/** the workspace Tools menu (TitleBar.tsx) */
export const TOOLS_MENU = '.workspace-tools';

type Chord = {menu: MenuName; item: string} | 'search';
/** The chords the page claims: Ctrl or Cmd with one of these keys, no Alt or Shift. Every other
    chord stays the browser's and the system's (Ctrl/Cmd+W closes the tab or window, Alt+F4 the
    application): it is not listed, so it is never prevented. */
const CHORDS: Record<string, Chord> = {
  o: {menu: 'file', item: 'openmodel'}, s: {menu: 'file', item: 'savesession'}, k: 'search'};

function inReach(pane: HTMLElement): boolean {
  if (pane.getClientRects().length === 0 || getComputedStyle(pane).visibility === 'hidden') return false;
  const drawer = pane.closest<HTMLElement>(DRAWER);
  return !drawer || drawer.classList.contains(DRAWER_OPEN) || getComputedStyle(drawer).position !== 'fixed';
}

/** Visible work areas cycle without walking every field or command. */
function focusPane(backwards: boolean): void {
  const panes = PANE_SELECTORS.map(selector => document.querySelector<HTMLElement>(selector))
    .filter((pane): pane is HTMLElement => !!pane && inReach(pane));
  if (!panes.length) return;
  const current = panes.findIndex(pane => pane === document.activeElement || pane.contains(document.activeElement));
  const next = current < 0 ? backwards ? panes.length - 1 : 0 : (current + (backwards ? -1 : 1) + panes.length) % panes.length;
  const pane = panes[next];
  if (!pane.matches('input, button, [tabindex]')) pane.tabIndex = -1;
  pane.focus();
}

const NAMED = new Set(['Escape', 'Enter', 'Backspace', 'Delete', 'Home', 'End', 'ArrowLeft', 'ArrowRight',
  'ArrowUp', 'ArrowDown', 'PageUp', 'PageDown']);
/** where typing is the element's own: text, lists, dialogs and menus */
const TYPING = 'input, textarea, select, [contenteditable], [role="dialog"], [role="menu"], [role="listbox"]';
/** controls that keep their own named keys (Enter, Space, the arrows) but not letters */
const BUTTONS = 'button, a, summary, [role="button"], [role="tab"], [role="slider"]';
/** a key typed inside these is the dialog's own */
const DIALOGS = '[role="dialog"], [aria-modal="true"]';

export function hotkey(e: KeyboardEvent): string | null {
  if (e.ctrlKey || e.metaKey || e.altKey) return null;
  return e.key.length === 1 || NAMED.has(e.key) ? e.key : null;
}

/** whether a key typed on `target` is XPP's (a printable one on a button is; nothing typed in a field is) */
export function isHotkeyTarget(target: Element | null, key: string): boolean {
  if (!target?.closest) return true;
  if (target.closest(TYPING)) return false;
  if (target.closest(BUTTONS)) return (key.length === 1 && key !== ' ') || key === 'Escape';
  return true;
}

/** The chord a key event is, or null: Ctrl/Cmd with O, S or K and nothing else held. */
export function chordOf(e: KeyboardEvent): Chord | null {
  if (!(e.ctrlKey || e.metaKey) || e.altKey || e.shiftKey) return null;
  return CHORDS[e.key.toLowerCase()] ?? null;
}

/** Labels use capitals: Caps Lock or Shift types a capital where only the lowercase letter is a
    command, so it is sent as that command (a capital that is a command of its own stays). The one
    place this is decided; `known` says whether a key is a command now. */
export function commandKey(k: string, known: (key: string) => boolean): string {
  return k.length === 1 && !known(k) && known(k.toLowerCase()) ? k.toLowerCase() : k;
}

/** Close the Tools menu; `focus` returns the focus to its summary (Escape, a click outside) and not
    after a choice, whose command has its own place for it. */
export function closeTools(details: HTMLDetailsElement, focus: boolean): void {
  details.open = false;
  if (focus) details.querySelector('summary')?.focus();
}

/** Escape closes what the page opened over its work, the innermost first (the Tools menu, then
    the command drawer); true when it closed one. The one place that does, so no component holds a
    document-wide Escape listener that could take the key from the Player or a dialog. */
function closeTransient(session: Session): boolean {
  const tools = document.querySelector<HTMLDetailsElement>(`${TOOLS_MENU}[open]`);
  if (tools) { closeTools(tools, true); return true; }
  const {drawerOpen, ask} = session.store.getState();
  if (!drawerOpen || ask) return false;
  session.store.dispatch({type: 'drawer', open: false});
  return true;
}

/** One key typed on the page. The browser's default is prevented only for a key the page acts on
    (or, for a chord the core cannot take now, explains in the status bar). */
export function handleKey(session: Session, e: KeyboardEvent): void {
  if (e.defaultPrevented) return;
  const target = e.target as Element | null;
  if (target?.closest?.(DIALOGS)) return;
  const chord = chordOf(e);
  if (chord === 'search') {
    e.preventDefault();
    session.store.dispatch({type: 'drawer', open: true});
    document.querySelector<HTMLInputElement>(COMMAND_SEARCH)?.focus();
    return;
  }
  if (chord) {
    const {hello, core} = session.store.getState();
    const command = menuCommand(chord.menu, chord.item);
    /* before hello, or for a command the core lacks, the key stays the browser's */
    if (!hello || kindOf(hello, core?.menu ?? 0, command) === null) return;
    e.preventDefault();
    if (session.may(command)) session.menuAction(chord.menu, chord.item);
    else {
      const label = hello.menus[chord.menu][hello.menus[`${chord.menu}_ids`].indexOf(chord.item)];
      session.store.dispatch({type: 'bottom', text: `${label}: ${BUSY_TITLE}`});
    }
    return;
  }
  if (e.key === 'Escape' && !e.ctrlKey && !e.metaKey && !e.altKey && closeTransient(session)) {
    e.preventDefault();
    return;
  }
  if (e.key === 'F6') {
    e.preventDefault();
    focusPane(e.shiftKey);
    return;
  }
  const k = hotkey(e);
  if (!k || !isHotkeyTarget(target, k)) return;
  e.preventDefault();
  const {hello, core, ask} = session.store.getState();
  session.typeKey(ask ? k : commandKey(k, key => kindOf(hello, core?.menu ?? 0, {cmd: 'key', key}) !== null));
}

export function useHotkeys(session: Session): void {
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => handleKey(session, e);
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [session]);
}

/** Whether Caps Lock is on, as the last key event reported it (the status bar says so, since it
    changes which letters are commands). Keyboard state belongs here with the other key handling. */
export function useCapsLock(): boolean {
  const [caps, setCaps] = useState(false);
  useEffect(() => {
    const update = (e: KeyboardEvent) => setCaps(e.getModifierState('CapsLock'));
    window.addEventListener('keydown', update);
    window.addEventListener('keyup', update);
    return () => { window.removeEventListener('keydown', update); window.removeEventListener('keyup', update); };
  }, []);
  return caps;
}
