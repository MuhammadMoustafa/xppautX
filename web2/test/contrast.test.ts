/* docs/ui-v2.md A1: the theme's colors meet WCAG 2.2 AA contrast. Reads
   the tokens from src/css/tokens.css, so a change there is checked here. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {test} from 'node:test';
import {contrastRatio as contrast} from '../src/plot/colors';

/* the tokens of :root (light) and :root[data-theme='dark'] */
function tokens(): {light: Record<string, string>; dark: Record<string, string>} {
  const css = fs.readFileSync(path.join(process.cwd(), 'src/css/tokens.css'), 'utf8');
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
  test(`${theme} theme: the foreground (--fg) of curves and text 3:1 against the plot`, () => {
    const t = tokens()[theme], c = t['--fg'], surface = t['--surface'];
    assert.ok(contrast(c, surface) >= 3, `${c} on ${surface}: ${contrast(c, surface).toFixed(2)}`);
  });
}
