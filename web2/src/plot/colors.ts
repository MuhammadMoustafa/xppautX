/* XPP's eleven curve colours (color_names[] in the core: 0 the foreground,
   then red .. purple), redrawn as a modern palette for each theme. The
   index is what the core sends; the hue family stays what XPP names. Every
   colour has at least 3:1 contrast against its theme's plot background
   (WCAG 1.4.11), which is why the light theme's yellows are dark gold. */

const LIGHT = ['#1f2937', '#d62839', '#e8590c', '#d97706', '#b8860b', '#a88400', '#6b8e00', '#2b9348', '#0c8599',
  '#1c7ed6', '#7048e8'];
const DARK = ['#e5e7eb', '#ff6b6b', '#ff922b', '#fcc419', '#ffe066', '#c0eb75', '#69db7c', '#3bc9db',
  '#4dabf7', '#748ffc', '#b197fc'];

export const CURVE_COLORS = {light: LIGHT, dark: DARK} as const;

export function curveColor(index: number, dark: boolean): string {
  const p = dark ? DARK : LIGHT;
  return p[index >= 0 && index < p.length ? index : 0];
}
