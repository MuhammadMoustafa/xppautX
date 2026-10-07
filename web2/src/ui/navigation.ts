/* The command sidebar and search, made from hello.command_table (core/command_table.h): the core owns
   every command's id, category, label, description, kind and keys; this file only filters and orders. */
import {commandRow} from '../protocol/kinds';
import type {HelloEvent, MenuName} from '../protocol/types';

/** the main-menu item that enters each legacy shortcut layer (its row in hello.command_table) */
const LAYER_ENTRY: Record<Exclude<MenuName, 'main'>, string> = {file: 'file', num: 'numerics'};

/** the core's own name of shortcut layer `name` (the label of the main item that enters it) */
export function layerLabel(hello: HelloEvent, name: Exclude<MenuName, 'main'>): string {
  return commandRow(hello, 'main', LAYER_ENTRY[name])?.label ?? name;
}

export interface NavigationItem {
  menu: MenuName;
  id: string;
  label: string;
  description: string;
  shortcut: string;
}

export interface NavigationGroup {
  id: string;
  name: string;
  expanded: boolean;
  items: NavigationItem[];
}

/** Show the sequence that works from the current shortcut layer, not just
    the sequence from Main. Clicked identities remain independent of mode. */
export function contextualShortcut(shortcut: string, target: MenuName, active: MenuName | null): string {
  if (!active || active === 'main') return shortcut;
  if (target === active) return shortcut.split(', ').at(-1)!;
  return `Esc, ${shortcut}`;
}

/** the listed categories (the sidebar's groups) with the commands of each that match every word of
    `query` in label, description or id, in the table's order; with no query only the primary commands
    and those of the active shortcut layer `layer` */
export function navigationGroups(hello: HelloEvent | null, query: string, layer: MenuName | null): NavigationGroup[] {
  const words = query.trim().toLocaleLowerCase().split(/\s+/).filter(Boolean);
  return (hello?.command_categories ?? []).filter(category => category.listed).map(category => ({
    id: category.id, name: category.label, expanded: category.expanded,
    items: hello!.command_table.filter(row => row.category === category.id && (words.length > 0 || row.primary || row.menu === layer))
      .map((row): NavigationItem => ({menu: row.menu, id: row.id, label: row.label, description: row.description,
        shortcut: row.legacy_keys.join(', ')}))
      .filter(item => words.every(word => `${item.label} ${item.description} ${item.id}`.toLocaleLowerCase().includes(word)))}));
}

/** what the table disagrees with itself on: a row of an unknown menu or category, a menu and id (or a
    menu and key) twice, a category with no commands, a shortcut-layer switch that is listed, or a
    default key two commands share; each is shown as an error once, never skipped silently. That every
    key a menu's handler takes has a row is core-side (tools/keycheck.py). */
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
    }
    if (categories.get(row.category)?.listed === false && row.pinnable) problems.push(`Command ${where} is a shortcut-layer switch but can be pinned`);
  }
  for (const category of hello.command_categories)
    if (category.listed && !hello.command_table.some(row => row.category === category.id))
      problems.push(`Category ${category.id} has no commands`);
  return problems;
}
