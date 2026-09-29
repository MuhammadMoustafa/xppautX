/* The keys of the windows' own layers ({"cmd":"key","win":...}, docs/protocol.md),
   as core/menus.cpp defines them (menu_auto_window, menu_browser_window,
   menu_ani_window, menu_aplot_window, menu_equilibrium_window): the one
   place the page names them. A window's button sends its key. */
export const AUTO_KEYS = {
  param: 'p', axes: 'a', numerics: 'n', run: 'r', grab: 'g', usr: 'u', clear: 'c', redraw: 'd', file: 'f',
} as const;

export const BROWSER_KEYS = {
  find: 'f', get: 'g', replace: 'r', unreplace: 'u', table: 't', first: 'h', last: 'e', restore: 's',
  addcol: 'a', delcol: 'd', load: 'l', write: 'w',
} as const;

export const ANI_KEYS = {file: 'f', go: 'g', reset: 'r', skip: 's', mpeg: 'm', fly: 'o', grab: 'a'} as const;

export const APLOT_KEYS = {redraw: 'd', edit: 'e', fit: 'f', range: 'r', print: 'p', gif: 'g'} as const;

export const EQUILIBRIUM_KEYS = {import: 'i'} as const;

/** the command of a window's key */
export function windowKey(win: 'auto' | 'browser' | 'ani' | 'aplot' | 'equilibrium', key: string,
  extra: Record<string, unknown> = {}): {cmd: 'key'; win: string; key: string; [k: string]: unknown} {
  return {cmd: 'key', win, key, ...extra};
}

/** whether `cmd` is the key `key` of window `win` */
export function isWindowKey(cmd: {cmd: string; [k: string]: unknown}, win: string, key: string): boolean {
  return cmd.cmd === 'key' && cmd.win === win && cmd.key === key;
}
