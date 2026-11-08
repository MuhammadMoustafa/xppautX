import type {CommandRow} from '../protocol/kinds';
import type {HelloEvent} from '../protocol/types';
import {inDesktopWindow} from '../desktop';

export function registryRows(hello: HelloEvent | null): CommandRow[] {
  /* the window leaves by its close box: Quit is the browser's command (W647) */
  const desktop = inDesktopWindow();
  return (hello?.commands ?? []).filter(row => row.arguments !== undefined && !(desktop && row.cmd === 'session.quit')).map(row => ({
    ...row, id: row.cmd, label: row.label!,
    description: row.description ?? '', category: row.category ?? '', pinnable: true,
    default_keys: row.default_keys ?? [],
  }));
}
/** the registry row of command `id`: its label, description and menu place are what every control for it shows */
export function commandRow(hello: HelloEvent | null, id: string): CommandRow | undefined {
  return registryRows(hello).find(row => row.id === id);
}
/** where a row sits in the menu bar, label last (`File/Sets/Apply set`) */
export function menuPlace(row: CommandRow): string {
  return [row.menu_path, row.label].filter(Boolean).join('/');
}
/** the row of a toolbar button: its label and description are the button's text and tooltip */
export function toolbarRow(hello: HelloEvent | null, window: string, id: string): CommandRow | undefined {
  return commandRow(hello, toolbarCommand(window, id));
}
export interface MenuNode {name: string; children: MenuNode[]; row?: CommandRow}
export function menuTree(rows: CommandRow[]): MenuNode[] {
  const tree: MenuNode[] = [];
  for (const row of rows) {
    if (!row.menu_path) continue;
    let children = tree;
    for (const name of row.menu_path.split('/')) {
      let node = children.find(node => node.name === name && !node.row);
      if (!node) { node = {name, children: []}; children.push(node); }
      children = node.children;
    }
    children.push({name: row.label, children: [], row});
  }
  return tree;
}
export function paletteRows(rows: CommandRow[], query: string): CommandRow[] {
  const words = query.toLowerCase().trim().split(/\s+/);
  return rows.filter(row => words.every(word => `${row.id} ${menuPlace(row)} ${row.description}`.toLowerCase().includes(word)));
}

/** the registry command of each toolbar button; a button's text is the row's label, never written again */
export function toolbarCommand(window: string, id: string): string {
  const commands: Record<string, Record<string, string>> = {
    autox: {axes: 'autox.configure_plot', numerics: 'autox.configure_numerics', run: 'autox.run', grab: 'autox.grab', usr: 'autox.configure_marks', clear: 'autox.hide_earlier', redraw: 'autox.redraw'},
    browser: {find: 'browser.find', replace: 'browser.replace_column', unreplace: 'browser.undo_replace', table: 'browser.tabulate_column', load: 'browser.load', first: 'browser.set_slice_start', last: 'browser.set_slice_end', restore: 'browser.restore_full_slice', addcol: 'browser.add_column', delcol: 'browser.delete_column', transpose: 'browser.transpose'},
    ani: {file: 'animation.load', grab: 'animation.grab_point'},
    aplot: {redraw: 'array_plot.redraw', edit: 'array_plot.configure', fit: 'array_plot.fit', range: 'array_plot.range', print: 'array_plot.export_pdf', gif: 'array_plot.export_gif'},
  };
  return commands[window]?.[id] ?? '';
}

/** where a command's `panel` points (core Command::panel): the option group, the sections of it the command uses (none listed: all of it), and whether the
    command's own arguments that no row sets stay in its form beside the rows ("plot+") */
export function panelSpec(panel: string | undefined): {group: string; sections: string[] | null; own: boolean} | null {
  if (!panel) return null;
  const own = panel.endsWith('+'), [group, sections] = panel.replace(/\+$/, '').split(':');
  return {group, sections: sections ? sections.split(',') : null, own};
}
