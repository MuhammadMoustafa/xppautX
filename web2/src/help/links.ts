/* Where a menu or dialog's "?" link sends you: chapter and anchor pairs
   matching docs/manual/README.md's "Menus and dialogs -> sections" table
   (W12b step 3). Kept as one small table, by hand, the same way that table
   itself is kept by hand: extend both together when a dialog gets a
   section it did not have before. */
import type {AppState} from '../store/state';

export interface HelpTarget {
  chapter: string;
  anchor?: string;
}

const U = '04-using-the-interface';

export const HELP = {
  page: {chapter: U, anchor: 'the-page-layout'} as HelpTarget,
  valuesPanel: {chapter: U, anchor: 'the-values-panel'} as HelpTarget,
  plotsAndAxes: {chapter: U, anchor: 'plots-and-axes'} as HelpTarget,
  files: {chapter: U, anchor: 'saving-pictures-and-files'} as HelpTarget,
  log: {chapter: U, anchor: 'the-log'} as HelpTarget,
  formulas: {chapter: U, anchor: 'formulas-as-values'} as HelpTarget,
  dataTab: {chapter: '07-data-browser'} as HelpTarget,
  mainMenu: {chapter: '05-commands'} as HelpTarget,
  fileMenu: {chapter: '05-commands', anchor: 'file'} as HelpTarget,
  numericsMenu: {chapter: '06-numerical-parameters'} as HelpTarget,
  animation: {chapter: '10-animations', anchor: 'the-animation-view'} as HelpTarget,
  autoView: {chapter: '09-auto', anchor: 'the-auto-view'} as HelpTarget,
  autoAxes: {chapter: '09-auto', anchor: 'diagram-axes'} as HelpTarget,
  autoNumerics: {chapter: '09-auto', anchor: 'numerical-parameters'} as HelpTarget,
  autoPars: {chapter: '09-auto', anchor: 'choosing-parameters'} as HelpTarget,
  autoMarks: {chapter: '09-auto', anchor: 'user-functions'} as HelpTarget,
  autoSaving: {chapter: '09-auto', anchor: 'saving-diagrams'} as HelpTarget,
} as const;

/** the current main-menu context (MenuPanel.tsx's WHICH): main, file or num commands */
export function menuHelp(which: number): HelpTarget {
  return which === 1 ? HELP.fileMenu : which === 2 ? HELP.numericsMenu : HELP.mainMenu;
}

/** a core prompt (AskDialog.tsx): the Data tab's own ops, the Numerics
    submenu's fields, a file ask, or (the common case) a main-menu command;
    README's table groups all of these by area, not by each dialog's exact
    title, so this does the same instead of guessing a per-field anchor */
export function askHelp(state: Pick<AppState, 'table' | 'core'>, kind: string): HelpTarget {
  if (kind === 'file') return HELP.files;
  if (state.table.open) return HELP.dataTab;
  return menuHelp(state.core?.menu ?? 0);
}

/* a manual cross-reference's href, as marked (tools/manualBuild.mjs) leaves
   it: "06-numerical-parameters.md", "...md#poincare-map", or a bare
   "#anchor" within the chapter open when it is clicked. Anything else
   (mailto:, http:, a doc outside docs/manual) is not one of ours: Help.tsx
   lets the browser handle it as an ordinary link. */
const LINK_RE = /^(\d\d-[a-z0-9-]+)\.md(?:#([\w-]+))?$|^#([\w-]+)$/i;

export function manualLinkTarget(href: string, chapter: string): HelpTarget | null {
  const m = LINK_RE.exec(href);
  if (!m) return null;
  return m[3] !== undefined ? {chapter, anchor: m[3]} : {chapter: m[1], anchor: m[2]};
}
