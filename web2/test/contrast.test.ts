/* docs/ui-v2.md A1: the theme's colours meet WCAG 2.2 AA contrast. Reads
   the tokens from src/theme.css, so a change there is checked here. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {test} from 'node:test';
import {CURVE_COLORS} from '../src/plot/colors';

function luminance(hex: string): number {
  const c = [1, 3, 5].map(i => parseInt(hex.slice(i, i + 2), 16) / 255)
    .map(v => (v <= 0.03928 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4));
  return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
}

export function contrast(a: string, b: string): number {
  const x = luminance(a), y = luminance(b);
  return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05);
}

/* the tokens of :root (light) and :root[data-theme='dark'] */
function tokens(): {light: Record<string, string>; dark: Record<string, string>} {
  const css = fs.readFileSync(path.join(process.cwd(), 'src/theme.css'), 'utf8');
  const block = (re: RegExp) => {
    const body = re.exec(css)![1], out: Record<string, string> = {};
    for (const m of body.matchAll(/(--[\w-]+):\s*(#[0-9a-fA-F]{6})/g)) out[m[1]] = m[2];
    return out;
  };
  const light = block(/:root \{([^}]*)\}/);
  return {light, dark: {...light, ...block(/:root\[data-theme='dark'\] \{([^}]*)\}/)}};
}

const PAIRS: [string, string, number][] = [
  ['--fg', '--bg', 4.5], ['--fg', '--surface', 4.5], ['--fg-muted', '--bg', 4.5], ['--fg-muted', '--surface', 4.5],
  ['--fg-muted', '--surface-2', 4.5], ['--accent-fg', '--accent', 4.5], ['--danger', '--surface', 4.5],
  ['--focus', '--surface', 3], ['--focus', '--bg', 3], ['--field-border', '--surface', 3], ['--field-border', '--bg', 3],
];

for (const theme of ['light', 'dark'] as const) {
  test(`${theme} theme: text 4.5:1, focus ring and field edges 3:1`, () => {
    const t = tokens()[theme];
    for (const [fg, bg, min] of PAIRS) {
      const r = contrast(t[fg], t[bg]);
      assert.ok(r >= min, `${fg} ${t[fg]} on ${bg} ${t[bg]}: ${r.toFixed(2)} < ${min}`);
    }
  });
  test(`${theme} theme: every curve colour 3:1 against the plot`, () => {
    const surface = tokens()[theme]['--surface'];
    for (const c of CURVE_COLORS[theme]) {
      assert.ok(contrast(c, surface) >= 3, `${c} on ${surface}: ${contrast(c, surface).toFixed(2)}`);
    }
  });
}
