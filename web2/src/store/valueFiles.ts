/* Load of the values panel's Parameters and Initial conditions
   (GitHub #18), from XPP's own file formats (written by the core,
   core/lunch-new.cpp io_parameter_file/io_ic_file, through the `values`
   command: docs/protocol.md "values"; W66 moved the writing there so
   the page writes no files):

   - parameters: "N   Number params", then one "value  name" line per
     parameter in the model's order (%.16g), then the model file and a
     date; XPP reads the values by position.
   - initial conditions: the -icfile / Initialconds/File format: the
     values alone, one per line, in the model's order.

   Load also takes plain "name value" or "name=value" lines. Pure. */

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
