import {useRef, useState} from 'preact/hooks';
import {useDialogFocus} from './dialogFocus';

/** Legend presentation only: changing a name or colour never changes a model variable. */
export function TraceDialog({label, color, close, apply}: {
  label: string; color: string; close(): void; apply(label: string, color: string): void;
}) {
  const box = useRef<HTMLDivElement>(null);
  const [name, setName] = useState(label), [css, setCss] = useState(color);
  useDialogFocus(box, []);
  return <div class="dialog-backdrop" onPointerDown={e => { if (e.target === e.currentTarget) close(); }}>
    <div class="dialog" ref={box} role="dialog" aria-modal="true" aria-labelledby="trace-title" data-trace-dialog
      onKeyDown={e => { if (e.key === 'Escape') { e.preventDefault(); e.stopPropagation(); close(); } }}>
      <div class="dialog-title-row"><h2 id="trace-title">Edit trace legend</h2>
        <button class="dialog-close" aria-label="Close trace editor" onClick={close}>×</button></div>
      <form onSubmit={e => { e.preventDefault(); if (name.trim()) apply(name.trim(), css); }}>
        <label>Legend text <input data-trace-name value={name} required
          onInput={e => setName(e.currentTarget.value)} /></label>
        <label>Colour <input data-trace-color type="color" value={css}
          onInput={e => setCss(e.currentTarget.value)} /></label>
        <p class="muted">This changes the legend, not the model's variables.</p>
        <div class="dialog-actions"><button class="primary" type="submit">Apply</button></div>
      </form>
    </div>
  </div>;
}
