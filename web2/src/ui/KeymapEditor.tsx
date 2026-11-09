import {registryRows} from './navigation';
/* The keymap editor (docs/command-design.md "Keymap editor"): a dialog listing every command with its
   keys and where they come from. Click a key, press the new combination (Esc cancels, Backspace clears);
   a key another command has asks "Replace / Cancel"; the keys the system keeps (hello.keymap.reserved) are
   refused. Every change is the `keymap` command's `set`: the answer is what the page then shows. The
   state (query, the key being recorded, the conflict) is the store's (store/keymap.ts), so a check reads it. */
import {useLayoutEffect, useRef} from 'preact/hooks';
import type {CommandRow} from '../protocol/kinds';
import {
  editorRows, effectiveKeys, recordKey, replaceKey, sourceLabel, togglePinned, withKey, withoutBinding, withoutKey,
  withPinned, userKeymap, type KeymapAction,
} from '../store/keymap';
import {Dialog} from './Dialog';
import {useSession, useStore} from './context';
import {CommandLabel} from './CommandButton';
import {keyName} from './hotkeys';

export function KeymapEditor() {
  const open = useStore(s => s.keymap.editor.open);
  return open ? <Editor /> : null;
}

function Editor() {
  const session = useSession();
  const hello = useStore(s => s.hello);
  const info = useStore(s => s.keymap.info);
  const editor = useStore(s => s.keymap.editor);
  const dialogBox = () => document.querySelector<HTMLElement>('[data-keymap-editor]');
  const searchKeys = useRef(false);
  const ui = (action: KeymapAction) => session.store.dispatch({type: 'keymapUi', action});
  const rows = registryRows(hello);
  /* a conflict is announced and its answer is a key away; after any change the focus stays in the dialog
     (the button that had it may be gone: Replace, a key removed), where the keys it records are heard */
  useLayoutEffect(() => {
    const dialog = dialogBox();
    if (!dialog) return;
    const rec = editor.recording;
    if (editor.conflict) dialog.querySelector<HTMLElement>('[data-conflict-replace]')?.focus();
    else if (rec) dialog.querySelector<HTMLElement>(`[data-key-cell="${rec.id}"][data-index="${rec.index}"]`)?.focus();
    else if (!dialog.contains(document.activeElement)) dialog.querySelector<HTMLElement>('[data-keymap-search]')?.focus();
  }, [editor.conflict, info, editor.recording]);
  if (!info) return null;
  const close = () => ui({type: 'close'});
  const rowOf = (id: string): CommandRow | undefined => rows.find(r => r.id === id);

  /* a key pressed while a key (or the search) is being recorded is the recording's, not the page's */
  const onKeyDown = (e: KeyboardEvent): boolean => {
    const rec = editor.recording;
    const name = keyName(e);
    if (rec || searchKeys.current) {
      if (!name) return true;
      if (name === 'Tab' || name === 'Shift+Tab') {
        searchKeys.current = false;
        ui({type: 'record', recording: null});
        return true;
      }
      e.preventDefault();
      e.stopPropagation();
      if (!rec) {
        searchKeys.current = false;
        if (name !== 'Escape') ui({type: 'query', query: name});
        return true;
      }
      const result = recordKey(info, rows, rec, name);
      const row = rowOf(rec.id);
      if (result.kind === 'cancel' || !row) ui({type: 'record', recording: null});
      else if (result.kind === 'clear') session.setKeymap(withoutKey(info, row, rec.index));
      else if (result.kind === 'refused') ui({type: 'refuse', text: result.text});
      else if (result.kind === 'conflict') ui({type: 'conflict', conflict: result.conflict});
      else session.setKeymap(withKey(info, row, rec.index, result.key));
      return true;
    }
    /* Escape answers a conflict first; the shell's own Escape closes the editor */
    if (e.key === 'Escape' && editor.conflict) {
      e.preventDefault();
      e.stopPropagation();
      ui({type: 'conflict', conflict: null});
      return true;
    }
    return false;
  };

  const conflict = editor.conflict;
  const conflictRow = conflict ? rowOf(conflict.id) : undefined;
  const otherRow = conflict ? rowOf(conflict.other) : undefined;
  const recordingRow = editor.recording ? rowOf(editor.recording.id) : undefined;
  const shown = editorRows(info, rows, editor.query);
  return <Dialog title={<CommandLabel command="keymap.edit" />} name="keyboard shortcuts" width="list" onClose={close} onKeyDown={onKeyDown}
    attrs={{'data-keymap-editor': ''}} className="keymap-dialog">
      {!info.ok && <p class="field-error" role="alert" data-keymap-error>
        The keymap file could not be loaded ({info.file}{info.line ? `:${info.line}` : ''}): {info.error}. The default keys are shown; changing a key replaces the file.</p>}
      <div class="keymap-tools">
        <label class="search-field"><span>Search</span><span class="input-box"><input type="search" data-autofocus data-keymap-search value={editor.query} placeholder="Name, id or keys"
          onInput={e => ui({type: 'query', query: e.currentTarget.value})} /></span></label>
        <button type="button" data-keymap-search-keys title="Press a key combination to find the commands on it"
          onClick={() => { searchKeys.current = true; dialogBox()?.querySelector<HTMLElement>('[data-keymap-search]')?.focus(); }}>Record keys</button>
        <button type="button" data-keymap-default onClick={() => session.setKeymap({...userKeymap(info), bindings: {}})}>Reset to defaults</button>
        {editor.confirmingReset
          ? <span class="keymap-confirm" role="group" aria-label="Reset all keys and pins">Reset every key and pin?{' '}
            <button type="button" class="danger" data-keymap-reset-confirm onClick={() => session.resetKeymap()}>Reset all</button>{' '}
            <button type="button" onClick={() => ui({type: 'confirmReset', on: false})}>Cancel</button></span>
          : <button type="button" data-keymap-reset-all onClick={() => ui({type: 'confirmReset', on: true})}>Reset all</button>}
      </div>
      <p class="keymap-live" role="status" aria-live="polite" data-keymap-status>
        {[editor.refusal, recordingRow && `Press the new key for ${recordingRow.label}. Esc cancels, Backspace removes the key.`].filter(Boolean).join(' ')}</p>
      {conflict && conflictRow && otherRow && <p class="keymap-conflict" role="alert" data-keymap-conflict>
        <kbd>{conflict.key}</kbd> is {otherRow.label}.{' '}
        <button type="button" class="primary" data-conflict-replace
          onClick={() => session.setKeymap(replaceKey(info, rows, conflictRow, conflict.index, conflict.key))}>Replace</button>{' '}
        <button type="button" data-conflict-cancel onClick={() => ui({type: 'conflict', conflict: null})}>Cancel</button></p>}
      <div class="keymap-scroll">
        <table class="keymap-table">
          <thead><tr><th scope="col">Command</th><th scope="col">Category</th><th scope="col">Keys</th><th scope="col">Source</th><th scope="col">Actions</th></tr></thead>
          <tbody>{shown.map(row => {
            const keys = effectiveKeys(info, row);
            const source = sourceLabel(info, row);
            const pinned = info.pinned.includes(row.id);
            const recording = editor.recording?.id === row.id ? editor.recording.index : -1;
            const record = (index: number) => ui({type: 'record', recording: {id: row.id, index}});
            return <tr key={row.id} data-keymap-row={row.id}>
              <th scope="row" title={row.description}>{row.label}</th>
              <td>{row.category}</td>
              <td class="keymap-keys">{[...keys, ...(recording === keys.length ? [''] : [])].map((key, i) =>
                <span class="keymap-key" key={i}>
                  <button type="button" data-key-cell={row.id} data-index={i} aria-label={`${row.label}: ${key || 'new key'}. Press Enter to change it`}
                    onClick={e => { e.currentTarget.focus(); record(i); }}
                    onBlur={() => { if (editor.recording?.id === row.id && editor.recording.index === i) ui({type: 'record', recording: null}); }}>
                    {recording === i ? <em>Press keys…</em> : <kbd>{key}</kbd>}</button>
                  {key && <button type="button" class="keymap-remove" data-key-remove={row.id} aria-label={`Remove ${key} from ${row.label}`}
                    onClick={() => session.setKeymap(withoutKey(info, row, i))}>×</button>}
                </span>)}
                {keys.length === 0 && recording !== 0 && <button type="button" data-key-cell={row.id} data-index={0}
                  aria-label={`${row.label}: no key. Press Enter to set one`} onClick={e => { e.currentTarget.focus(); record(0); }}>None</button>}
                {keys.length > 0 && keys.length < info.limits.keys && recording < 0 &&
                  <button type="button" data-key-add={row.id} aria-label={`Add a second key to ${row.label}`}
                    onClick={e => { e.currentTarget.focus(); record(keys.length); }}>+</button>}
              </td>
              <td data-source={source}>{source}</td>
              <td class="keymap-actions">
                {source !== 'default' && <button type="button" data-key-reset={row.id} aria-label={`Reset the keys of ${row.label}`}
                  onClick={() => session.setKeymap(withoutBinding(info, row.id))}>Reset</button>}
                {row.pinnable && <button type="button" data-key-pin={row.id} aria-pressed={pinned}
                  aria-label={`${pinned ? 'Unpin' : 'Pin'} ${row.label} ${pinned ? 'from' : 'to'} the toolbar`}
                  onClick={() => session.setKeymap(withPinned(info, togglePinned(info.pinned, row.id)))}>{pinned ? 'Unpin' : 'Pin'}</button>}
              </td>
            </tr>;
          })}</tbody>
        </table>
        {!shown.length && <p role="status">No command matches.</p>}
      </div>
  </Dialog>;
}
