/* Colour maps for the array plot (docs/ui-v2.md T12): a perceptually
   uniform default (`viridis`, a widely used 8-stop reduction of the
   matplotlib table, linearly interpolated) and XPP's own (`xpp`,
   core/colormap.c's default table, custom_color 0 / C_NORM -- the palette
   the array plot has always drawn, aplotwin.c FIRSTCOLOR..+color_total).
   The core's rfun/gfun/bfun formula is reimplemented here (not its code:
   the core still owns the numbers, `values` and zmin/zmax; this only turns
   a 0..1 fraction of them into the same hue XPP would). Pure: no DOM. */

export type AplotColorMap = 'viridis' | 'xpp';

const VIRIDIS: readonly [number, number, number][] = [
  [68, 1, 84], [70, 50, 126], [54, 92, 141], [39, 127, 142],
  [31, 161, 135], [74, 193, 109], [160, 218, 57], [253, 231, 37],
];

function lerp(a: number, b: number, t: number): number {
  return a + (b - a) * t;
}

function viridis(t: number): [number, number, number] {
  const c = clamp01(t) * (VIRIDIS.length - 1);
  const i = Math.min(VIRIDIS.length - 2, Math.floor(c)), f = c - i;
  const a = VIRIDIS[i], b = VIRIDIS[i + 1];
  return [lerp(a[0], b[0], f), lerp(a[1], b[1], f), lerp(a[2], b[2], f)];
}

/* core/colormap.c rfun/gfun/bfun at x = i/n, C_NORM: r=rfun(1-x,0),
   g=gfun(1-x,0), b=bfun(1-x,0); the per=1 (periodic) branch of rfun is not
   used at custom_color 0, so it is left out here */
function rfun(x: number): number {
  return x > 1 / 3 ? 0 : 3 * 255 * Math.sqrt((0.333334 - x) * (x + 0.33334));
}
function gfun(y: number): number {
  return y > 0.666666 ? 0 : 3 * 255 * Math.sqrt((0.6666667 - y) * y);
}
function bfun(y: number): number {
  return y < 0.333334 ? 0 : 2.79 * 255 * Math.sqrt((1.05 - y) * (y - 0.333333333));
}

function xppMap(t: number): [number, number, number] {
  const x = 1 - clamp01(t);
  return [rfun(x), gfun(x), bfun(x)];
}

function clamp01(t: number): number {
  return t < 0 ? 0 : t > 1 ? 1 : t;
}

function byte(v: number): number {
  return Math.max(0, Math.min(255, Math.round(v)));
}

function hex(r: number, g: number, b: number): string {
  const h = (n: number) => byte(n).toString(16).padStart(2, '0');
  return `#${h(r)}${h(g)}${h(b)}`;
}

/** a cell not on the grid, or whose value is off the stored rows/columns (NaN) */
export const BLANK_COLOR = '#8888884d';

/** `t` in [0,1] (already scaled to zmin..zmax) as "#rrggbb"; NaN/Infinity as BLANK_COLOR */
export function mapColor(map: AplotColorMap, t: number): string {
  if (!Number.isFinite(t)) return BLANK_COLOR;
  const [r, g, b] = map === 'xpp' ? xppMap(t) : viridis(t);
  return hex(r, g, b);
}

/** a cell's colour straight from its value and the event's zmin/zmax (a
    degenerate range, zmax<=zmin, paints every cell the same, like the
    core's own cells do) */
export function cellColor(map: AplotColorMap, value: number, zmin: number, zmax: number): string {
  if (!Number.isFinite(value)) return BLANK_COLOR;
  const t = zmax > zmin ? (value - zmin) / (zmax - zmin) : 0.5;
  return mapColor(map, t);
}

/** `n` evenly spaced stops from zmax (index 0) to zmin, for a CSS
    `linear-gradient(to top, ...)` legend bar */
export function legendStops(map: AplotColorMap, n = 9): string[] {
  return Array.from({length: n}, (_, i) => mapColor(map, 1 - i / (n - 1)));
}
