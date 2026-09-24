/* AUTO's settings as a file (docs/ui-v2.md T21): the Numerics form's values
   (AutoNum: Ntst, Nmax, NPr, ... SuppBP) and the Axes (the plot type and the
   AutoPlot form: Y-axis, Main Parm, Secnd Parm, Xmin, Ymin, Xmax, Ymax),
   saved from the forms the core asks and answered back into them on Load,
   so a model is set up in one step. The file is small JSON:

     {"xppautX": "auto-settings", "version": 1,
      "numerics": {"Ntst": "15", "Nmax": "200", ...},
      "plot": 2, "axes": {"Y-axis": "V", "Main Parm": "iapp", ...}}

   Values are the forms' text, as typed. Names are the forms' labels
   (without the core's `*1` list markers); a name the file lacks keeps the
   form's value, a name the form lacks is ignored. Pure. */

/** Auto.plot -> its key in the Axes menu ("Plot Type": hI-lo is i, ...) */
export const PLOT_KEYS: Record<number, string> = {0: 'h', 1: 'n', 2: 'i', 3: 'p', 4: 't', 10: 'r', 11: 'a'};

export interface AutoSettings {
  numerics: [string, string][];
  plot: number;
  axes: [string, string][];
}

/** a form field's name without the core's list marker ("*1Y-axis" -> "Y-axis") */
export function plainName(name: string): string {
  return name.replace(/^\*\d/, '').trim();
}

const pairs = (names: string[], values: string[]): [string, string][] =>
  names.map((n, i) => [plainName(n), String(values[i] ?? '')]);

export function formatSettings(num: {names: string[]; values: string[]}, plot: number,
  axes: {names: string[]; values: string[]}): string {
  return `${JSON.stringify({
    xppautX: 'auto-settings', version: 1,
    numerics: Object.fromEntries(pairs(num.names, num.values)),
    plot,
    axes: Object.fromEntries(pairs(axes.names, axes.values)),
  }, null, 1)}\n`;
}

const textPairs = (o: unknown): [string, string][] | null => {
  if (!o || typeof o !== 'object' || Array.isArray(o)) return null;
  const out: [string, string][] = [];
  for (const [k, v] of Object.entries(o as Record<string, unknown>)) {
    if (typeof v !== 'string' && typeof v !== 'number') return null;
    out.push([plainName(k), String(v)]);
  }
  return out;
};

export function parseSettings(text: string): {settings: AutoSettings | null; error: string | null} {
  const bad = (why: string) => ({settings: null, error: `Not an AUTO settings file: ${why}.`});
  let o: Record<string, unknown>;
  try {
    o = JSON.parse(text);
  } catch {
    return bad('it is not JSON');
  }
  if (!o || typeof o !== 'object' || o.xppautX !== 'auto-settings') return bad('it lacks "xppautX": "auto-settings"');
  const numerics = o.numerics === undefined ? [] : textPairs(o.numerics);
  const axes = o.axes === undefined ? [] : textPairs(o.axes);
  if (!numerics || !axes) return bad('"numerics" and "axes" must map names to values');
  const plot = o.plot === undefined ? -1 : Number(o.plot);
  if (!(plot in PLOT_KEYS) && plot !== -1) return bad(`"plot" ${String(o.plot)} is not one of AUTO's plot types`);
  if (!numerics.length && !axes.length) return bad('it holds no values');
  return {settings: {numerics, plot, axes}, error: null};
}

/** the form's values with the file's put in by name (case does not matter) */
export function valuesFor(names: string[], current: string[], saved: [string, string][]): string[] {
  const byName = new Map(saved.map(([n, v]) => [n.toLowerCase(), v]));
  return names.map((n, i) => byName.get(plainName(n).toLowerCase()) ?? String(current[i] ?? ''));
}

/** the AutoPlot form's values (Y-axis, Main Parm, Secnd Parm, Xmin, Ymin,
    Xmax, Ymax) for the axis dialog's change: the names it changes, and the
    view's ranges as AUTO's axes */
export function axesFormValues(current: string[],
  change: {yvar?: string; par1?: string; par2?: string; ranges: {x: {min: number; max: number}; y: {min: number; max: number}}}): string[] {
  const v = current.map(String);
  while (v.length < 7) v.push('');
  if (change.yvar) v[0] = change.yvar;
  if (change.par1) v[1] = change.par1;
  if (change.par2) v[2] = change.par2;
  const r = change.ranges;
  [v[3], v[4], v[5], v[6]] = [r.x.min, r.y.min, r.x.max, r.y.max].map(n => String(n));
  return v;
}
