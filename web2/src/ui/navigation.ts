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
    group[menu].map((id): NavigationItem | null => {
      const i = hello.menus[`${menu}_ids`].indexOf(id);
      return i < 0 ? null : {menu, id, label: hello.menus[menu][i], hint: hello.menus[`${menu}_hints`][i],
        shortcut: `${menu === 'file' ? 'F, ' : menu === 'num' ? 'U, ' : ''}${hello.menus[`${menu}_keys`][i].toUpperCase()}`};
    }).filter((item): item is NavigationItem => item !== null))
    .filter(item => words.every(word => `${item.label} ${item.hint} ${item.id}`.toLocaleLowerCase().includes(word))) : []}));
}
