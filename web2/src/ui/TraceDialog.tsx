import {useState} from 'preact/hooks';
import {browserColor, CORE_COLOR, cssToHex} from '../plot/colors';
import {ColorField} from './ColorField';
import {useStore} from './context';
import {Dialog} from './Dialog';
import {FormRow} from './FormFields';
import {useDark} from './theme';

/** what the picker shows for a colour it cannot be given (never written back: Apply keeps the trace's own) */
const PICKER_BLACK = '#000000';

/** Legend presentation only: changing a name or colour never changes a model variable. */
export function TraceDialog({label, color, close, apply}: {
  label: string; color: string; close(): void; apply(label: string, color: string): void;
}) {
  /* the picker takes `#rrggbb` and an alpha: the colour shown is that form of `color` (a CSS colour of the
     page's own is converted), and Apply keeps `color` itself unless the user picked another (black is never
     written for a colour that did not parse) */
  const dark = useDark(useStore(s => s.theme));
  const [name, setName] = useState(label), [picked, setPicked] = useState<string | null>(null);
  const shown = picked ?? (CORE_COLOR.test(color) ? color : cssToHex(color, browserColor) ?? PICKER_BLACK);
  const ok = !!name.trim() && (picked === null || CORE_COLOR.test(picked));
  return <Dialog title="Edit trace legend" name="trace editor" description="This changes the legend, not the model's variables." onClose={close}
    action={{label: 'Apply'}} attrs={{'data-trace-dialog': ''}}
    onSubmit={e => { e.preventDefault(); if (ok) apply(name.trim(), picked ?? color); }}>
    <FormRow name="legend" label="Legend text" required>
      <span class="input-box"><input data-trace-name value={name} aria-label="Legend text" onInput={e => setName(e.currentTarget.value)} /></span>
    </FormRow>
    <FormRow name="colour" label="Colour"><ColorField value={shown} dark={dark} label="Trace" onChange={setPicked} /></FormRow>
  </Dialog>;
}
