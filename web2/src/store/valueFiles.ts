/* Save and Load of the values panel's Parameters and Initial conditions
   (GitHub #18), in XPP's own file formats so a file goes both ways:

   - parameters: File/Write par's format (core/lunch-new.c
     io_parameter_file): "N   Number params", then one "value  name" line
     per parameter in the model's order (%.16g), then the model file and a
     date; XPP reads the values by position.
   - initial conditions: the -icfile / Initialconds/File format
     (io_ic_file): the values alone, one per line, in the model's order.

   Load also takes plain "name value" or "name=value" lines. Pure. */

/** C's %.16g: 16 significant digits, trailing zeros dropped, e-notation outside 1e-4..1e16 */
export function formatG16(v: number): string {
  if (!Number.isFinite(v)) return Number.isNaN(v) ? 'nan' : v > 0 ? 'inf' : '-inf';
  if (v === 0) return Object.is(v, -0) ? '-0' : '0';
  const exp = Math.floor(Math.log10(Math.abs(Number(v.toPrecision(16)))));
  if (exp < -4 || exp >= 16) {
    const [m, e] = v.toExponential(15).split('e');
    const mant = m.includes('.') ? m.replace(/0+$/, '').replace(/\.$/, '') : m;
    const n = Number(e);
    return `${mant}e${n < 0 ? '-' : '+'}${String(Math.abs(n)).padStart(2, '0')}`;
  }
  const f = v.toFixed(Math.max(0, 15 - exp));
  return f.includes('.') ? f.replace(/0+$/, '').replace(/\.$/, '') : f;
}

export function formatParFile(pars: [string, number][], file: string, date: string): string {
  const lines = [`${pars.length}   Number params`, ...pars.map(([n, v]) => `${formatG16(v)}  ${n}`)];
  return `${lines.join('\n')}\n\n\nFile:${file}\n${date}\n`;
}

export function formatIcFile(ics: [string, number][]): string {
  return ics.map(([, v]) => formatG16(v)).join('\n') + '\n';
}

export interface ParsedValues {
  /** [name as the model spells it, value text] */
  values: [string, string][];
  error: string | null;
}

const NUMBER = /^[-+]?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?$/;
const isNumber = (t: string) => NUMBER.test(t);

/** the values of a saved file for `names` (the model's, in order) */
export function parseValuesFile(text: string, names: string[]): ParsedValues {
  const lines = text.split(/\r?\n/);
  const byName = new Map(names.map(n => [n.toLowerCase(), n]));
  const fail = (error: string): ParsedValues => ({values: [], error});
  /* XPP's parameter file: a count, then that many "value  name" lines, by position */
  const head = /^\s*(\d+)\s+Number params/i.exec(lines[0] ?? '');
  if (head) {
    const n = Number(head[1]);
    if (n !== names.length) return fail(`The file has ${n} parameters, the model ${names.length}`);
    const values: [string, string][] = [];
    for (let i = 0; i < n; i++) {
      const [v] = (lines[1 + i] ?? '').trim().split(/\s+/);
      if (!v || !isNumber(v)) return fail(`Line ${i + 2} is not a number: ${lines[1 + i] ?? ''}`);
      values.push([names[i], v]);
    }
    return {values, error: null};
  }
  const rows = lines.map(l => l.trim()).filter(l => l && !l.startsWith('#'));
  /* the IC file: numbers alone, by position */
  if (rows.length && rows.every(r => r.split(/\s+/).every(isNumber))) {
    const nums = rows.flatMap(r => r.split(/\s+/));
    if (nums.length !== names.length) return fail(`The file has ${nums.length} values, the model ${names.length}`);
    return {values: nums.map((v, i) => [names[i], v]), error: null};
  }
  /* name value, name=value, or value name */
  const values: [string, string][] = [];
  const unknown: string[] = [];
  for (const r of rows) {
    const t = r.split(/\s*=\s*|\s+/).filter(Boolean);
    if (t.length < 2) return fail(`Not "name value": ${r}`);
    const [name, v] = isNumber(t[1]) && !isNumber(t[0]) ? [t[0], t[1]] : isNumber(t[0]) ? [t[1], t[0]] : ['', ''];
    if (!v) return fail(`Not "name value": ${r}`);
    const known = byName.get(name.toLowerCase());
    if (!known) unknown.push(name);
    else values.push([known, v]);
  }
  if (unknown.length) return fail(`Not in the model: ${unknown.join(', ')}`);
  if (!values.length) return fail('No values in the file');
  return {values, error: null};
}
