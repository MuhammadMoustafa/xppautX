/* What an action is, and so whether it may start now (W95, docs/protocol.md
   "Action kinds"). The core defines every action's kind once -- each menu
   item's (core/menus.cpp) and each other command's (core/ui_json.cpp's
   command table) -- and sends them in `hello`: the main-window menus'
   `menus.<name>_kinds`, the windows' key layers `windows` (their keys,
   kinds and the page's names for their items: the one place the page
   learns a window's keys), and `commands`. Only a running computation (the
   core's `computing` event, until the command's idle) or an open question
   disables anything; the page's own catch-up commands never do. */
import type {Command, HelloEvent, MenuName} from './types';

/** control (Abort, Quit, an answer): always; view (only changes what is
    shown) and setting (a parameter, an initial or boundary condition, a
    delay, the numerics, AUTO's forms: W106, applied when a computation
    ends, to the next one): also while a computation runs; data (saves,
    loads) and computation (starts one): only when none runs */
export type Kind = 'control' | 'view' | 'setting' | 'data' | 'computation';

const OF_LETTER: Record<string, Kind> = {c: 'control', v: 'view', s: 'setting', d: 'data', x: 'computation'};

/** a window with a key layer of its own ({"cmd":"key","win":...}) */
export type LayerWindow = 'auto' | 'browser' | 'ani' | 'aplot' | 'equilibrium';

/** one window's key layer (hello.windows) */
export interface WindowLayer {
  items: string[];
  keys: string;
  kinds: string;
  /** the page's name for each item (param, run, write, go, ...) */
  ids: string[];
  hints: string[];
}

/** a command with its kind (hello.commands; a key's is its menu item's: "") and whether it is
    a step of the user's (what a recording records, files.ts's run record); op absent: the
    command's other lines */
export interface CommandKind {
  cmd: string;
  op?: string;
  kind: string;
  step: boolean;
}

/** the table's entry for `cmd` (the first with its op, else the one with none) */
function commandOf(hello: HelloEvent, cmd: Command): CommandKind | undefined {
  return hello.commands.find(c => c.cmd === cmd.cmd && (c.op === undefined || c.op === cmd.op));
}

/** whether `cmd` is a step of the user's (hello.commands) */
export function isStep(hello: HelloEvent, cmd: Command): boolean {
  return commandOf(hello, cmd)?.step ?? false;
}

/** the name of main-window menu `menu` (state's menu number), null for none */
export function menuName(hello: HelloEvent | null, menu: number): MenuName | null {
  return hello?.menus.names[menu] ?? null;
}

/** the number of main-window menu `name` (state's menu) */
export function menuNumber(hello: HelloEvent, name: MenuName): number {
  return hello.menus.names.indexOf(name);
}

/** the key of item `id` of main-window menu `name` ('' before hello, or for no such item) */
export function menuKey(hello: HelloEvent | null, name: MenuName, id: string): string {
  const i = hello ? hello.menus[`${name}_ids`].indexOf(id) : -1;
  return i >= 0 ? hello!.menus[`${name}_keys`][i] : '';
}

/** the command of item `id` of the main menu (its key in no window) */
export function mainKey(hello: HelloEvent | null, id: string): {cmd: 'key'; key: string} {
  return {cmd: 'key', key: menuKey(hello, 'main', id)};
}

/** A menu action does not depend on which legacy shortcut menu is active. */
export function menuCommand(menu: MenuName, item: string): Command {
  return {cmd: 'key', menu, item};
}

/** the key of item `id` of window `win`'s layer ('' before hello, or for no such item) */
export function layerKey(hello: HelloEvent | null, win: LayerWindow, id: string): string {
  const l = hello?.windows[win];
  const i = l ? l.ids.indexOf(id) : -1;
  return i >= 0 ? l!.keys[i] : '';
}

/** the command of item `id` of window `win`'s layer */
export function windowCommand(hello: HelloEvent | null, win: LayerWindow, id: string,
  extra: Record<string, unknown> = {}): {cmd: 'key'; win: string; key: string; [k: string]: unknown} {
  return {cmd: 'key', win, key: layerKey(hello, win, id), ...extra};
}

/** whether `cmd` is item `id` of window `win`'s layer */
export function isWindowKey(hello: HelloEvent | null, cmd: Command, win: LayerWindow, id: string): boolean {
  const key = layerKey(hello, win, id);
  return key !== '' && cmd.cmd === 'key' && cmd.win === win && cmd.key === key;
}

/** the kind of `cmd`, sent now with main-window menu `menu` shown (0 main, 1 File,
    2 nUmerics); null before hello, or for a key no menu has */
export function kindOf(hello: HelloEvent | null, menu: number, cmd: Command): Kind | null {
  /* Startup can ask for a model (or conversion choices) before hello.
     Its answer and cancellation must reach the waiting core. */
  if (!hello) return cmd.cmd === 'answer' || cmd.cmd === 'abort' || cmd.cmd === 'quit' ? 'control' : null;
  if (cmd.cmd === 'key') {
    if ('menu' in cmd || 'item' in cmd) {
      if (typeof cmd.menu !== 'string' || typeof cmd.item !== 'string' || 'key' in cmd || 'win' in cmd) return null;
      const name = hello.menus.names.find(n => n === cmd.menu);
      if (!name) return null;
      const i = hello.menus[`${name}_ids`].indexOf(cmd.item);
      return i >= 0 ? OF_LETTER[hello.menus[`${name}_kinds`][i]] ?? null : null;
    }
    const key = String(cmd.key ?? '');
    if (key === 'Escape') return 'control';
    if (typeof cmd.win === 'string') {
      const l = hello.windows[cmd.win as LayerWindow];
      const i = l ? l.keys.indexOf(key) : -1;
      return i >= 0 ? OF_LETTER[l!.kinds[i]] ?? null : null;
    }
    const name = menuName(hello, menu);
    if (!name) return null;
    const keys = hello.menus[`${name}_keys`], kinds = hello.menus[`${name}_kinds`];
    const i = key.length === 1 ? keys.indexOf(key) : -1;
    return i >= 0 ? OF_LETTER[kinds[i]] ?? null : null;
  }
  const e = commandOf(hello, cmd);
  return e ? OF_LETTER[e.kind] ?? null : null;
}

/** whether an action of `kind` may start: control and a setting always (a setting sent
    while a question is open or a computation runs applies when the core can: at once
    before the command computes, else when it ends); while a question is open nothing
    else (only answering or cancelling it applies); while a computation runs a view
    only. An unknown kind counts as a computation. */
export function mayStart(kind: Kind | null, computing: boolean, asking: boolean): boolean {
  if (kind === 'control' || kind === 'setting') return true;
  if (asking) return false;
  return kind === 'view' || !computing;
}
