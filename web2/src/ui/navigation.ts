import type {CommandRow} from '../protocol/kinds';
import type {HelloEvent, KeymapInfo} from '../protocol/types';
import {inDesktopWindow} from '../desktop';
import {choiceLabel} from '../store/fieldKinds';
import {pinRows} from '../store/keymap';

/** why command `id` cannot run in this session now, from the state event's list (state.unavailable: the core's own words), null when it can */
export function unavailableReason(unavailable: {cmd: string; reason: string}[] | undefined, id: string): string | null {
  return unavailable?.find(u => u.cmd === id)?.reason ?? null;
}
export function registryRows(hello: HelloEvent | null): CommandRow[] {
  /* the window leaves by its close box: Quit is the browser's command (W647) */
  const desktop = inDesktopWindow();
  return (hello?.commands ?? []).filter(row => row.arguments !== undefined && !(desktop && row.cmd === 'session.quit')).map(row => ({
    ...row, id: row.cmd, label: row.label!, name: row.name!,
    binding: row.cmd, description: row.description ?? '', category: row.category ?? '', pinnable: true,
    default_keys: row.default_keys ?? [],
  }));
}
/** why a row that needs `feature` is not for this model ("needs a model with delays"), null when it is, or when it needs none */
export function featureLack(hello: HelloEvent | null, feature: string | undefined): string | null {
  const found = feature ? hello?.model_features.find(f => f.name === feature) : undefined;
  return found && !found.present ? found.needs : null;
}
/** whether a row goes to the file picker at once, with no form in front (maintainer, 2026-11-25: every open, save and load): each argument
    besides the file is settled without asking, by what the call gives (`given`: a menu item's choice, a fixed argument), by being
    optional (the value in force: the active window, an option, the core's default), or by the file's extension (`from_extension`, the
    core takes it from the name picked); the core asks the file when none is given. A computation that reads a file as one of its inputs
    (a range run, a fit) and a row with a panel of its own keep their form. */
export function fileOnly(row: CommandRow, given: Record<string, unknown> = {}): boolean {
  const args = row.arguments ?? [];
  const file = args.find(a => a.type === 'file' || a.type === 'path');
  if (!file || row.panel || row.kind === 'x') return false;
  return args.every(a => a === file || !a.required || a.from_extension || a.name in given);
}
/** the registry row of command `id`: its label, description and menu place are what every control for it shows */
export function commandRow(hello: HelloEvent | null, id: string): CommandRow | undefined {
  return registryRows(hello).find(row => row.id === id);
}
/** the row of a toolbar button: its label and description are the button's text and tooltip */
export function toolbarRow(hello: HelloEvent | null, window: string, id: string): CommandRow | undefined {
  return commandRow(hello, toolbarCommand(window, id));
}
/** the values of the row's menu_choice argument */
const choiceValues = (row: CommandRow): string[] => row.arguments?.find(a => a.name === row.menu_choice)?.choices ?? [];
/** a row with a menu_choice as one item per value (File > Export > Model as > Python): the row with `choice` set (the command's
    arguments: the value, the row's fixed argument or `fixed`, which a place that knows more gives, as the table's Export menu its
    target, and file_picker for a command whose file is required, which the core then asks for; one with an optional file asks itself) and its name ending in the value's label. The one builder of such items: the menu bar, Search commands and the
    table's and the plot's Export menus all send what it makes. */
export function choiceItems(row: CommandRow, separator: string, fixed = row.menu_fixed): CommandRow[] {
  return choiceValues(row).map(value => {
    const args = {...(fixed && {[fixed.argument]: fixed.value}), [row.menu_choice!]: value};
    /* an item has no keys of its own: the row's are the bare command's */
    return {...row, choice: {value, arguments: {...args, ...(row.arguments?.some(a => a.type === 'file' && a.required) && {file_picker: true})}},
      binding: commandValue(row, args), default_keys: [], name: `${row.name}${separator}${choiceLabel(value)}`};
  });
}
/** the command value of `row` run with `args` (core read_value: the name, then `--argument value` in the row's own order, a choice bare);
    what an item is bound and pinned by */
export function commandValue(row: CommandRow, args: Record<string, string | boolean>): string {
  return row.id + (row.arguments ?? []).filter(a => a.name in args)
    .map(a => ` --${a.name} ${a.type === 'choice' || typeof args[a.name] === 'boolean' ? args[a.name] : JSON.stringify(args[a.name])}`).join('');
}
/** every row a key or a pin may name: the commands, each followed by its menu_choice items, then the command values of the keymap
    that no row lists (a binding or a pin the user wrote with arguments of their own), as rows of the command they run */
export function bindableRows(hello: HelloEvent | null, info: KeymapInfo | null): CommandRow[] {
  const rows = registryRows(hello).flatMap(row => row.menu_choice ? [row, ...choiceItems(row, hello!.menu_separator)] : [row]);
  const known = new Set(rows.map(row => row.binding));
  const extra = (info?.values ?? []).filter(({value}) => !known.has(value)).flatMap(({value, run: {cmd, ...args}}) => {
    const base = rows.find(row => row.id === cmd && row.binding === cmd);
    return base ? [{...base, binding: value, default_keys: [], choice: {value: value.slice(cmd.length + 1), arguments: args}, name: `${base.name} (${value.slice(cmd.length + 1)})`}] : [];
  });
  return [...rows, ...extra, ...pinRows(info)];
}
/** the items of a menu_choice row as a place shows them (the array plot's, the kinescope's, the table's and the plot's Export menus), with
    `fixed` when the place knows more than the row's own fixed argument: one list from choiceItems, none without a hello */
export function rowChoiceItems(hello: HelloEvent | null, row: CommandRow | undefined, fixed?: {argument: string; value: string}): CommandRow[] {
  return row && hello ? choiceItems(row, hello.menu_separator, fixed) : [];
}
/** data.export's items for the data a place writes: the row's own target (the table), or `target` when the place knows another (the plot's
    curves, the table's output columns): one list for File > Export > Data as, the table's and the plot's Export menus */
export function dataExportItems(hello: HelloEvent | null, target?: string): CommandRow[] {
  return rowChoiceItems(hello, commandRow(hello, 'data.export'), target ? {argument: 'target', value: target} : undefined);
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
    if (row.menu_choice) children.push({name: row.label, children: choiceItems(row, hello!.menu_separator).map(item => ({name: choiceLabel(item.choice!.value), children: [], row: item}))});
    else children.push({name: row.label, children: [], row});
  }
  return tree;
}
export function paletteRows(rows: CommandRow[], query: string, separator: string): CommandRow[] {
  const words = query.toLowerCase().trim().split(/\s+/);
  return rows.flatMap(row => row.menu_choice ? choiceItems(row, separator) : [row]).filter(row => words.every(word => `${row.id} ${row.name} ${row.description}`.toLowerCase().includes(word)));
}

/** the registry command of each toolbar button; a button's text is the row's label, never written again */
export function toolbarCommand(window: string, id: string): string {
  const commands: Record<string, Record<string, string>> = {
    autox: {axes: 'autox.configure_axes', numerics: 'autox.configure_numerics', run: 'autox.run', grab: 'autox.grab', usr: 'autox.configure_marks', clear: 'autox.hide_earlier', redraw: 'autox.redraw'},
    data: {find: 'data.find', replace: 'data.replace_column', unreplace: 'data.undo_replace', table: 'data.tabulate_column', load: 'data.load', first: 'data.set_slice_start', last: 'data.set_slice_end', restore: 'data.restore_full_slice', addcol: 'data.add_column', delcol: 'data.delete_column', transpose: 'data.transpose'},
    ani: {file: 'animation.load', grab: 'animation.grab_point'},
    aplot: {redraw: 'array_plot.redraw', edit: 'array_plot.configure', fit: 'array_plot.fit', range: 'array_plot.range', export: 'array_plot.export'},
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
