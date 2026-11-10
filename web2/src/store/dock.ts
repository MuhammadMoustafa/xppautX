/* The right dock (docs/ui-v2.md R6, T26; W644): Values, Table, Model and Help are its tabs, one open at a time.
   Table, Model and Help keep their own open flags (what each shows and asks the core for follows them); the tab
   shown is derived from them, and a rule on every state change keeps at most one open, the one opened last, and
   shows the dock when one opens. Pure: no DOM, no I/O. */

export type DockTab = 'values' | 'table' | 'text' | 'help';

/** the tabs in the order the dock shows them, with the words on them */
export const DOCK_TABS: readonly {id: DockTab; label: string}[] = [
  {id: 'values', label: 'Values'}, {id: 'table', label: 'Table'}, {id: 'text', label: 'Model'}, {id: 'help', label: 'Help'},
];

interface DockHolder {
  table: {open: boolean};
  text: {open: boolean};
  help: {open: boolean};
  panels: {dockHidden: boolean};
}

const PANELS = ['table', 'text', 'help'] as const;

/** the tab shown: the panel that is open, else Values */
export function dockTab(s: DockHolder): DockTab {
  return PANELS.find(k => s[k].open) ?? 'values';
}

/** whether the dock column is shown: not hidden by the user, and beside the autoX view only for a panel, not for Values */
export function dockIsShown(hidden: boolean, autoxShown: boolean, tab: DockTab): boolean {
  return !hidden && !(autoxShown && tab === 'values');
}

/** `next` as the dock keeps it after a change from `prev`: the panel that just opened is the only one open, and the dock is shown */
export function withDock<S extends DockHolder>(prev: S, next: S): S {
  const opened = PANELS.filter(k => next[k].open && !prev[k].open);
  if (!opened.length) return next;
  const keep = opened[opened.length - 1];
  const out = {...next, panels: {...next.panels, dockHidden: false}};
  for (const k of PANELS) if (k !== keep && out[k].open) out[k] = {...out[k], open: false};
  return out;
}
