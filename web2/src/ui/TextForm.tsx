/* A text placed on a plot (Text,etc/Text): its text, size, style and colour, the form of the ask that
   adds it (AskDialog's `text`). A text already there is edited through annotation.edit's command form,
   which a double click on it opens (PlotView), and moved or deleted in Plot > Annotations (W517). The
   colour is any colour with its alpha (ui/ColorField.tsx), which the core keeps and its PDF and SVG draw. */
import {useState} from 'preact/hooks';
import {CORE_COLOR} from '../plot/colors';
import {textPx} from '../plot/marks';
import {choiceLabel, fieldsValid, TEXT} from '../store/fieldKinds';
import type {HelloEvent, TextStyle} from '../protocol/types';
import type {TextValues} from '../store/marks';
import {ColorField} from './ColorField';
import {Field} from './Field';
import {useStore} from './context';
import {useDark} from './theme';

export function TextForm({initial, submit, onSubmit}: {
  initial: TextValues; submit: string; onSubmit(v: TextValues): void;
}) {
  const limits = useStore(s => s.hello?.text) as HelloEvent['text'];
  const styles = useStore(s => s.hello?.text_styles) as HelloEvent['text_styles'];
  const dark = useDark(useStore(s => s.theme));
  const [text, setText] = useState(initial.text);
  const [size, setSize] = useState(initial.size), [style, setStyle] = useState(initial.style), [color, setColor] = useState(initial.color);
  const valid = text !== '' && CORE_COLOR.test(color) && fieldsValid([TEXT], [text]);
  return <form onSubmit={e => {
    e.preventDefault();
    if (valid) onSubmit({text, size, style, color});
  }}>
    <div class="form-grid">
      <label><span>Text</span>
        <Field spec={TEXT} value={text} onInput={setText} maxLength={limits.max_length} data-text-input data-autofocus /></label>
      <label><span>Size</span>
        <select data-text-size value={size} onChange={e => setSize(Number(e.currentTarget.value))}>
          {Array.from({length: limits.size_max + 1}, (_, i) => <option key={i} value={i}>{i} ({textPx(i)} px)</option>)}
        </select></label>
      <label><span>Style</span>
        <select data-text-style value={style} onChange={e => setStyle(e.currentTarget.value as TextStyle)}>
          {styles.map(name => <option key={name} value={name}>{choiceLabel(name)}</option>)}
        </select></label>
      <label><span>Colour</span>
        <ColorField value={color} dark={dark} label="Text" onChange={setColor} /></label>
    </div>
    <div class="dialog-actions">
      <button type="submit" class="primary" disabled={!valid}>{submit}</button>
    </div>
  </form>;
}
