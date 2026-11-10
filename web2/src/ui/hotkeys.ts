import {registryRows} from './navigation';
/* The page's keyboard layer (docs/command-design.md): one dispatcher from a key event to a command id.
   A key is normalised to a name ("Ctrl+Shift+S"), looked up among the commands' effective keys (the
   registry's defaults, or the user's), and the command runs through Session.registryAction, the path a
   menu click takes. A binding is one key: there are no chords. A key typed in a text field, a select, a dialog or a menu is the
   element's own; a button keeps its Enter, Space and arrows. Tab is never taken: it moves the focus. */
import {useEffect} from 'preact/hooks';
import {noValueEdit} from '../protocol/kinds';
import type {CommandRow} from '../protocol/kinds';
import type {HelloEvent} from '../protocol/types';
import {effectiveKeys} from '../store/keymap';
import type {KeymapInfo} from '../protocol/types';
import type {Session} from '../session';
import {BUSY_TITLE} from './context';

/** the work areas F6 cycles, in reading order: the command search, the plot, then the panels. A pane
    that is hidden, or in a drawer that is closed, is skipped (focusPane) */
const PANE_SELECTORS = [ '.plot-host', '#values-panel', '#table-panel', '#text-panel'];
/** the pop-ups Escape closes after the menu bar's: the plot's ⋯ (PlotView.tsx) and the status bar's Messages (Messages.tsx) */
const POPUPS = ':is(.plot-more, .status-bar .messages)';

/** Pane navigation is presentation, rather than a command binding. */
export const FIXED_KEYS: readonly {key: string; does: string}[] = [
  {key: 'F6 / Shift+F6', does: 'Next / previous pane'}];

function inReach(pane: HTMLElement): boolean {
  if (pane.getClientRects().length === 0 || getComputedStyle(pane).visibility === 'hidden') return false;
  return true;
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
/** a key the page takes wherever the focus is: with Ctrl or Alt, or a function key */
const ANYWHERE = /^(Ctrl|Alt)\+|^F\d+$/;
/** where Ctrl+Z is the browser's text undo */
const TEXT_FIELD = 'input:not([type="checkbox"]):not([type="radio"]):not([type="range"]):not([type="button"]), textarea, [contenteditable]';
/** the commands whose keys are a text field's own while it has the focus */
const EDIT_COMMANDS = new Set(['value.undo', 'value.redo']);
const MODIFIER_KEYS = new Set(['Control', 'Shift', 'Alt', 'Meta', 'AltGraph', 'CapsLock', 'Dead']);

/** a key an open menu or prompt can take: a single sign, or a named key without a modifier */
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

/** The name of a key event, written as the table writes keys ("Ctrl+Shift+S"): Ctrl (Cmd on macOS), Alt,
    Shift (a letter's or a named key's: a sign carries its own shift), then the key, a letter in capitals,
    taken from the physical key under Ctrl or Alt, which the system may compose into another character.
    Null for a modifier alone: it neither runs nor cancels anything. */
export function keyName(e: KeyboardEvent): string | null {
  if (MODIFIER_KEYS.has(e.key)) return null;
  const physical = /^Key([A-Z])$/.exec(e.code ?? '')?.[1];
  const base = (e.ctrlKey || e.metaKey || e.altKey) && physical ? physical
    : e.key.length === 1 ? (e.key === ' ' ? 'Space' : e.key.toUpperCase()) : e.key;
  const shift = e.shiftKey && (base.length > 1 || /[A-Z]/.test(base));
  return [e.ctrlKey || e.metaKey ? 'Ctrl' : '', e.altKey ? 'Alt' : '', shift ? 'Shift' : '', base].filter(Boolean).join('+');
}

/** the command whose effective keys (the table's, or the user's) include the key `name`, else null */
export function commandOnKey(hello: HelloEvent | null, keymap: KeymapInfo | null, name: string): CommandRow | null {
  return registryRows(hello).find(row => effectiveKeys(keymap, row).includes(name)) ?? null;
}

/** how a command's keys are written where the page lists it: its effective keys */
export function shortcutLabel(row: CommandRow, keymap: KeymapInfo | null): string {
  return effectiveKeys(keymap, row).join(' / ');
}

/** Close a pop-up `details`; `focus` returns the focus to its summary (Escape) and not after a choice. */
export function closePopup(details: HTMLDetailsElement, focus: boolean): void {
  details.open = false;
  if (focus) details.querySelector('summary')?.focus();
}

/** Close the menu bar's open menus (the one place that does): `focus` returns the focus to the title of the
    menu the focus was in (Escape, an entry's choice); a click outside leaves it where it is. True when a menu was open. */
export function closeMenuBar(focus: boolean): boolean {
  const bar = document.querySelector<HTMLElement>('.app-menu');
  const open = bar?.querySelectorAll('details[open]');
  if (!bar || !open?.length) return false;
  const title = document.activeElement?.closest('[data-top-menu]')?.querySelector<HTMLElement>(':scope > summary');
  open.forEach(d => d.removeAttribute('open'));
  if (focus) title?.focus();
  return true;
}

/** the open panels, the last opened on top: Escape closes the topmost (escape) */
const panels: {close: () => void}[] = [];

/** A panel that is open registers how it closes; the returned function removes it again (an effect's cleanup). */
export function openPanel(close: () => void): () => void {
  const entry = {close};
  panels.push(entry);
  return () => {
    const at = panels.indexOf(entry);
    if (at >= 0) panels.splice(at, 1);
  };
}

/** the questions a pick or a grab asks: Escape cancels them (PickBar, GrabBar's cancelPick) */
const PICK_ASKS = new Set(['mouse', 'rubber', 'drag', 'grab']);

/** Escape, the one owner of the key (W633). A press does one thing, the innermost first: an open menu (the
    menu bar, then the ⋯ and Messages pop-ups), the command drawer, a pick or a grab, a question the page asks (answered as
    cancelled; a dialog keeps its own Escape and never reaches here), the topmost open panel, and last the stop of a
    running command. A panel's field that holds an edit to drop keeps its Escape (data-own-escape). True when taken. */
function escape(session: Session, target: Element | null): boolean {
  const {busy, ask, pick, drawerOpen} = session.store.getState();
  if (closeMenuBar(true)) return true;
  const popup = document.querySelector<HTMLDetailsElement>(`${POPUPS}[open]`);
  if (popup) { closePopup(popup, true); return true; }
  if (drawerOpen && !ask) { session.store.dispatch({type: 'drawer', open: false}); return true; }
  if (pick || (ask && PICK_ASKS.has(ask.kind))) { session.cancelPick(); return true; }
  if (ask) {
    if (!isHotkeyTarget(target, 'Escape')) return false;
    session.typeKey('Escape');
    return true;
  }
  const top = panels[panels.length - 1];
  if (top && !target?.closest?.('[data-own-escape]')) { top.close(); return true; }
  if (busy) { session.abort(); return true; }
  return false;
}

/** Run the command a key names, through the action a click on it takes; while a run forbids
    it (its kind, protocol/kinds.ts) the status bar says why. */
function runCommand(session: Session, row: CommandRow): void {
  if (session.may({cmd: row.id})) session.registryAction(row.id);
  else session.store.dispatch({type: 'bottom', text: `${row.name}: ${noValueEdit(session.store.getState().core, row.id) ? `nothing to ${row.id}` : BUSY_TITLE}`});
}

/** One key typed on the page. The browser's default is prevented only for a key the page acts on: a
    command's key. */
export function handleKey(session: Session, e: KeyboardEvent): void {
  if (e.defaultPrevented) return;
  const target = e.target as Element | null;
  if (target?.closest?.(DIALOGS)) return;
  const name = keyName(e);
  if (!name) return;
  const {hello, keymap, ask} = session.store.getState();
  /* the keys the system and the browser keep (the core's list: hello.keymap.reserved) are never taken */
  if (keymap.info?.reserved.includes(name)) return;
  if (name === 'Escape' && escape(session, target)) {
    e.preventDefault();
    return;
  }
  if (name === 'F6' || name === 'Shift+F6') {
    e.preventDefault();
    focusPane(e.shiftKey);
    return;
  }
  /* an open prompt, or one on its way, takes the key */
  const typed = hotkey(e);
  if (ask) {
    if (typed && isHotkeyTarget(target, typed)) {
      e.preventDefault();
      session.typeKey(typed);
    }
    return;
  }
  if (!ANYWHERE.test(name) && !isHotkeyTarget(target, e.key)) return;
  const row = commandOnKey(hello, keymap.info, name);
  if (!row) return;
  /* in a text field Undo and Redo are the field's own (its typing), not the Values' */
  if (EDIT_COMMANDS.has(row.id) && target?.closest?.(TEXT_FIELD)) return;
  e.preventDefault();
  runCommand(session, row);
}

export function useHotkeys(session: Session): void {
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => handleKey(session, e);
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [session]);
}
