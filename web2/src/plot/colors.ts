/* Colors as the core says them (core/colormap.h): `#rrggbb`, `#rrggbbaa` (alpha only when not opaque) or
   `foreground`, the color the page draws dark on a light theme and light on a dark one. Every color of a
   curve, a mark, a nullcline or a text arrives in that form. It is drawn as it is, except on the dark theme,
   where a color too dark to read against the background is lightened (cssColor); the foreground by the
   theme (--fg); the pickers give back the color as given (splitColor, joinColor). */
import type {CoreColor} from '../protocol/types';
import {cssVar, darkTheme} from './css';

export type {CoreColor};

/** each color lightened for a background, by the color and the background: cssColor runs per curve per frame */
const lightened = new Map<string, string>();

/** the contrast a curve on the dark theme's background is lightened to: WCAG 2.2's for text (1.4.3 AA, 4.5:1), since
    the maintainer found its ratio for a graphic (1.4.11, 3:1) left pure blue too dim (2026-11-25) */
export const CURVE_CONTRAST = 4.5;
/** how far one step raises a too-dark color's HSL lightness (0 to 1): small, so the lightening stops just past CURVE_CONTRAST */
const LIGHTEN_STEP = 0.01;

/** the color the canvas or CSS draws `c` in: `foreground` is the theme's --fg (tokens.css), read from the page;
    on the dark theme a color that does not reach CURVE_CONTRAST against --surface, the plots' background, is lightened
    (lightenForBackground) */
export function cssColor(c: CoreColor): string {
  if (c === 'foreground') return cssVar('--fg');
  const bg = darkTheme() ? cssToHex(cssVar('--surface')) : null;
  if (!bg) return c;
  const key = c + bg;
  let out = lightened.get(key);
  if (out === undefined) lightened.set(key, out = lightenForBackground(c, bg));
  return out;
}

const CORE_HEX = /^#([0-9a-f]{6})([0-9a-f]{2})?$/i;

/** a color as a picker has it: `foreground` as the theme's --fg, anything else as the core wrote it (never lightened) */
function givenColor(c: CoreColor): string {
  return c === 'foreground' ? cssVar('--fg') : c;
}

/** a color as the core writes it: `foreground`, `#rrggbb` or `#rrggbbaa` */
export const CORE_COLOR = /^(foreground|#[0-9a-f]{6}([0-9a-f]{2})?)$/i;

/** a color as a picker has it: its `#rrggbb` and its alpha, 0 to 255 (the foreground: the theme's, opaque); the color as given, not lightened */
export function splitColor(c: CoreColor): {hex: string; alpha: number} {
  const m = CORE_HEX.exec(givenColor(c));
  return m ? {hex: '#' + m[1].toLowerCase(), alpha: m[2] ? parseInt(m[2], 16) : 255} : {hex: '#000000', alpha: 255};
}

/** a picker's `#rrggbb` and alpha as the core's color text (`#rrggbbaa` only when not opaque) */
export function joinColor(hex: string, alpha: number): CoreColor {
  const a = Math.max(0, Math.min(255, Math.round(alpha)));
  return hex.toLowerCase() + (a === 255 ? '' : a.toString(16).padStart(2, '0'));
}

/** a `#rrggbb`'s WCAG relative luminance, 0 (black) to 1 (white) */
export function relativeLuminance(hex: string): number {
  const [r, g, b] = [1, 3, 5].map(i => parseInt(hex.slice(i, i + 2), 16) / 255)
    .map(v => (v <= 0.03928 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4));
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

/** WCAG's contrast ratio of two `#rrggbb` colors, 1 (none) to 21 */
export function contrastRatio(a: string, b: string): number {
  const x = relativeLuminance(a), y = relativeLuminance(b);
  return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05);
}

/** `#rrggbb` (or `#rrggbbaa`, its alpha kept) lightened in HSL, hue and saturation kept, until its contrast against
    `bg` (`#rrggbb`) reaches CURVE_CONTRAST; a color that already does, or is not a hex color, comes back as it was */
export function lightenForBackground(c: string, bg: string): string {
  const m = CORE_HEX.exec(c);
  if (!m || contrastRatio('#' + m[1], bg) >= CURVE_CONTRAST) return c;
  const [h, s, l0] = toHsl('#' + m[1]);
  let l = l0, out = '#' + m[1].toLowerCase();
  while (contrastRatio(out, bg) < CURVE_CONTRAST && l < 1) {
    l = Math.min(1, l + LIGHTEN_STEP);
    out = cssToHex(`hsl(${h * 360} ${s * 100}% ${l * 100}%)`) ?? out;
  }
  return out + (m[2] ?? '');
}

/** `#rrggbb` as hue (0 to 1), saturation and lightness (0 to 1) */
function toHsl(hex: string): [number, number, number] {
  const [r, g, b] = [1, 3, 5].map(i => parseInt(hex.slice(i, i + 2), 16) / 255);
  const max = Math.max(r, g, b), min = Math.min(r, g, b), l = (max + min) / 2;
  if (max === min) return [0, 0, l];
  const d = max - min, s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
  const h = max === r ? (g - b) / d + (g < b ? 6 : 0) : max === g ? (b - r) / d + 2 : (r - g) / d + 4;
  return [h / 6, s, l];
}

const HEX = /^#([0-9a-f]{3}|[0-9a-f]{6})$/i;
const FUNCTION = /^(rgba?|hsla?)\(\s*([^)]*)\)$/i;
/** the channel and alpha separators CSS allows: commas, spaces, a slash before alpha */
const SEPARATORS = /[\s,/]+/;

const channel = (n: number) => Math.round(Math.min(255, Math.max(0, n))).toString(16).padStart(2, '0');

/** One CSS color as the `#rrggbb` an <input type=color> takes, or null when it is not one this
    parses (a named color: pass `resolve`, the browser's own conversion, which only the page has).
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

/** the browser's own conversion of any CSS color (a name included) to a form `cssToHex` parses;
    null for text it does not accept */
export function browserColor(css: string): string | null {
  const ctx = document.createElement('canvas').getContext('2d');
  if (!ctx) return null;
  /* an unaccepted color leaves fillStyle as it was: two different starting values tell */
  ctx.fillStyle = '#000000'; ctx.fillStyle = css;
  const first = String(ctx.fillStyle);
  ctx.fillStyle = '#ffffff'; ctx.fillStyle = css;
  return first === String(ctx.fillStyle) ? first : null;
}
