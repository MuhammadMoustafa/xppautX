/* Colours as the core says them (core/colormap.h): `#rrggbb`, `#rrggbbaa` (alpha only when not opaque) or
   `foreground`, the colour the page draws dark on a light theme and light on a dark one. Every colour of a
   curve, a mark, a nullcline or a text arrives in that form and is drawn as it is (CSS reads `#rrggbbaa`),
   the foreground by the theme; the pickers give it back in the same form. */
import type {CoreColor} from '../protocol/types';

export type {CoreColor};

/** the foreground colour of each theme (the plot's text and default curve) */
const FOREGROUND = {light: '#1f2937', dark: '#e5e7eb'} as const;

/** the colour the canvas or CSS draws `c` in */
export function cssColor(c: CoreColor, dark: boolean): string {
  return c === 'foreground' ? (dark ? FOREGROUND.dark : FOREGROUND.light) : c;
}

const CORE_HEX = /^#([0-9a-f]{6})([0-9a-f]{2})?$/i;

/** a colour as the core writes it: `foreground`, `#rrggbb` or `#rrggbbaa` */
export const CORE_COLOR = /^(foreground|#[0-9a-f]{6}([0-9a-f]{2})?)$/i;

/** a colour as a picker has it: its `#rrggbb` and its alpha, 0 to 255 (the foreground: the theme's, opaque) */
export function splitColor(c: CoreColor, dark: boolean): {hex: string; alpha: number} {
  const m = CORE_HEX.exec(cssColor(c, dark));
  return m ? {hex: '#' + m[1].toLowerCase(), alpha: m[2] ? parseInt(m[2], 16) : 255} : {hex: '#000000', alpha: 255};
}

/** a picker's `#rrggbb` and alpha as the core's colour text (`#rrggbbaa` only when not opaque) */
export function joinColor(hex: string, alpha: number): CoreColor {
  const a = Math.max(0, Math.min(255, Math.round(alpha)));
  return hex.toLowerCase() + (a === 255 ? '' : a.toString(16).padStart(2, '0'));
}

const HEX = /^#([0-9a-f]{3}|[0-9a-f]{6})$/i;
const FUNCTION = /^(rgba?|hsla?)\(\s*([^)]*)\)$/i;
/** the channel and alpha separators CSS allows: commas, spaces, a slash before alpha */
const SEPARATORS = /[\s,/]+/;

const channel = (n: number) => Math.round(Math.min(255, Math.max(0, n))).toString(16).padStart(2, '0');

/** One CSS colour as the `#rrggbb` an <input type=color> takes, or null when it is not one this
    parses (a named colour: pass `resolve`, the browser's own conversion, which only the page has).
    Alpha is dropped: the picker has none. */
export function cssToHex(css: string, resolve?: (css: string) => string | null): string | null {
  const text = css.trim();
  const hex = HEX.exec(text);
  if (hex) return '#' + (text.length === 4 ? [...hex[1]].map(c => c + c).join('') : hex[1]).toLowerCase();
  const fn = FUNCTION.exec(text);
  if (fn) {
    const parts = fn[2].trim().split(SEPARATORS);
    const number = (p: string, percentOf: number) => p.endsWith('%') ? Number(p.slice(0, -1)) * percentOf / 100 : Number(p);
    if (parts.length < 3 || parts.length > 4) return null;
    let rgb: number[];
    if (fn[1].toLowerCase().startsWith('rgb')) rgb = parts.slice(0, 3).map(p => number(p, 255));
    else {
      const h = (((parseFloat(parts[0]) % 360) + 360) % 360) / 360, s = number(parts[1], 100) / 100, l = number(parts[2], 100) / 100;
      const a = s * Math.min(l, 1 - l);
      const f = (n: number) => { const k = (n + h * 12) % 12; return (l - a * Math.max(-1, Math.min(k - 3, 9 - k, 1))) * 255; };
      rgb = [f(0), f(8), f(4)];
    }
    return rgb.every(Number.isFinite) ? '#' + rgb.map(channel).join('') : null;
  }
  const resolved = resolve?.(text) ?? null;
  return resolved !== null && resolved !== text ? cssToHex(resolved) : null;
}

/** the browser's own conversion of any CSS colour (a name included) to a form `cssToHex` parses;
    null for text it does not accept */
export function browserColor(css: string): string | null {
  const ctx = document.createElement('canvas').getContext('2d');
  if (!ctx) return null;
  /* an unaccepted colour leaves fillStyle as it was: two different starting values tell */
  ctx.fillStyle = '#000000'; ctx.fillStyle = css;
  const first = String(ctx.fillStyle);
  ctx.fillStyle = '#ffffff'; ctx.fillStyle = css;
  return first === String(ctx.fillStyle) ? first : null;
}
