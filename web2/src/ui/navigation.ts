import type {CommandRow} from '../protocol/kinds';
import type {HelloEvent} from '../protocol/types';

export function registryRows(hello: HelloEvent | null): CommandRow[] {
  return (hello?.commands ?? []).filter(row => row.arguments !== undefined).map(row => ({
    ...row, id: row.cmd, label: row.menu_path?.split('/').pop() || row.cmd,
    description: row.description ?? '', category: row.category ?? '', pinnable: true,
    default_keys: row.default_keys ?? [],
  }));
}
export interface MenuNode {name: string; children: MenuNode[]; row?: CommandRow}
export function menuTree(rows: CommandRow[]): MenuNode[] {
  const tree: MenuNode[] = [];
  for (const row of rows) {
    let children = tree;
    const path = row.menu_path?.split('/') ?? [];
    for (const [index, name] of path.entries()) {
      let node = children.find(node => node.name === name && !node.row);
      if (index === path.length - 1) { children.push({name, children: [], row}); break; }
      if (!node) { node = {name, children: []}; children.push(node); }
      children = node.children;
    }
  }
  return tree;
}
export function paletteRows(rows: CommandRow[], query: string): CommandRow[] {
  const words = query.toLowerCase().trim().split(/\s+/);
  return rows.filter(row => words.every(word => `${row.id} ${row.menu_path} ${row.description}`.toLowerCase().includes(word)));
}

/** Toolbar buttons use the registry path too; these are presentation shortcuts, not a menu tree. */
export function toolbarCommand(window: string, id: string): string {
  const commands: Record<string, Record<string, string>> = {
    auto: {axes: 'auto.configure_plot', numerics: 'auto.configure_numerics', run: 'auto.run', grab: 'auto.grab', usr: 'auto.configure_marks', clear: 'auto.hide_earlier', redraw: 'auto.redraw'},
    browser: {find: 'browser.find', replace: 'browser.replace_column', unreplace: 'browser.undo_replace', table: 'browser.tabulate_column', load: 'browser.load', write: 'browser.export', first: 'browser.set_slice_start', last: 'browser.set_slice_end', restore: 'browser.restore_full_slice', addcol: 'browser.add_column', delcol: 'browser.delete_column'},
    ani: {file: 'animation.load', grab: 'animation.grab_point'},
    aplot: {redraw: 'array_plot.redraw', edit: 'array_plot.configure', fit: 'array_plot.fit', range: 'array_plot.range', print: 'array_plot.export_pdf', gif: 'array_plot.export_gif'},
  };
  return commands[window]?.[id] ?? '';
}
