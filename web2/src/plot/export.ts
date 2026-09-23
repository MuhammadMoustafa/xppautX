/* Saving what the plot shows: the picture as PNG, the curves as CSV. Both
   are downloads made in the browser (docs/ui-v2.md "Files"). */
import type {PlotModel} from './model';

export function download(name: string, url: string): void {
  const a = document.createElement('a');
  a.href = url;
  a.download = name;
  document.body.appendChild(a);
  a.click();
  a.remove();
}

/** a stored float32 in full: 9 significant digits read back as the same float */
function num(v: number): string {
  return Number.isFinite(v) ? String(Number(v.toPrecision(9))) : String(v);
}

/** one block per curve: row, T (when sent), x, y */
export function curvesCsv(m: PlotModel): string {
  const lines: string[] = [];
  m.curves.forEach((c, k) => {
    if (k) lines.push('');
    lines.push(`# ${c.label}`);
    lines.push(m.t ? 'row,T,x,y' : 'row,x,y');
    for (let i = 0; i < c.xs.length; i++) {
      const row = c.row0 + i;
      const xy = `${num(c.xs[i])},${num(c.ys[i])}`;
      lines.push(m.t ? `${row},${num(m.t[row])},${xy}` : `${row},${xy}`);
    }
  });
  return lines.join('\n') + '\n';
}

export function downloadCsv(name: string, m: PlotModel): void {
  const url = URL.createObjectURL(new Blob([curvesCsv(m)], {type: 'text/csv'}));
  download(name, url);
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
