/* The page's keyboard layer (docs/command-design.md): one dispatcher from a key event to a command id.
   A key is normalised to a name ("Ctrl+Shift+S"), looked up among the command table's default keys (and,
   when the preset "XPPAUT sequences" is on, its legacy sequences: F then S is a chord over the same
   dispatcher), and the command runs through Session.menuAction, the path a click on it takes. A key typed
   while a menu is open or on its way answers it (session.typeKey: typing I then G quickly integrates).
   A chord waits for its next key in the store's pendingKeys, shown in the status bar, until Esc or a
   change of focus: never for a time. A key typed in a text field, a select, a dialog or a menu is the
   element's own; a button keeps its Enter, Space and arrows. Tab is never taken: it moves the focus. */
import {useEffect} from 'preact/hooks';
import {menuCommand, noValueEdit} from '../protocol/kinds';
import type {CommandRow} from '../protocol/kinds';
import type {HelloEvent} from '../protocol/types';
import type {KeyPreset} from '../store/state';
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

/** Keys the page never binds and never prevents: the system's and the browser's own (Alt+F4 closes the
    application, Ctrl+W the tab or window, Ctrl+Q quits, F11 is full screen, F12 the developer tools, F5
    reloads the page). A table row with one of them is a problem navigationProblems shows. Ctrl+R is not
    here: it is Reload model's default key (docs/command-design.md), taken from the browser in browser mode. */
export const RESERVED_KEYS: readonly string[] = ['Alt+F4', 'Ctrl+W', 'Ctrl+Q', 'F11', 'F12', 'F5'];
/** the command search, which is no command: the way back to every command (docs/command-design.md: fixed) */
const SEARCH_KEY = 'Ctrl+K';
/** the keys the page takes that are no table command, listed by the Help key table */
export const FIXED_KEYS: readonly {key: string; does: string}[] = [
  {key: SEARCH_KEY, does: 'Search commands'}, {key: 'F6 / Shift+F6', does: 'Next / previous pane'},
  {key: 'Esc', does: 'Stop the running command, cancel a pending key or a prompt, close a menu'}, {key: 'F1', does: 'Help'}];

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
/** a key the page takes wherever the focus is: with Ctrl or Alt, or a function key */
const ANYWHERE = /^(Ctrl|Alt)\+|^F\d+$/;
/** where Ctrl+Z is the browser's text undo */
const TEXT_FIELD = 'input:not([type="checkbox"]):not([type="radio"]):not([type="range"]):not([type="button"]), textarea, [contenteditable]';
/** the commands whose keys are a text field's own while it has the focus */
const EDIT_COMMANDS = new Set(['undo', 'redo']);
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
    : e.key.length === 1 ? (e.key === ' ' ? 'Space' : e.key.toUpperCase()) : e.key === 'Escape' ? 'Esc' : e.key;
  const shift = e.shiftKey && (base.length > 1 || /[A-Z]/.test(base));
  return [e.ctrlKey || e.metaKey ? 'Ctrl' : '', e.altKey ? 'Alt' : '', shift ? 'Shift' : '', base].filter(Boolean).join('+');
}

/** a key sequence that runs a command: its default keys (each a chord, or several separated by spaces,
    "Ctrl+K Ctrl+S"), and with the XPPAUT preset its legacy sequence */
interface Binding {keys: string[]; row: CommandRow}

/** every binding of the table under `preset`; the three switches of the legacy shortcut layers (category
    layer) have none: the page has no layer to enter */
function bindingsOf(hello: HelloEvent | null, preset: KeyPreset): Binding[] {
  return (hello?.command_table ?? []).flatMap(row => [
    ...row.default_keys.map(chord => ({keys: chord.split(' '), row})),
    ...preset === 'xppaut' && row.category !== 'layer' ? [{keys: row.legacy_keys, row}] : []]);
}

/** what the keys typed so far are among the bindings: the command they complete, else whether some binding
    goes on with them (a chord not yet finished) */
export function resolveKeys(hello: HelloEvent | null, preset: KeyPreset, keys: string[]): {row: CommandRow | null; more: boolean} {
  const bindings = bindingsOf(hello, preset);
  const starts = (b: Binding) => keys.every((k, i) => b.keys[i] === k);
  const done = bindings.find(b => b.keys.length === keys.length && starts(b));
  return {row: done?.row ?? null, more: !done && bindings.some(b => b.keys.length > keys.length && starts(b))};
}

/** how a command's keys are written where the page lists it: its default keys, else, when that preset is on,
    its XPPAUT sequence */
export function shortcutLabel(row: CommandRow, preset: KeyPreset): string {
  return row.default_keys.length ? row.default_keys.join(' / ') : preset === 'xppaut' ? row.legacy_keys.join(' ') : '';
}

/** a plain letter's name without Shift ("Shift+I" is I: the sequences are letters) */
const plainName = (name: string): string => name.replace(/^Shift\+(?=[A-Z]$)/, '');

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

/** Run the command a key sequence completed, through the action a click on it takes; while a run forbids
    it (its kind, protocol/kinds.ts) the status bar says why. */
function runCommand(session: Session, row: CommandRow): void {
  if (session.may(menuCommand(row.menu, row.id))) session.menuAction(row.menu, row.id);
  else session.store.dispatch({type: 'bottom', text: `${row.label}: ${noValueEdit(session.store.getState().core, row.id) ? `nothing to ${row.id}` : BUSY_TITLE}`});
}

/** One key typed on the page. The browser's default is prevented only for a key the page acts on: a
    command's key, a chord begun or cancelled, or a key that completes none (the status bar says so). */
export function handleKey(session: Session, e: KeyboardEvent): void {
  if (e.defaultPrevented) return;
  const target = e.target as Element | null;
  if (target?.closest?.(DIALOGS)) return;
  const name = keyName(e);
  if (!name || RESERVED_KEYS.includes(name)) return;
  const {hello, keyPreset, pendingKeys, busy, ask} = session.store.getState();
  const dispatch = session.store.dispatch;
  if (name === 'Esc' && pendingKeys.length) {
    e.preventDefault();
    dispatch({type: 'pendingKeys', keys: []});
    return;
  }
  if (name === SEARCH_KEY) {
    e.preventDefault();
    dispatch({type: 'drawer', open: true});
    document.querySelector<HTMLInputElement>(COMMAND_SEARCH)?.focus();
    return;
  }
  if (name === 'Esc' && closeTransient(session)) {
    e.preventDefault();
    return;
  }
  if (name === 'F6' || name === 'Shift+F6') {
    e.preventDefault();
    focusPane(e.shiftKey);
    return;
  }
  /* an open menu or prompt, or one on its way, takes the key; Escape stops a running command */
  const typed = hotkey(e);
  if (ask || session.awaitingMenu() || (name === 'Esc' && busy)) {
    if (typed && isHotkeyTarget(target, typed)) {
      e.preventDefault();
      session.typeKey(typed);
    }
    return;
  }
  if (!ANYWHERE.test(name) && !isHotkeyTarget(target, e.key)) return;
  const keys = [...pendingKeys, plainName(name)];
  const {row, more} = resolveKeys(hello, keyPreset, keys);
  /* in a text field Undo and Redo are the field's own (its typing), not the Values' */
  if (row && EDIT_COMMANDS.has(row.id) && target?.closest?.(TEXT_FIELD)) return;
  if (!row && !more && !pendingKeys.length) return;
  e.preventDefault();
  dispatch({type: 'pendingKeys', keys: more ? keys : []});
  if (row) runCommand(session, row);
  else if (!more) dispatch({type: 'bottom', text: `${keys.join(' ')} is not a command`});
}

export function useHotkeys(session: Session): void {
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => handleKey(session, e);
    /* a chord waits for its next key only while the focus stays: its moving, or the window's losing it, cancels */
    const cancel = () => session.store.dispatch({type: 'pendingKeys', keys: []});
    window.addEventListener('keydown', onKey);
    window.addEventListener('blur', cancel);
    document.addEventListener('focusin', cancel);
    return () => {
      window.removeEventListener('keydown', onKey);
      window.removeEventListener('blur', cancel);
      document.removeEventListener('focusin', cancel);
    };
  }, [session]);
}
