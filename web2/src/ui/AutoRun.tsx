/* AUTO's Run: the kind of continuation and, from a grabbed point, whether to switch branches.
   It sends `auto.run` (docs/protocol.md); which kinds a start takes is the core's own table, in
   hello.auto_run and the `autoinfo` event's `start` -- the page never decides it. */
import {useRef, useState} from 'preact/hooks';
import {choiceLabel} from '../store/fieldKinds';
import {pointCount, activeView} from '../store/diagram';
import {useSession, useStore} from './context';
import {FOCUSABLE, useDialogFocus} from './dialogFocus';

/** the kinds whose start needs more than a kind: the Equations dialog of AUTO's key menu asks for them */
const NEEDS_MORE = ['homoclinic', 'heteroclinic'];

export function AutoRunDialog({onClose}: {onClose: () => void}) {
  const session = useSession();
  const table = useStore(s => s.hello?.auto_run);
  const start = useStore(s => s.diagram.start);
  const points = useStore(s => pointCount(activeView(s.diagram).points));
  const kinds = (start && start.label > 0 ? start.kinds : table?.current ?? []).filter(k => !NEEDS_MORE.includes(k));
  const [kind, setKind] = useState<string>(kinds[0] ?? '');
  const [switching, setSwitching] = useState(false);
  const [replace, setReplace] = useState(false);
  const box = useRef<HTMLDivElement>(null);
  useDialogFocus(box, []);
  if (!table) return null;
  const switchable = !!start && start.switch.includes(kind);
  const fresh = !start && kind === 'equilibrium' && points > 1;
  const ok = () => {
    if (!kind) return;
    session.autoRun({
      kind,
      ...(start && start.label > 0 ? {label: start.label} : {}),
      ...(switchable ? {switch: switching} : {}),
      ...(fresh ? {replace_diagram: replace} : {}),
    });
    onClose();
  };
  const onKeyDown = (e: KeyboardEvent) => {
    if (e.key === 'Escape') {
      e.preventDefault();
      e.stopPropagation();
      onClose();
    } else if (e.key === 'Tab') {
      const all = [...box.current!.querySelectorAll<HTMLElement>(FOCUSABLE)];
      if (!all.length) return;
      const i = all.indexOf(document.activeElement as HTMLElement);
      e.preventDefault();
      all[e.shiftKey ? (i <= 0 ? all.length - 1 : i - 1) : (i === all.length - 1 ? 0 : i + 1)].focus();
    } else {
      e.stopPropagation();
    }
  };
  return (
    <div class="dialog-backdrop" onPointerDown={e => { if (e.target === e.currentTarget) onClose(); }}>
      <div class="dialog auto-settings-dialog" ref={box} role="dialog" aria-modal="true" aria-labelledby="auto-run-title"
        data-settings="auto-run" onKeyDown={onKeyDown}>
        <div class="dialog-title-row">
          <h2 id="auto-run-title">AUTO Run</h2>
          <button class="dialog-close" aria-label="Close AUTO Run" onClick={onClose}>×</button>
        </div>
        <p class="muted auto-settings-note">
          {start && start.label > 0
            ? `From label ${start.label}, a ${choiceLabel(start.type).toLowerCase()} point of a ${choiceLabel(start.family).toLowerCase()} branch.`
            : 'From the current state of the model.'}
        </p>
        {kinds.length === 0 && <p class="field-error" role="alert">This point starts no run from here.</p>}
        <form class="auto-run" onSubmit={e => { e.preventDefault(); ok(); }}>
          <label>Kind{' '}
            <select value={kind} data-field="run-kind" data-autofocus="" onChange={e => setKind((e.target as HTMLSelectElement).value)}>
              {kinds.map(k => <option key={k} value={k}>{choiceLabel(k)}</option>)}
            </select>
          </label>
          {switchable && (
            <label><input type="checkbox" data-field="run-switch" checked={switching}
              onChange={e => setSwitching((e.target as HTMLInputElement).checked)} /> Switch branches</label>
          )}
          {fresh && (
            <label><input type="checkbox" data-field="run-replace" checked={replace}
              onChange={e => setReplace((e.target as HTMLInputElement).checked)} /> Begin the diagram again</label>
          )}
        </form>
        <div class="dialog-actions">
          <button type="button" onClick={onClose}>Cancel</button>
          <button type="button" class="primary" data-field="run-ok" disabled={!kind} onClick={ok}>Run</button>
        </div>
      </div>
    </div>
  );
}
