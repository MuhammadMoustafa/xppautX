import type {CommandRow} from '../protocol/kinds';
import type {HelloEvent} from '../protocol/types';
import {inDesktopWindow} from '../desktop';
import {choiceLabel} from '../store/fieldKinds';

export function registryRows(hello: HelloEvent | null): CommandRow[] {
  /* the window leaves by its close box: Quit is the browser's command (W647) */
  const desktop = inDesktopWindow();
  return (hello?.commands ?? []).filter(row => row.arguments !== undefined && !(desktop && row.cmd === 'session.quit')).map(row => ({
    ...row, id: row.cmd, label: row.label!, name: row.name!,
    description: row.description ?? '', category: row.category ?? '', pinnable: true,
    default_keys: row.default_keys ?? [],
  }));
}
/** why a row that needs `feature` is not for this model ("needs a model with delays"), null when it is, or when it needs none */
export function featureLack(hello: HelloEvent | null, feature: string | undefined): string | null {
  const found = feature ? hello?.model_features.find(f => f.name === feature) : undefined;
  return found && !found.present ? found.needs : null;
}
/** the registry row of command `id`: its label, description and menu place are what every control for it shows */
export function commandRow(hello: HelloEvent | null, id: string): CommandRow | undefined {
  return registryRows(hello).find(row => row.id === id);
}
/** the row of a toolbar button: its label and description are the button's text and tooltip */
export function toolbarRow(hello: HelloEvent | null, window: string, id: string): CommandRow | undefined {
  return commandRow(hello, toolbarCommand(window, id));
}
/** between the groups of a full name (core MENU_SEPARATOR, command_def.h) */
const MENU_SEPARATOR = ' \u203a ';
/** the values of the row's menu_choice argument */
const choiceValues = (row: CommandRow): string[] => row.arguments?.find(a => a.name === row.menu_choice)?.choices ?? [];
/** a row with a menu_choice as one item per value (File > Export > Model as > Python): the row with `choice` set and its name ending in the value's label */
export function choiceItems(row: CommandRow): CommandRow[] {
  return choiceValues(row).map(value => ({...row, choice: {argument: row.menu_choice!, value}, name: `${row.name}${MENU_SEPARATOR}${choiceLabel(value)}`}));
}
export interface MenuNode {name: string; children: MenuNode[]; row?: CommandRow}
export function menuTree(rows: CommandRow[], hello: HelloEvent | null): MenuNode[] {
  const tree: MenuNode[] = [];
  for (const row of rows) {
    if (!row.menu_path || featureLack(hello, row.needs)) continue;
    let children = tree;
    for (const name of row.menu_path.split('/')) {
      let node = children.find(node => node.name === name && !node.row);
      if (!node) { node = {name, children: []}; children.push(node); }
      children = node.children;
    }
    if (row.menu_choice) children.push({name: row.label, children: choiceItems(row).map(item => ({name: choiceLabel(item.choice!.value), children: [], row: item}))});
    else children.push({name: row.label, children: [], row});
  }
  return tree;
}
export function paletteRows(rows: CommandRow[], query: string): CommandRow[] {
  const words = query.toLowerCase().trim().split(/\s+/);
  return rows.flatMap(row => row.menu_choice ? choiceItems(row) : [row]).filter(row => words.every(word => `${row.id} ${row.name} ${row.description}`.toLowerCase().includes(word)));
}

/** the registry command of each toolbar button; a button's text is the row's label, never written again */
export function toolbarCommand(window: string, id: string): string {
  const commands: Record<string, Record<string, string>> = {
    autox: {axes: 'autox.configure_axes', numerics: 'autox.configure_numerics', run: 'autox.run', grab: 'autox.grab', usr: 'autox.configure_marks', clear: 'autox.hide_earlier', redraw: 'autox.redraw'},
    data: {find: 'data.find', replace: 'data.replace_column', unreplace: 'data.undo_replace', table: 'data.tabulate_column', load: 'data.load', first: 'data.set_slice_start', last: 'data.set_slice_end', restore: 'data.restore_full_slice', addcol: 'data.add_column', delcol: 'data.delete_column', transpose: 'data.transpose'},
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
