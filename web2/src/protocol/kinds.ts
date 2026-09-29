/* What an action is, and so whether it may start now (W95, docs/protocol.md
   "Action kinds"). The core defines every action's kind once -- each menu
   item's (core/menus.cpp) and each other command's (core/ui_json.cpp's
   command table) -- and sends them in `hello`: the main-window menus'
   `menus.<name>_kinds`, the windows' key layers `windows` (their keys,
   kinds and the page's names for their items: the one place the page
   learns a window's keys), and `commands`. Only a running computation (the
   core's `computing` event, until the command's idle) or an open question
   disables anything; the page's own catch-up commands never do. */
import type {Command, HelloEvent} from './types';

/** control (Abort, Quit, an answer): always; view (only changes what is
    shown): also while a computation runs; data (saves, loads, values,
    settings) and computation (starts one): only when none runs */
export type Kind = 'control' | 'view' | 'data' | 'computation';

const OF_LETTER: Record<string, Kind> = {c: 'control', v: 'view', d: 'data', x: 'computation'};

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

/** a command that is not a key, with its kind (hello.commands); op absent: the command's other lines */
export interface CommandKind {
  cmd: string;
  op?: string;
  kind: string;
}

const MENU_NAMES = ['main', 'file', 'num'] as const;

/** the key of item `id` of window `win`'s layer ('' before hello, or for no such item) */
export function layerKey(hello: HelloEvent | null, win: LayerWindow, id: string): string {
  const l = hello?.windows?.[win];
  const i = l ? l.ids.indexOf(id) : -1;
  return i >= 0 ? l!.keys[i] : '';
}

/** the command of item `id` of window `win`'s layer */
export function windowKey(hello: HelloEvent | null, win: LayerWindow, id: string,
  extra: Record<string, unknown> = {}): {cmd: 'key'; win: string; key: string; [k: string]: unknown} {
  return {cmd: 'key', win, key: layerKey(hello, win, id), ...extra};
}

/** whether `cmd` is item `id` of window `win`'s layer */
export function isWindowKey(hello: HelloEvent | null, cmd: Command, win: LayerWindow, id: string): boolean {
  const key = layerKey(hello, win, id);
  return key !== '' && cmd.cmd === 'key' && cmd.win === win && cmd.key === key;
}

/** the kind of `cmd`, sent now with main-window menu `menu` shown (0 main, 1 File,
    2 nUmerics); null when hello does not say (an older server, a key no menu has) */
export function kindOf(hello: HelloEvent | null, menu: number, cmd: Command): Kind | null {
  if (!hello) return null;
  if (cmd.cmd === 'key') {
    const key = String(cmd.key ?? '');
    if (key === 'Escape') return 'control';
    if (typeof cmd.win === 'string') {
      const l = hello.windows?.[cmd.win as LayerWindow];
      const i = l ? l.keys.indexOf(key) : -1;
      return i >= 0 ? OF_LETTER[l!.kinds[i]] ?? null : null;
    }
    const name = MENU_NAMES[menu] ?? 'main';
    const keys = hello.menus[`${name}_keys`], kinds = hello.menus[`${name}_kinds`];
    const i = key.length === 1 && keys ? keys.indexOf(key) : -1;
    return i >= 0 && kinds ? OF_LETTER[kinds[i]] ?? null : null;
  }
  const e = hello.commands?.find(c => c.cmd === cmd.cmd && (c.op === undefined || c.op === cmd.op));
  return e ? OF_LETTER[e.kind] ?? null : null;
}

/** whether an action of `kind` may start: control always; while a question is open nothing
    else (only answering or cancelling it applies); while a computation runs view only.
    An unknown kind counts as a computation. */
export function mayStart(kind: Kind | null, computing: boolean, asking: boolean): boolean {
  if (kind === 'control') return true;
  if (asking) return false;
  return kind === 'view' || !computing;
}
