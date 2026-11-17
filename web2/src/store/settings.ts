/* The Settings window's own state (W792, docs/ui-v2.md): whether it is open, which file it shows (the user's settings.json or the
   model folder's) and which group, and the two files' values as `settings.get` answered them (scope user: the user's file alone;
   scope folder: what the model runs with, the folder's file on top). Pure: the commands it sends are the session's (session.ts), so
   a value the window shows is always what the core last said; the page keeps no second copy of the settings. */

export type SettingsScope = 'user' | 'folder';

/** `settings.get`'s answer: every value by its key (threads and automatic, the colors by their keys, theme ...), where each key is set
    (a key in neither file has its default), the file shown, the model folder and the most threads */
export interface SettingsValues {
  scope: SettingsScope;
  file: string;
  folder: string;
  threads_max: number;
  /** whether the file shown is on disk yet (the Open file button) */
  file_exists: boolean;
  sources: Record<string, SettingsScope>;
  [key: string]: unknown;
}

export interface SettingsState {
  open: boolean;
  scope: SettingsScope;
  group: string;
  values: Record<SettingsScope, SettingsValues | null>;
  /** why the last change was refused (the core's text), until the next one */
  error: string;
}

export const initialSettings: SettingsState = {open: false, scope: 'user', group: 'general', values: {user: null, folder: null}, error: ''};

export type SettingsAction =
  | {type: 'open'; group: string}
  | {type: 'close'}
  | {type: 'scope'; scope: SettingsScope}
  | {type: 'group'; group: string}
  | {type: 'values'; values: SettingsValues}
  | {type: 'error'; text: string};

export function reduceSettings(state: SettingsState, action: SettingsAction): SettingsState {
  switch (action.type) {
    case 'open':
      return {...state, open: true, group: action.group, error: ''};
    case 'close':
      return {...state, open: false, error: ''};
    case 'scope':
      return {...state, scope: action.scope, error: ''};
    case 'group':
      return {...state, group: action.group, error: ''};
    case 'values':
      return {...state, values: {...state.values, [action.values.scope]: action.values}};
    case 'error':
      return {...state, error: action.text};
  }
}

/** where a key's value comes from, as the window says it for the file shown: in the user's view the value is the user's or the default; in the
    folder's, the folder's, the user's or the default */
export function sourceOf(values: SettingsValues | null, group: string, key: string): SettingsScope | 'default' {
  return values?.sources[`${group}.${key}`] ?? 'default';
}
