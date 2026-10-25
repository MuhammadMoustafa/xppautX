import {useRef, useState} from 'preact/hooks';
import {browserColor, CORE_COLOR, cssToHex} from '../plot/colors';
import {ColorField} from './ColorField';
import {useStore} from './context';
import {useDialogFocus} from './dialogFocus';
import {useDark} from './theme';

/** what the picker shows for a colour it cannot be given (never written back: Apply keeps the trace's own) */
const PICKER_BLACK = '#000000';

/** Legend presentation only: changing a name or colour never changes a model variable. */
export function TraceDialog({label, color, close, apply}: {
  label: string; color: string; close(): void; apply(label: string, color: string): void;
}) {
  const box = useRef<HTMLDivElement>(null);
  /* the picker takes `#rrggbb` and an alpha: the colour shown is that form of `color` (a CSS colour of the
     page's own is converted), and Apply keeps `color` itself unless the user picked another (black is never
     written for a colour that did not parse) */
  const dark = useDark(useStore(s => s.theme));
  const [name, setName] = useState(label), [picked, setPicked] = useState<string | null>(null);
  const shown = picked ?? (CORE_COLOR.test(color) ? color : cssToHex(color, browserColor) ?? PICKER_BLACK);
  useDialogFocus(box, []);
  return <div class="dialog-backdrop" onPointerDown={e => { if (e.target === e.currentTarget) close(); }}>
    <div class="dialog" ref={box} role="dialog" aria-modal="true" aria-labelledby="trace-title" data-trace-dialog
      onKeyDown={e => { if (e.key === 'Escape') { e.preventDefault(); e.stopPropagation(); close(); } }}>
      <div class="dialog-title-row"><h2 id="trace-title">Edit trace legend</h2>
        <button class="dialog-close" aria-label="Close trace editor" onClick={close}>×</button></div>
      <form onSubmit={e => { e.preventDefault(); if (name.trim() && (picked === null || CORE_COLOR.test(picked))) apply(name.trim(), picked ?? color); }}>
        <label>Legend text <input data-trace-name value={name} required
          onInput={e => setName(e.currentTarget.value)} /></label>
        <label>Colour <ColorField value={shown} dark={dark} label="Trace" onChange={setPicked} /></label>
        <p class="muted">This changes the legend, not the model's variables.</p>
        <div class="dialog-actions"><button class="primary" type="submit">Apply</button></div>
      </form>
    </div>
  </div>;
}
