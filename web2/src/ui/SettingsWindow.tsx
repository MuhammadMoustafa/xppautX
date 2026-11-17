/* The Settings window (W792, docs/ui-v2.md, docs/manual "Settings"): File > Settings…, and Help > Keyboard shortcuts at its Keymap group.
   The groups of settings.json at the side (a select on a phone), a User / Folder switch for the file shown (the user's settings.json or the
   model folder's .xppautx/settings.json, which goes on top of it key by key), each value with where it comes from (default, user, folder) and a
   ↺ that takes the key out of the file shown. A change is saved as it is made, in place (settings.configure; the core keeps the file's comments);
   what is shown is what `settings.get` last answered (store/settings.ts), never a copy the page keeps. */
import {useEffect, useRef, useState} from 'preact/hooks';
import type {SettingRow} from '../protocol/types';
import {CORE_COLOR} from '../plot/colors';
import {PANEL_LIMITS} from '../store/panels';
import {choiceLabel} from '../store/fieldKinds';
import {sourceOf, type SettingsScope, type SettingsValues} from '../store/settings';
import {ColorField} from './ColorField';
import {CommandLabel} from './CommandButton';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {Dialog} from './Dialog';
import {KeymapPane} from './KeymapPane';

/** how long a color or a number is left alone after the last edit before it is saved (a color picker sends a value at every move) */
const SETTLE_MS = 500;
/** the threads setting's word for "as many as pay": the core's (xpp_cpu.h THREADS_AUTO_WORD), as settings.json and the protocol write it */
const THREADS_AUTO = 'auto';

export function SettingsWindow() {
  const open = useStore(s => s.settings.open);
  return open ? <Window /> : null;
}

/** a row's value as text, from one of the two answers (the control's value, and the user's value shown beside a folder's) */
function valueText(row: SettingRow, values: SettingsValues | null): string {
  const v = values?.[row.key];
  if (row.kind === 'threads') return values?.automatic ? THREADS_AUTO : String(values?.threads ?? '');
  if (row.kind === 'flag') return v ? 'on' : 'off';
  if (row.kind === 'width') return v === null || v === undefined ? '' : String(v);
  return String(v ?? '');
}

function folderName(path: string): string {
  const parts = path.split(/[\\/]/).filter(Boolean);
  return parts[parts.length - 1] ?? '';
}

function Window() {
  const session = useSession();
  const hello = useStore(s => s.hello);
  const st = useStore(s => s.settings);
  const shown = st.values[st.scope];
  const folder = st.values.folder?.folder ?? '';
  const groups = hello?.settings.groups ?? [];
  const rows = (hello?.settings.rows ?? []).filter(r => r.group === st.group);
  const pick = (scope: SettingsScope) => session.store.dispatch({type: 'settingsUi', action: {type: 'scope', scope}});
  const go = (group: string) => session.store.dispatch({type: 'settingsUi', action: {type: 'group', group}});
  const info = hello?.settings;
  return <Dialog title={<CommandLabel command="settings.open" />} name="settings" width="list" onClose={() => session.closeSettings()}
    attrs={{'data-settings': st.group}} className="settings-dialog"
    headExtra={<>
      <div class="seg" role="group" aria-label="Settings file">
        <button type="button" class={'seg-item' + (st.scope === 'user' ? ' on' : '')} data-settings-scope="user" aria-pressed={st.scope === 'user'} onClick={() => pick('user')}>User</button>
        <button type="button" class={'seg-item' + (st.scope === 'folder' ? ' on' : '')} data-settings-scope="folder" aria-pressed={st.scope === 'folder'} disabled={!folder}
          title={folder ? folder : 'No model is open'} onClick={() => pick('folder')}>Folder{folder && <span class="scope-name">: {folderName(folder)}/</span>}</button>
      </div>
    </>}
    extra={<>
      <span class="settings-saved">Saved as you change, comments kept:</span>
      <code class="settings-file" data-settings-file title={shown?.file}>{shown?.file || 'no file: the system names no settings folder'}</code>
      <button type="button" data-settings-open-file disabled={!shown?.file_exists} title={shown?.file_exists ? 'Open it in the system\'s program for .json files' : 'No file yet: change a value to make it'}
        onClick={() => session.openSettingsFile()}>Open file</button>
    </>}>
    <div class="settings-top">
      <label class="settings-groupsel">
        <select data-settings-group-select aria-label="Settings group" value={st.group} onChange={e => go(e.currentTarget.value)}>
          {groups.map(g => <option key={g} value={g}>{choiceLabel(g)}</option>)}
        </select></label>
    </div>
    {info && !info.ok && <p class="field-error" role="alert" data-settings-bad>The settings file could not be loaded ({info.file}{info.line ? `:${info.line}` : ''}): {info.error}.
      The defaults are shown; changing a setting does not replace a file that is bad.</p>}
    {st.error && <p class="field-error" role="alert" data-settings-error>{st.error}</p>}
    <div class="settings-main">
      <nav class="settings-nav" aria-label="Settings groups">
        {groups.map(g => <button type="button" key={g} data-settings-group={g} aria-current={g === st.group ? 'page' : undefined} onClick={() => go(g)}>{choiceLabel(g)}</button>)}
      </nav>
      <div class="settings-pane" data-settings-pane={st.group} tabIndex={-1} data-autofocus={st.group === 'keymap' ? undefined : ''}>
        {st.group === 'keymap'
          ? (st.scope === 'folder'
            ? <p class="settings-note" data-settings-keymap-user>The keymap is the user's: the keys and pins are kept in their settings.json.{' '}
              <button type="button" onClick={() => pick('user')}>Show the user's</button></p>
            : <KeymapPane />)
          : <>
            <h3 class="settings-heading">{choiceLabel(st.group)}</h3>
            {shown ? rows.map(row => <SettingControl key={`${row.group}.${row.key}`} row={row} scope={st.scope} />) : <p role="status">Loading…</p>}
          </>}
      </div>
    </div>
  </Dialog>;
}

function SettingControl({row, scope}: {row: SettingRow; scope: SettingsScope}) {
  const session = useSession();
  const st = useStore(s => s.settings);
  const may = useMay();
  const values = st.values[scope];
  const user = st.values.user;
  const source = sourceOf(values, row.group, row.key);
  const locked = scope === 'folder' && !row.in_folder;
  const off = !may({cmd: 'settings.configure'}) || locked;
  const label = choiceLabel(row.key);
  const current = valueText(row, values);
  const save = (json: string) => void session.changeSetting(row.group, row.key, json);
  const sourceText = locked ? "Always the user's: choose User to change it"
    : scope === 'user' ? (source === 'user' ? 'Set here' : 'Default')
      : source === 'folder' ? `Set here · the user's: ${valueText(row, user) || 'default'}` : source === 'user' ? "From the user's file" : 'Default';
  return <div class="setting-row" data-setting={`${row.group}.${row.key}`} data-source={source}>
    <span class="setting-label">{label}</span>
    <span class="setting-help">{row.help}</span>
    <span class="setting-source" data-setting-source>{sourceText}</span>
    <span class="setting-control" title={off && !locked ? BUSY_TITLE : undefined}>
      <Control row={row} current={current} label={label} off={off} threadsMax={Number(values?.threads_max ?? 1)} save={save} />
      {source === scope && !locked && <button type="button" class="setting-reset" data-setting-reset={`${row.group}.${row.key}`} disabled={off}
        aria-label={`Reset ${label} to ${scope === 'user' ? 'the default' : "the user's"}`} title={scope === 'user' ? 'Back to the default' : "Back to the user's"}
        onClick={() => void session.changeSetting(row.group, row.key, null)}>↺</button>}
    </span>
  </div>;
}

function Control({row, current, label, off, threadsMax, save}: {row: SettingRow; current: string; label: string; off: boolean; threadsMax: number; save(json: string): void}) {
  if (row.kind === 'flag')
    return <input type="checkbox" role="switch" class="setting-switch" data-setting-input checked={current === 'on'} disabled={off} aria-label={label}
      onChange={e => save(e.currentTarget.checked ? 'true' : 'false')} />;
  if (row.kind === 'choice')
    return <select data-setting-input value={current} disabled={off} aria-label={label} onChange={e => save(JSON.stringify(e.currentTarget.value))}>
      {row.choices.map(c => <option key={c} value={c}>{choiceLabel(c)}</option>)}</select>;
  if (row.kind === 'threads') {
    const counts = Array.from({length: threadsMax}, (_, i) => String(i + 1));
    return <select data-setting-input value={current} disabled={off} aria-label={label}
      onChange={e => save(e.currentTarget.value === THREADS_AUTO ? JSON.stringify(THREADS_AUTO) : e.currentTarget.value)}>
      <option value={THREADS_AUTO}>{choiceLabel(THREADS_AUTO)}</option>{counts.map(c => <option key={c} value={c}>{c}</option>)}</select>;
  }
  if (row.kind === 'color') return <ColorSetting current={current} label={label} off={off} save={save} />;
  return <WidthSetting current={current} label={label} off={off} save={save} />;
}

/** the draft of a value that is saved when it settles (a color; a number): the control shows the draft until the core's answer replaces it */
function useSettled(current: string, save: (text: string) => void, ready: (text: string) => boolean): [string, (text: string) => void] {
  const [draft, setDraft] = useState(current);
  const timer = useRef<ReturnType<typeof setTimeout> | null>(null);
  useEffect(() => setDraft(current), [current]);
  useEffect(() => () => { if (timer.current !== null) clearTimeout(timer.current); }, []);
  return [draft, text => {
    setDraft(text);
    if (timer.current !== null) clearTimeout(timer.current);
    timer.current = ready(text) && text !== current ? setTimeout(() => save(text), SETTLE_MS) : null;
  }];
}

function ColorSetting({current, label, off, save}: {current: string; label: string; off: boolean; save(json: string): void}) {
  const [draft, edit] = useSettled(current, text => save(JSON.stringify(text)), text => CORE_COLOR.test(text));
  return <fieldset class="setting-color" disabled={off}><ColorField value={draft} onChange={edit} label={label} /></fieldset>;
}

function WidthSetting({current, label, off, save}: {current: string; label: string; off: boolean; save(json: string): void}) {
  const {min, max} = PANEL_LIMITS.dockWidth;
  const [draft, edit] = useSettled(current, text => save(text === '' ? 'null' : text), text => text === '' || (Number.isInteger(Number(text)) && Number(text) >= min && Number(text) <= max));
  return <span class="input-box"><input type="number" data-setting-input value={draft} min={min} max={max} placeholder="default" disabled={off} aria-label={`${label} in pixels`}
    onInput={e => edit(e.currentTarget.value)} /></span>;
}
