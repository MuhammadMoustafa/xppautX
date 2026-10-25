/* A colour picker that gives the core's colour (core/colormap.h): `#rrggbb`, `#rrggbbaa` when the alpha is not
   full, or `foreground`. The native colour input picks the red, green and blue, the slider the alpha, the box
   shows (and takes) the text the core reads; the foreground button gives the theme's own colour back. */
import {CORE_COLOR, joinColor, splitColor} from '../plot/colors';

export function ColorField({value, onChange, dark, label}: {
  value: string; onChange(text: string): void; dark: boolean; label: string;
}) {
  const valid = CORE_COLOR.test(value);
  const {hex, alpha} = splitColor(valid ? value : 'foreground', dark);
  return <span class="color-field" data-color-field>
    <input type="color" value={hex} aria-label={`${label}: colour`} data-color-hex
      onInput={e => onChange(joinColor(e.currentTarget.value, alpha))} />
    <input type="range" min={0} max={255} value={alpha} aria-label={`${label}: opacity`} data-color-alpha
      onInput={e => onChange(joinColor(hex, Number(e.currentTarget.value)))} />
    <input type="text" value={value} spellcheck={false} autocomplete="off" aria-label={`${label}: colour text`} data-color-text
      aria-invalid={valid ? undefined : 'true'} onInput={e => onChange(e.currentTarget.value)} />
    <button type="button" data-color-foreground disabled={value === 'foreground'} onClick={() => onChange('foreground')}>Foreground</button>
  </span>;
}
