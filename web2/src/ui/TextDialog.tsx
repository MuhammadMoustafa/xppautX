/* A text on a plot (Text,etc/Text): its text, size, style and colour, and for one already there its
   position and Delete. One form for the ask that adds a text (AskDialog's `text`) and for the dialog a
   double click on a text opens (TextDialog), as the trace dialog opens on a legend item. The colours
   are the curves' (plot/colors.ts): the core keeps an index, which its PostScript and the other marks
   share, not the free colour TraceDialog's picker gives a legend. */
import {useRef, useState} from 'preact/hooks';
import {CURVE_COLOR_NAMES, curveColor} from '../plot/colors';
import {TEXT_STYLES, textPx} from '../plot/marks';
import {fieldsValid, NUMBER, TEXT} from '../store/fieldKinds';
import type {HelloEvent} from '../protocol/types';
import type {TextPosition, TextValues} from '../store/marks';
import {Field} from './Field';
import {useStore} from './context';
import {useDialogFocus} from './dialogFocus';
import {useDark} from './theme';

export function TextForm({initial, position, submit, onSubmit, onDelete}: {
  initial: TextValues; position: TextPosition | null; submit: string;
  onSubmit(v: TextValues, at: TextPosition | null): void; onDelete?: () => void;
}) {
  const limits = useStore(s => s.hello?.text) as HelloEvent['text'];
  const dark = useDark(useStore(s => s.theme));
  const [text, setText] = useState(initial.text);
  const [size, setSize] = useState(initial.size), [style, setStyle] = useState(initial.style), [color, setColor] = useState(initial.color);
  const [x, setX] = useState(position ? String(position.x) : ''), [y, setY] = useState(position ? String(position.y) : '');
  const specs = position ? [TEXT, NUMBER, NUMBER] : [TEXT];
  const texts = position ? [text, x, y] : [text];
  const valid = text !== '' && fieldsValid(specs, texts) && (!position || (x !== '' && y !== ''));
  return <form onSubmit={e => {
    e.preventDefault();
    if (valid) onSubmit({text, size, style, color}, position ? {x: Number(x), y: Number(y)} : null);
  }}>
    <div class="form-grid">
      <label><span>Text</span>
        <Field spec={TEXT} value={text} onInput={setText} maxLength={limits.max_length} data-text-input data-autofocus /></label>
      <label><span>Size</span>
        <select data-text-size value={size} onChange={e => setSize(Number(e.currentTarget.value))}>
          {Array.from({length: limits.size_max + 1}, (_, i) => <option key={i} value={i}>{i} ({textPx(i)} px)</option>)}
        </select></label>
      <label><span>Style</span>
        <select data-text-style value={style} onChange={e => setStyle(Number(e.currentTarget.value))}>
          {TEXT_STYLES.slice(0, limits.style_count).map((st, i) => <option key={i} value={i}>{st.name}</option>)}
        </select></label>
      <div class="text-colors" role="radiogroup" aria-label="Colour">
        <span>Colour</span>
        <span class="text-color-row">
          {Array.from({length: limits.color_max + 1}, (_, i) =>
            <button type="button" key={i} role="radio" aria-checked={color === i} aria-label={CURVE_COLOR_NAMES[i]}
              title={CURVE_COLOR_NAMES[i]} data-text-color={i} class={'text-color' + (color === i ? ' chosen' : '')}
              style={{background: curveColor(i, dark)}} onClick={() => setColor(i)} />)}
        </span>
      </div>
      {position && <>
        <label><span>X</span><Field spec={NUMBER} value={x} onInput={setX} data-text-x /></label>
        <label><span>Y</span><Field spec={NUMBER} value={y} onInput={setY} data-text-y /></label>
      </>}
    </div>
    <div class="dialog-actions">
      {onDelete && <button type="button" data-text-delete onClick={onDelete}>Delete</button>}
      <button type="submit" class="primary" disabled={!valid}>{submit}</button>
    </div>
  </form>;
}

/** the dialog a double click on a text opens: change any of it, or Delete it */
export function TextDialog({initial, position, close, apply, remove}: {
  initial: TextValues; position: TextPosition; close(): void;
  apply(v: TextValues, at: TextPosition): void; remove(): void;
}) {
  const box = useRef<HTMLDivElement>(null);
  useDialogFocus(box, []);
  return <div class="dialog-backdrop" onPointerDown={e => { if (e.target === e.currentTarget) close(); }}>
    <div class="dialog" ref={box} role="dialog" aria-modal="true" aria-labelledby="text-title" data-text-dialog
      onKeyDown={e => { if (e.key === 'Escape') { e.preventDefault(); e.stopPropagation(); close(); } }}>
      <div class="dialog-title-row"><h2 id="text-title">Edit text</h2>
        <button class="dialog-close" aria-label="Close text editor" onClick={close}>×</button></div>
      <TextForm initial={initial} position={position} submit="Apply"
        onSubmit={(v, at) => apply(v, at!)} onDelete={remove} />
    </div>
  </div>;
}
