/* Visual grouping only: labels, keys, kinds and stable identities belong to hello.menus. */
import type {HelloEvent, MenuName} from '../protocol/types';

export const COMMAND_GROUPS = [
  {name: 'Run', main: ['initialconds', 'continue', 'parameters'], file: [], num: ['total', 'start', 'transient', 'dt', 'method', 'noutput', 'bounds']},
  {name: 'Files', main: [], file: ['openmodel', 'opensession', 'savesession', 'reload', 'importset', 'saveinfo', 'quit'], num: []},
  {name: 'Analysis', main: ['singpts', 'nullcline', 'dirfield', 'bndryval'], file: ['auto'],
    num: ['ncline', 'singpt', 'delay', 'stochastic', 'poincare', 'ruelle', 'bndval', 'averaging']},
  {name: 'Plot', main: ['window', 'phasespace', 'graphic', 'viewaxes', 'xivst', 'text', 'makewindow', 'restore', '3dparams', 'erase', 'kinescope'], file: [], num: ['colorcode']},
  {name: 'Tools', main: [], file: ['source', 'calculator', 'transpose', 'getparset', 'clone', 'xpprc', 'tutorial', 'copyset', 'record', 'play', 'help'], num: ['lookup']},
] as const;

/** the main-menu item that enters each legacy shortcut layer (hello.menus.main_ids) */
const LAYER_ENTRY: Record<Exclude<MenuName, 'main'>, string> = {file: 'file', num: 'numerics'};

/** the items that are shortcut-layer switches, not commands: no group lists them (the page shows the
    layer in its own panel, with Esc to return) */
export const LAYER_SWITCHES: Record<MenuName, readonly string[]> = {main: ['file', 'numerics'], file: [], num: ['exit']};

/** the core's own name of shortcut layer `name` (the label of the main item that enters it) */
export function layerLabel(hello: HelloEvent, name: Exclude<MenuName, 'main'>): string {
  return hello.menus.main[hello.menus.main_ids.indexOf(LAYER_ENTRY[name])];
}

export interface NavigationItem {
  menu: MenuName;
  id: string;
  label: string;
  hint: string;
  shortcut: string;
}

/** Show the sequence that works from the current shortcut layer, not just
    the sequence from Main. Clicked identities remain independent of mode. */
export function contextualShortcut(shortcut: string, target: MenuName, active: MenuName | null): string {
  if (!active || active === 'main') return shortcut;
  if (target === active) return shortcut.split(', ').at(-1)!;
  return `Esc, ${shortcut}`;
}

export function navigationGroups(hello: HelloEvent | null, query: string) {
  const words = query.trim().toLocaleLowerCase().split(/\s+/).filter(Boolean);
  return COMMAND_GROUPS.map(group => ({name: group.name, items: hello ? (['main', 'file', 'num'] as const).flatMap(menu =>
    group[menu].flatMap((id): NavigationItem[] => {
      const i = hello.menus[`${menu}_ids`].indexOf(id);
      /* an id the core lacks is reported by navigationProblems, once, not listed */
      return i < 0 ? [] : [{menu, id, label: hello.menus[menu][i], hint: hello.menus[`${menu}_hints`][i],
        shortcut: `${menu === 'file' ? 'F, ' : menu === 'num' ? 'U, ' : ''}${hello.menus[`${menu}_keys`][i].toUpperCase()}`}];
    }))
    .filter(item => words.every(word => `${item.label} ${item.hint} ${item.id}`.toLocaleLowerCase().includes(word))) : []}));
}

/** what the groups and hello disagree on: an id a group names that hello lacks, or one hello has that
    no group (and no layer switch) lists; each is shown as an error once, never skipped silently */
export function navigationProblems(hello: HelloEvent): string[] {
  const grouped = new Map<string, number>();
  for (const group of COMMAND_GROUPS) for (const menu of ['main', 'file', 'num'] as const)
    for (const id of group[menu]) grouped.set(`${menu}:${id}`, (grouped.get(`${menu}:${id}`) ?? 0) + 1);
  const problems: string[] = [];
  const known = new Set<string>();
  for (const menu of ['main', 'file', 'num'] as const) for (const id of hello.menus[`${menu}_ids`]) {
    known.add(`${menu}:${id}`);
    const n = grouped.get(`${menu}:${id}`) ?? 0;
    if (LAYER_SWITCHES[menu].includes(id)) {
      if (n) problems.push(`Command ${menu}/${id} is a shortcut-layer switch but is also grouped`);
    } else if (n !== 1) problems.push(`Command ${menu}/${id} is in ${n} command groups, not one`);
  }
  for (const key of grouped.keys()) if (!known.has(key)) problems.push(`Command group names ${key.replace(':', '/')}, which the core does not have`);
  return problems;
}
