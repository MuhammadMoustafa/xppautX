/* XPP's eleven curve colours (color_names[] in the core: 0 the foreground,
   then red .. purple), redrawn as a modern palette for each theme. The
   index is what the core sends; the hue family stays what XPP names. Every
   colour has at least 3:1 contrast against its theme's plot background
   (WCAG 1.4.11), which is why the light theme's yellows are dark gold. */

const LIGHT = ['#1f2937', '#d62839', '#e8590c', '#d97706', '#b8860b', '#a88400', '#6b8e00', '#2b9348', '#0c8599',
  '#1c7ed6', '#7048e8'];
const DARK = ['#e5e7eb', '#ff6b6b', '#ff7a45', '#ffa94d', '#fcc419', '#ffe066', '#c0eb75', '#69db7c', '#3bc9db',
  '#4dabf7', '#b197fc'];

export const CURVE_COLORS = {light: LIGHT, dark: DARK} as const;

export function curveColor(index: number, dark: boolean): string {
  const p = dark ? DARK : LIGHT;
  return p[index >= 0 && index < p.length ? index : 0];
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
