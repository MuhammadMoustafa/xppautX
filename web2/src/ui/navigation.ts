/* The command sidebar and search, made from hello.command_table (core/command_table.h): the core owns
   every command's id, category, label, description, kind and keys; this file only filters and orders. */
import type {HelloEvent, MenuName} from '../protocol/types';
import type {KeyPreset} from '../store/state';
import {RESERVED_KEYS, shortcutLabel} from './hotkeys';

export interface NavigationItem {
  menu: MenuName;
  id: string;
  label: string;
  description: string;
  /** its keys as listed (shortcutLabel) */
  shortcut: string;
  /** its first default key, empty for none */
  defaultKey: string;
}

export interface NavigationGroup {
  id: string;
  name: string;
  expanded: boolean;
  items: NavigationItem[];
}

/** the listed categories (the sidebar's groups) with the commands of each that match every word of
    `query` in label, description or id, in the table's order; with no query only the primary commands */
export function navigationGroups(hello: HelloEvent | null, query: string, preset: KeyPreset): NavigationGroup[] {
  const words = query.trim().toLocaleLowerCase().split(/\s+/).filter(Boolean);
  return (hello?.command_categories ?? []).filter(category => category.listed).map(category => ({
    id: category.id, name: category.label, expanded: category.expanded,
    items: hello!.command_table.filter(row => row.category === category.id && (words.length > 0 || row.primary))
      .map((row): NavigationItem => ({menu: row.menu, id: row.id, label: row.label, description: row.description,
        shortcut: shortcutLabel(row, preset), defaultKey: row.default_keys[0] ?? ''}))
      .filter(item => words.every(word => `${item.label} ${item.description} ${item.id}`.toLocaleLowerCase().includes(word)))}));
}

/** what the table disagrees with itself on: a row of an unknown menu or category, a menu and id (or a
    menu and key) twice, a category with no commands, a shortcut-layer switch that is listed, or a
    default key two commands share, or one the system or the browser keeps (RESERVED_KEYS); each is shown as an
    error once, never skipped silently. That every key a menu's handler takes has a row is core-side
    (tools/keycheck.py). */
export function navigationProblems(hello: HelloEvent): string[] {
  const problems: string[] = [];
  const categories = new Map(hello.command_categories.map(category => [category.id, category]));
  const ids = new Set<string>(), keys = new Set<string>(), defaults = new Map<string, string>();
  for (const row of hello.command_table) {
    const where = `${row.menu}/${row.id}`;
    if (!hello.menu_names.includes(row.menu)) problems.push(`Command ${where} is in menu ${row.menu}, which the core does not have`);
    if (!categories.has(row.category)) problems.push(`Command ${where} is in category ${row.category}, which the core does not have`);
    if (ids.has(where)) problems.push(`Command ${where} is in the command table twice`);
    ids.add(where);
    const key = `${row.menu}:${row.key}`;
    if (keys.has(key)) problems.push(`Command ${where} shares its key with another command of menu ${row.menu}`);
    keys.add(key);
    for (const chord of row.default_keys) {
      const other = defaults.get(chord);
      if (other) problems.push(`Commands ${other} and ${where} are both on ${chord}`);
      defaults.set(chord, where);
      if (chord.split(' ').some(key => RESERVED_KEYS.includes(key))) problems.push(`Command ${where} is on ${chord}, which the system or the browser keeps`);
    }
    if (categories.get(row.category)?.listed === false && row.pinnable) problems.push(`Command ${where} is a shortcut-layer switch but can be pinned`);
  }
  for (const category of hello.command_categories)
    if (category.listed && !hello.command_table.some(row => row.category === category.id))
      problems.push(`Category ${category.id} has no commands`);
  return problems;
}
