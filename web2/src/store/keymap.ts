/* The user's keymap (W211, docs/protocol.md "Keymap"): what the core says the keys are, from hello and
   from each `keymap` reply, kept as it came. The core owns the file, the grammar and the checks
   (core/xpp_keymap.h); this slice only holds the effective keymap for the dispatcher (W208) and the
   editor (W212), and turns it back into the differences a `set` sends. Pure: no DOM, no I/O. */
import type {KeymapInfo, UserKeymap} from '../protocol/types';

export interface KeymapState {
  /** the effective keymap, null before hello */
  info: KeymapInfo | null;
}

export const initialKeymap: KeymapState = {info: null};

export type KeymapAction = {type: 'info'; info: KeymapInfo};

export function reduceKeymap(state: KeymapState, action: KeymapAction): KeymapState {
  return action.info === state.info ? state : {info: action.info};
}

/** the effective keys of command `id` ([] for a command with none, or before hello) */
export function keysOf(state: KeymapState, id: string): string[] {
  return state.info?.commands.find(c => c.id === id)?.keys ?? [];
}

/** the user's differences as a `set` sends them: the commands whose source is `user`, the pinned ones
    and the preset. A keymap the core marked as an error has no user part (its defaults are shown) */
export function userKeymap(info: KeymapInfo): UserKeymap {
  const bindings: Record<string, string[]> = {};
  for (const c of info.commands) if (c.source === 'user') bindings[c.id] = c.keys;
  return {preset: info.preset, pinned: info.pinned, bindings};
}
